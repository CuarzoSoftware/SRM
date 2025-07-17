#include <CZ/SRM/SRMConnectorMode.h>
#include <CZ/SRM/SRMDevice.h>
#include <CZ/SRM/SRMLog.h>
#include <CZ/SRM/SRMRenderer.h>
#include <CZ/SRM/SRMConnector.h>
#include <CZ/SRM/SRMPlane.h>
#include <CZ/SRM/SRMCrtc.h>
#include <CZ/SRM/SRMEncoder.h>
#include <CZ/SRM/SRMCore.h>

#include <CZ/Ream/RImage.h>
#include <CZ/Ream/RSurface.h>
#include <CZ/Ream/RSync.h>
#include <CZ/Ream/DRM/RDRMFramebuffer.h>

#include <future>

#include <drm_fourcc.h>
#include <sys/poll.h>

using namespace CZ;

std::unique_ptr<SRMRenderer> SRMRenderer::Make(SRMConnector *conn, const SRMConnectorInterface *iface, void *ifaceData) noexcept
{
    if (!iface || !iface->initializeGL || !iface->uninitializeGL || !iface->pageFlipped || !iface->paintGL || !iface->resizeGL)
    {
        conn->log(CZError, CZLN, "Invalid SRMConnectorInterface");
        return {};
    }

    auto rend { std::make_unique<SRMRenderer>(conn, iface, ifaceData) };

    SRMEncoder *bestEncoder;
    SRMCrtc *bestCrtc;
    SRMPlane *bestPrimaryPlane;
    SRMPlane *bestCursorPlane;

    if (!conn->findConfiguration(&bestEncoder, &bestCrtc, &bestPrimaryPlane, &bestCursorPlane))
    {
        conn->log(CZError, "Could not find a encoder, crtc or primary plane");
        return {};
    }

    rend->encoder = bestEncoder;
    rend->crtc = bestCrtc;
    rend->primaryPlane = bestPrimaryPlane;

    bestEncoder->m_currentConnector = conn;
    bestCrtc->m_currentConnector = conn;
    bestPrimaryPlane->m_currentConnector = conn;

    // When using legacy cursor IOCTLs, the plane is choosen by the driver
    if (!conn->device()->core()->m_pf.has(SRMCore::pForceLegacyCursor) && conn->device()->clientCaps().Atomic)
    {
        rend->cursorPlane = bestCursorPlane;

        if (bestCursorPlane)
            bestCursorPlane->m_currentConnector = conn;
    }

    rend->initGamma();
    return rend;
}

SRMRenderer::SRMRenderer(SRMConnector *connector, const SRMConnectorInterface *iface, void *ifaceData) noexcept :
    conn(connector),
    iface(iface),
    ifaceData(ifaceData)
{
    log = conn->log.newWithContext("REND");
    logAtomic = log.newWithContext("ATOMIC");
    logLegacy = log.newWithContext("LEGACY");
    conn->setState(SRMConnector::Initializing);
}

SRMRenderer::~SRMRenderer() noexcept
{
    //     srmConnectorRenderThreadCleanUp(connector);

    if (encoder)
        encoder->m_currentConnector = nullptr;

    if (crtc)
        crtc->m_currentConnector = nullptr;

    if (primaryPlane)
        primaryPlane->m_currentConnector = nullptr;

    if (cursorPlane)
        cursorPlane->m_currentConnector = nullptr;

    conn->setState(SRMConnector::Uninitialized);
}

void SRMRenderer::initGamma() noexcept
{
    const UInt64 gammaSize { crtc->gammaSize() };

    if (gammaSize > 0)
    {
        log(CZInfo, "Gamma LUT Size: {}", gammaSize);

        m_gamma.resize(gammaSize);

        /* Apply linear gamma */

        Float64 n = gammaSize - 1.0;
        Float64 val;

        if (device()->clientCaps().Atomic)
        {
            for (UInt32 i = 0; i < gammaSize; i++)
            {
                val = (Float64)i / n;
                m_gamma[i].red = m_gamma[i].green = m_gamma[i].blue = (UInt16)(UINT16_MAX * val);
            }

            atomicChanges.add(CHGammaLUT);
        }
        else
        {
            UInt16 *r = (UInt16*)m_gamma.data();
            UInt16 *g = r + gammaSize;
            UInt16 *b = g + gammaSize;

            for (UInt32 i = 0; i < gammaSize; i++)
            {
                val = (Float64)i / n;
                r[i] = g[i] = b[i] = (UInt16)(UINT16_MAX * val);
            }

            if (drmModeCrtcSetGamma(device()->fd(),
                                    crtc->id(),
                                    (UInt32)gammaSize,
                                    r,
                                    g,
                                    b))
            {
                logLegacy(CZError, CZLN, "Failed to set gamma LUT");
            }
        }
    }
    else
    {
        log(CZInfo, "Gamma LUT not supported");
    }
}

static void drmPageFlipHandler(Int32 fd, UInt32 seq, UInt32 sec, UInt32 usec, void *data)
{
    CZ_UNUSED(fd);

    if (data)
    {
        SRMRenderer *rend { static_cast<SRMRenderer*>(data) };
        rend->pendingPageFlip = false;

        if (rend->conn->state() == SRMConnector::Initialized)
            return;

        if (rend->currentVSync)
        {
            rend->presentationTime.flags.set(RPresentationTime::HWClock | RPresentationTime::HWCompletion | RPresentationTime::VSync);
            rend->presentationTime.frame = seq;
            rend->presentationTime.time.tv_sec = sec;
            rend->presentationTime.time.tv_nsec = usec * 1000;
            rend->presentationTime.period = rend->conn->currentMode()->info().vrefresh == 0 ? 0 : 1000000000/rend->conn->currentMode()->info().vrefresh;
        }
        else
        {
            rend->presentationTime.flags = 0;
            rend->presentationTime.frame = 0;
            rend->presentationTime.period = 0;

            Int64 prevUsec = (rend->presentationTime.time.tv_sec * 1000000LL) + (rend->presentationTime.time.tv_nsec / 1000LL);
            clock_gettime(rend->device()->caps().TimestampMonotonic ? CLOCK_MONOTONIC : CLOCK_REALTIME, &rend->presentationTime.time);

            if (rend->tearingLimit < 0)
                return;

            Int64 currUsec = (rend->presentationTime.time.tv_sec * 1000000LL) + (rend->presentationTime.time.tv_nsec / 1000LL);
            Int64 periodUsec;

            // Limit FPS to 2 * vrefresh
            if (rend->tearingLimit == 0)
                periodUsec = (rend->conn->currentMode()->info().vrefresh == 0 ? 0 : 490000/(rend->conn->currentMode()->info().vrefresh));
            else
                periodUsec = (rend->conn->currentMode()->info().vrefresh == 0 ? 0 : 1000000/(rend->tearingLimit));

            if (periodUsec > 0)
            {
                Int64 diffUsec = currUsec - prevUsec;

                if (diffUsec >= 0 && diffUsec < periodUsec)
                {
                    usleep(periodUsec - diffUsec);
                    clock_gettime(rend->device()->caps().TimestampMonotonic ? CLOCK_MONOTONIC : CLOCK_REALTIME, &rend->presentationTime.time);
                }
            }
        }
    }
}

bool SRMRenderer::initRenderThread() noexcept
{
    std::promise<bool> initPromise;
    std::future<bool> initFuture = initPromise.get_future();

    std::thread([this](std::promise<bool> initPromise)
    {
        drmEventCtx.version = DRM_EVENT_CONTEXT_VERSION,
        drmEventCtx.vblank_handler = NULL,
        drmEventCtx.page_flip_handler = &drmPageFlipHandler,
        drmEventCtx.page_flip_handler2 = NULL;
        drmEventCtx.sequence_handler = NULL;

        //srmRenderModeCommonCreateCursor(connector);

        if (!rendInit())
        {
            log(CZError, CZLN, "Failed to initialize renderer");
            initPromise.set_value(false);
            return;
        }

        initPromise.set_value(true);

        // Render loop
        while (true)
        {
            currentVSync = pendingVSync;

            if (!rendWaitForRepaint())
                return;

            const std::lock_guard<std::recursive_mutex> lockState { conn->m_stateMutex };
            const auto state { conn->state() };

            if (state == SRMConnector::Initialized)
            {
                if (pendingRepaint)
                {
                    pendingRepaint = false;

                    rendering = true;
                    rendRender();
                    rendering = false;
                    rendFlipPage();
                    updateAgeAndIndex();
                    continue;
                }
                else if (atomicChanges)
                {
                    presentFrame(lastFb);
                }
            }

            if (state == SRMConnector::ChangingMode)
            {
                imageAge = 0;

                if (rendUpdateMode())
                {
                    conn->setState(SRMConnector::Initialized);
                    log(CZInfo, "Mode change applied");
                    continue;
                }
                else
                {
                    log(CZError, "Mode change failed");
                    conn->setState(SRMConnector::RevertingMode);
                    continue;
                }
            }
            else if (state == SRMConnector::Suspending)
            {
                conn->setState(SRMConnector::Suspended);
                rendSuspend();
                usleep(1000);
                continue;
            }
            else if (state == SRMConnector::Resuming)
            {
                conn->setState(SRMConnector::Initialized);
                imageAge = 0;
                rendResume();
                usleep(1000);
                continue;
            }

        }

    }, std::move(initPromise)).detach();

    try {
        return initFuture.get();
    }
    catch (...) {
        return false;
    }
}

bool SRMRenderer::rendInit() noexcept
{
    return rendInitImages() && rendInitCrtc();
}

bool SRMRenderer::rendInitImages() noexcept
{
    images.resize(3);
    fbs.resize(images.size());
    surfaces.resize(images.size());

    const RDRMFormat format { DRM_FORMAT_XRGB8888, { DRM_FORMAT_MOD_LINEAR } };

    for (size_t i = 0; i < images.size(); i++)
    {
        images[i] = RImage::Make(conn->currentMode()->size(), format, RStorageType::GBM, device()->reamDevice());

        if (!images[i])
        {
            log(CZError, CZLN, "Failed to create swapchain RImage {}", i);
            return false;
        }

        fbs[i] = images[i]->drmFb(device()->reamDevice());

        if (!fbs[i])
        {
            log(CZError, CZLN, "Failed to get RDRMFramebuffer from swapchain RImage {}", i);
            return false;
        }

        surfaces[i] = RSurface::WrapImage(images[i], 1);

        if (!surfaces[i])
        {
            log(CZError, CZLN, "Failed to create RSurface from swapchain RImage {}", i);
            return false;
        }
    }

    return true;
}

bool SRMRenderer::rendInitCrtc() noexcept
{
    Int32 ret;

    if (device()->clientCaps().Atomic)
    {
        drmModeAtomicReqPtr req;
        req = drmModeAtomicAlloc();

        // Plane

        drmModeAtomicAddProperty(req,
                                 primaryPlane->id(),
                                 primaryPlane->m_propIDs.FB_ID,
                                 0);

        drmModeAtomicAddProperty(req,
                                 primaryPlane->id(),
                                 primaryPlane->m_propIDs.CRTC_ID,
                                 0);

        // CRTC

        if (conn->m_currentModeBlobId)
        {
            drmModeDestroyPropertyBlob(device()->fd(), conn->m_currentModeBlobId);
            conn->m_currentModeBlobId = 0;
        }

        drmModeCreatePropertyBlob(device()->fd(),
                                  &conn->currentMode()->info(),
                                  sizeof(drmModeModeInfo),
                                  &conn->m_currentModeBlobId);

        drmModeAtomicAddProperty(req,
                                 crtc->id(),
                                 crtc->m_propIDs.MODE_ID,
                                 conn->m_currentModeBlobId);

        drmModeAtomicAddProperty(req,
                                 crtc->id(),
                                 crtc->m_propIDs.ACTIVE,
                                 1);
        // Connector

        drmModeAtomicAddProperty(req,
                                 conn->id(),
                                 conn->m_propIDs.CRTC_ID,
                                 crtc->id());

        drmModeAtomicAddProperty(req,
                                 conn->id(),
                                 conn->m_propIDs.link_status,
                                 DRM_MODE_LINK_STATUS_GOOD);


        auto prevCursorIndex { cursorI };
        rendAppendAtomicChanges(req, false);
        ret = rendAtomicCommit(req, DRM_MODE_ATOMIC_ALLOW_MODESET, this, true);
        drmModeAtomicFree(req);

        if (ret)
        {
            if (cursorPlane)
                cursorI = prevCursorIndex;
            logAtomic(CZError, CZLN, "Failed to set CRTC mode. DRM Error: {}", strerror(ret));
        }
        else
        {
            atomicChanges = 0;
            goto skipLegacy;
        }
    }

    /* Occasionally, the Atomic API fails to set the connector CRTC for reasons unknown.
     * As a workaround, we enforce the use of the legacy API in this case. */

    ret = drmModeSetCrtc(device()->fd(),
                         crtc->id(),
                         0,
                         0,
                         0,
                         &conn->m_id,
                         1,
                         &conn->currentMode()->m_info);

    if (ret)
    {
        logLegacy(CZError, CZLN, "Failed to set CRTC mode. DRM Error: {}", ret);
        return false;
    }

skipLegacy:

    if (conn->state() == SRMConnector::Initializing)
    {
        iface->initializeGL(conn, ifaceData);
        conn->setState(SRMConnector::Initialized);
    }
    else if (conn->state() == SRMConnector::ChangingMode)
    {
        iface->resizeGL(conn, ifaceData);
    }

    return true;
}

bool SRMRenderer::rendWaitForRepaint() noexcept
{
    const bool needsWait { (!pendingRepaint && atomicChanges == 0) || device()->core()->isSuspended() };

    if (needsWait)
    {
        atomicChanges.set(0);
        semaphore.acquire();
    }

    std::unique_lock<std::recursive_mutex> stateLock { conn->m_stateMutex };

    if (conn->state() == SRMConnector::Uninitializing)
    {
        stateLock.unlock();
        pendingPageFlip = true;
        rendWaitPageFlip(3);
        iface->uninitializeGL(conn, ifaceData);
        rendUnit();
        return false;
    }

    return true;
}

bool SRMRenderer::rendWaitPageFlip(int iterLimit) noexcept
{
    pollfd fds {};
    fds.fd = device()->fd();
    fds.events = POLLIN;

    while (pendingPageFlip)
    {
        if (conn->state() != SRMConnector::Initialized || iterLimit == 0)
            return false;

        const std::lock_guard<std::mutex> lock { device()->m_pageFlipMutex };

        // Double check if the pageflip was notified in another thread
        if (!pendingPageFlip)
            return true;

        poll(&fds, 1, iterLimit == -1 ? 500 : 1);
        drmHandleEvent(fds.fd, &drmEventCtx);

        if (iterLimit > 0)
            iterLimit--;
    }

    return true;
}

void SRMRenderer::presentFrame(UInt32 fb) noexcept
{
    int ret { 0 };
    const auto buffersCount { images.size() };

    if (pendingPageFlip || buffersCount == 1 || buffersCount > 2)
        rendWaitPageFlip(-1);

    lastFb = fb;

    if (device()->clientCaps().Atomic)
    {
        const std::lock_guard<std::mutex> lock { propsMutex };

        if (currentVSync)
        {
            drmModeAtomicReqPtr req;
            req = drmModeAtomicAlloc();

            const auto prevCursorIndex { cursorI };
            rendAppendAtomicChanges(req, false);
            atomicAppendPlane(req);
            ret = rendAtomicCommit(req, DRM_MODE_PAGE_FLIP_EVENT | DRM_MODE_ATOMIC_NONBLOCK, this, false);

            if (ret && cursorPlane)
                cursorI = prevCursorIndex;
            else
                atomicChanges = 0;

            drmModeAtomicFree(req);
            pendingPageFlip = true;
        }
        else
        {
            if (atomicChanges == 0)
            {
                drmModeAtomicReqPtr req;
                req = drmModeAtomicAlloc();
                atomicAppendPlane(req);
                ret = rendAtomicCommit(req, DRM_MODE_PAGE_FLIP_EVENT | DRM_MODE_PAGE_FLIP_ASYNC | DRM_MODE_ATOMIC_NONBLOCK, this, false);
                drmModeAtomicFree(req);
            }

            if (atomicChanges || ret == -22)
            {
                drmModeAtomicReqPtr req;
                req = drmModeAtomicAlloc();

                const auto prevCursorIndex { cursorI };
                rendAppendAtomicChanges(req, false);
                atomicAppendPlane(req);
                ret = rendAtomicCommit(req, DRM_MODE_PAGE_FLIP_EVENT | DRM_MODE_ATOMIC_NONBLOCK, this, false);
                drmModeAtomicFree(req);

                if (ret && cursorPlane)
                    cursorI = prevCursorIndex;
                else
                    atomicChanges = 0;
            }

            pendingPageFlip = true;
        }

        if (ret)
            logAtomic(CZError, CZLN, "Failed to page flip. DRM Error: {}", strerror(ret));
    }
    else
    {
        if (currentVSync)
            ret = drmModePageFlip(device()->fd(), crtc->id(), lastFb, DRM_MODE_PAGE_FLIP_EVENT, this);
        else
        {
            ret = drmModePageFlip(device()->fd(), crtc->id(), lastFb, DRM_MODE_PAGE_FLIP_ASYNC | DRM_MODE_PAGE_FLIP_EVENT, this);

            if (ret == -22)
                ret = drmModePageFlip(device()->fd(), crtc->id(), lastFb, DRM_MODE_PAGE_FLIP_EVENT, this);
        }

        pendingPageFlip = true;

        if (ret)
            logLegacy(CZError, CZLN, "Failed to page flip. DRM Error: {}", strerror(ret));
    }


    if (ret)
    {
        pendingPageFlip = false;

        /*
        if (customScanoutBuffer && ret == -22)
        {
            if (!srmFormatIsInList(
                    connector->currentPrimaryPlane->inFormatsBlacklist,
                    connector->userScanoutBufferRef[0]->scanout.fmt.format,
                    connector->userScanoutBufferRef[0]->scanout.fmt.modifier))
                srmFormatsListAddFormat(
                    connector->currentPrimaryPlane->inFormatsBlacklist,
                    connector->userScanoutBufferRef[0]->scanout.fmt.format,
                    connector->userScanoutBufferRef[0]->scanout.fmt.modifier);
        }*/
    }

    if (/*customScanoutBuffer ||*/ buffersCount == 2 || firstPageFlip)
    {
        firstPageFlip = false;
        rendWaitPageFlip(-1);
    }
}

bool SRMRenderer::rendRender() noexcept
{
    iface->paintGL(conn, ifaceData);
    return false;
}

bool SRMRenderer::rendFlipPage() noexcept
{
    if (images[imageI]->writeSync() && images[imageI]->writeSync()->gpuWait())
    {
        //printf("w");
    }
    presentFrame(fbs[imageI]->id());
    iface->pageFlipped(conn, ifaceData);
    return true;
}

bool SRMRenderer::rendUpdateMode() noexcept
{
    return false;
}

bool SRMRenderer::rendSuspend() noexcept
{
    return false;
}

bool SRMRenderer::rendResume() noexcept
{
    return false;
}

void SRMRenderer::rendUnit() noexcept
{

}

void SRMRenderer::updateAgeAndIndex() noexcept
{
    log(CZTrace, "Presented image index: {} age: {} fb: {}", imageI, imageAge, fbs[imageI]->id());
    if (++imageI == images.size())
        imageI = 0;

    if (imageAge < images.size())
        imageAge++;
}

int SRMRenderer::rendAtomicCommit(drmModeAtomicReqPtr req, UInt32 flags, void *data, bool forceRetry) noexcept
{
    if (!forceRetry)
        return drmModeAtomicCommit(device()->fd(), req, flags, data);

    int ret;
retry:
    ret = drmModeAtomicCommit(device()->fd(), req, flags | DRM_MODE_ATOMIC_TEST_ONLY, data);

    if (ret == -16) // -EBUSY
    {
        usleep(2000);
        goto retry;
    }

    return drmModeAtomicCommit(device()->fd(), req, flags, data);
}

void SRMRenderer::rendAppendAtomicChanges(drmModeAtomicReqPtr req, bool clearFlags) noexcept
{
    /*
    if (connector->currentCursorPlane)
    {
        UInt8 updatedFB = 0;

        if (connector->atomicChanges & SRM_ATOMIC_CHANGE_CURSOR_BUFFER)
        {
            if (clearFlags)
                connector->atomicChanges &= ~SRM_ATOMIC_CHANGE_CURSOR_BUFFER;

            connector->cursorIndex = 1 - connector->cursorIndex;

            if (connector->cursorVisible)
            {
                drmModeAtomicAddProperty(req,
                                         connector->currentCursorPlane->id,
                                         connector->currentCursorPlane->propIDs.FB_ID,
                                         connector->cursor[connector->cursorIndex].fb);
                updatedFB = 1;
            }
        }

        UInt8 updatedVisibility = 0;

        if (connector->atomicChanges & SRM_ATOMIC_CHANGE_CURSOR_VISIBILITY)
        {
            if (clearFlags)
                connector->atomicChanges &= ~SRM_ATOMIC_CHANGE_CURSOR_VISIBILITY;

            if (connector->cursorVisible)
            {
                updatedVisibility = 1;

                if (!updatedFB)
                {
                    drmModeAtomicAddProperty(req,
                                             connector->currentCursorPlane->id,
                                             connector->currentCursorPlane->propIDs.FB_ID,
                                             connector->cursor[connector->cursorIndex].fb);
                }

                drmModeAtomicAddProperty(req,
                                         connector->currentCursorPlane->id,
                                         connector->currentCursorPlane->propIDs.CRTC_ID,
                                         connector->currentCrtc->id);

                drmModeAtomicAddProperty(req,
                                         connector->currentCursorPlane->id,
                                         connector->currentCursorPlane->propIDs.CRTC_X,
                                         connector->cursorX);

                drmModeAtomicAddProperty(req,
                                         connector->currentCursorPlane->id,
                                         connector->currentCursorPlane->propIDs.CRTC_Y,
                                         connector->cursorY);

                drmModeAtomicAddProperty(req,
                                         connector->currentCursorPlane->id,
                                         connector->currentCursorPlane->propIDs.CRTC_W,
                                         64);

                drmModeAtomicAddProperty(req,
                                         connector->currentCursorPlane->id,
                                         connector->currentCursorPlane->propIDs.CRTC_H,
                                         64);

                drmModeAtomicAddProperty(req,
                                         connector->currentCursorPlane->id,
                                         connector->currentCursorPlane->propIDs.SRC_X,
                                         0);

                drmModeAtomicAddProperty(req,
                                         connector->currentCursorPlane->id,
                                         connector->currentCursorPlane->propIDs.SRC_Y,
                                         0);

                drmModeAtomicAddProperty(req,
                                         connector->currentCursorPlane->id,
                                         connector->currentCursorPlane->propIDs.SRC_W,
                                         (UInt64)64 << 16);

                drmModeAtomicAddProperty(req,
                                         connector->currentCursorPlane->id,
                                         connector->currentCursorPlane->propIDs.SRC_H,
                                         (UInt64)64 << 16);
            }
            else
            {
                drmModeAtomicAddProperty(req,
                                         connector->currentCursorPlane->id,
                                         connector->currentCursorPlane->propIDs.CRTC_ID,
                                         0);

                drmModeAtomicAddProperty(req,
                                         connector->currentCursorPlane->id,
                                         connector->currentCursorPlane->propIDs.FB_ID,
                                         0);
            }
        }

        if (connector->atomicChanges & SRM_ATOMIC_CHANGE_CURSOR_POSITION)
        {
            if (clearFlags)
                connector->atomicChanges &= ~SRM_ATOMIC_CHANGE_CURSOR_POSITION;

            if (!updatedVisibility)
            {
                drmModeAtomicAddProperty(req,
                                         connector->currentCursorPlane->id,
                                         connector->currentCursorPlane->propIDs.CRTC_X,
                                         connector->cursorX);

                drmModeAtomicAddProperty(req,
                                         connector->currentCursorPlane->id,
                                         connector->currentCursorPlane->propIDs.CRTC_Y,
                                         connector->cursorY);
            }
        }
    }*/

    /*
    if (connector->atomicChanges & SRM_ATOMIC_CHANGE_GAMMA_LUT)
    {
        if (clearFlags)
            connector->atomicChanges &= ~SRM_ATOMIC_CHANGE_GAMMA_LUT;

        if (connector->gammaBlobId)
        {
            drmModeDestroyPropertyBlob(connector->device->fd, connector->gammaBlobId);
            connector->gammaBlobId = 0;
        }

        if (drmModeCreatePropertyBlob(connector->device->fd,
                                      connector->gamma,
                                      srmCrtcGetGammaSize(connector->currentCrtc) * sizeof(*connector->gamma),
                                      &connector->gammaBlobId))
        {
            connector->gammaBlobId = 0;
            SRMError("[%s] [%s] Failed to create gamma lut blob.", connector->device->shortName, connector->name);
        }
        else
        {
            drmModeAtomicAddProperty(req,
                                     connector->currentCrtc->id,
                                     connector->currentCrtc->propIDs.GAMMA_LUT,
                                     connector->gammaBlobId);
        }
    }

    if (connector->atomicChanges & SRM_ATOMIC_CHANGE_CONTENT_TYPE)
    {
        if (clearFlags)
            connector->atomicChanges &= ~SRM_ATOMIC_CHANGE_CONTENT_TYPE;

        if (connector->propIDs.content_type)
            drmModeAtomicAddProperty(req,
                                     connector->id,
                                     connector->propIDs.content_type,
                                     connector->contentType);
    }

    drmModeAtomicAddProperty(req,
                             connector->currentPrimaryPlane->id,
                             connector->currentPrimaryPlane->propIDs.IN_FENCE_FD,
                             connector->fenceFD); */
}

void SRMRenderer::atomicAppendPlane(drmModeAtomicReqPtr req) noexcept
{
    drmModeAtomicAddProperty(req,
                             primaryPlane->id(),
                             primaryPlane->m_propIDs.FB_ID,
                             fbs[imageI]->id());

    drmModeAtomicAddProperty(req,
                             primaryPlane->id(),
                             primaryPlane->m_propIDs.CRTC_ID,
                             crtc->id());

    drmModeAtomicAddProperty(req,
                             primaryPlane->id(),
                             primaryPlane->m_propIDs.CRTC_X,
                             0);

    drmModeAtomicAddProperty(req,
                             primaryPlane->id(),
                             primaryPlane->m_propIDs.CRTC_Y,
                             0);

    drmModeAtomicAddProperty(req,
                             primaryPlane->id(),
                             primaryPlane->m_propIDs.CRTC_W,
                             conn->currentMode()->info().hdisplay);

    drmModeAtomicAddProperty(req,
                             primaryPlane->id(),
                             primaryPlane->m_propIDs.CRTC_H,
                             conn->currentMode()->info().vdisplay);

    drmModeAtomicAddProperty(req,
                             primaryPlane->id(),
                             primaryPlane->m_propIDs.SRC_X,
                             0);

    drmModeAtomicAddProperty(req,
                             primaryPlane->id(),
                             primaryPlane->m_propIDs.SRC_Y,
                             0);

    drmModeAtomicAddProperty(req,
                             primaryPlane->id(),
                             primaryPlane->m_propIDs.SRC_W,
                             (UInt64)conn->currentMode()->info().hdisplay << 16);

    drmModeAtomicAddProperty(req,
                             primaryPlane->id(),
                             primaryPlane->m_propIDs.SRC_H,
                             (UInt64)conn->currentMode()->info().vdisplay << 16);
}

SRMDevice *SRMRenderer::device() const noexcept
{
    return conn->device();
}
