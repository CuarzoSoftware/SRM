#include <CZ/SRM/SRMConnectorMode.h>
#include <CZ/SRM/SRMDevice.h>
#include <CZ/SRM/SRMLog.h>
#include <CZ/SRM/SRMRenderer.h>
#include <CZ/SRM/SRMConnector.h>
#include <CZ/SRM/SRMPlane.h>
#include <CZ/SRM/SRMCrtc.h>
#include <CZ/SRM/SRMEncoder.h>
#include <CZ/SRM/SRMCore.h>
#include <CZ/SRM/SRMAtomicRequest.h>

#include <CZ/Ream/RImage.h>
#include <CZ/Ream/RSurface.h>
#include <CZ/Ream/RSync.h>
#include <CZ/Ream/RCore.h>
#include <CZ/Ream/RPass.h>
#include <CZ/Ream/RPainter.h>
#include <CZ/Ream/DRM/RDRMFramebuffer.h>
#include <CZ/Ream/DRM/RDumbBuffer.h>
#include <CZ/Ream/GBM/RGBMBo.h>

#include <CZ/Ream/GL/RGLMakeCurrent.h>

#include <GLES2/gl2.h>
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
    if (!conn->device()->core()->m_forceLegacyCursor && conn->device()->clientCaps().Atomic)
    {
        rend->cursorPlane = bestCursorPlane;

        if (bestCursorPlane)
            bestCursorPlane->m_currentConnector = conn;
    }

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
}

SRMRenderer::~SRMRenderer() noexcept
{
    if (encoder)
        encoder->m_currentConnector = nullptr;

    if (crtc)
        crtc->m_currentConnector = nullptr;

    if (primaryPlane)
        primaryPlane->m_currentConnector = nullptr;

    if (cursorPlane)
        cursorPlane->m_currentConnector = nullptr;
}

void SRMRenderer::initContentType() noexcept
{
    conn->setContentType(conn->contentType(), true);
}

void SRMRenderer::initGamma() noexcept
{
    conn->setGammaLUT(nullptr);
}

void SRMRenderer::initCursor() noexcept
{
    if (device()->core()->m_disableCursor)
        return;

    const bool atomic = device()->clientCaps().Atomic &&
                   !device()->core()->m_forceLegacyCursor &&
                   cursorPlane &&
                   (cursorPlane->formats().has(DRM_FORMAT_ARGB8888, DRM_FORMAT_MOD_LINEAR) ||
                    cursorPlane->formats().has(DRM_FORMAT_ARGB8888, DRM_FORMAT_MOD_INVALID));

    RImageConstraints consts {};
    consts.allocator = device()->reamDevice();
    consts.writeFormats.emplace(DRM_FORMAT_ARGB8888);
    consts.caps[device()->reamDevice()] = RImageCap_GBMBo;

    if (atomic)
        consts.caps[device()->reamDevice()].add(RImageCap_DRMFb);

    for (size_t i = 0; i < 2; i++)
    {
        cursor[i].bo = RGBMBo::MakeCursor({ 64, 64 }, DRM_FORMAT_ARGB8888, device()->reamDevice());

        if (!cursor[i].bo)
            goto fail;

        if (atomic)
        {
            cursor[i].fb = RDRMFramebuffer::MakeFromGBMBo(cursor[i].bo);

            if (!cursor[i].fb)
                goto fail;
        }
    }

    cursorAPI = atomic ? CursorAPI::Atomic : CursorAPI::Legacy;
    return;

fail:
    for (size_t i = 0; i < 2; i++)
    {
        cursor[i].bo.reset();
        cursor[i].fb.reset();
    }
    cursorAPI = CursorAPI::None;
}

static void drmPageFlipHandler(Int32 fd, UInt32 seq, UInt32 sec, UInt32 usec, void *data)
{
    CZ_UNUSED(fd);

    if (data)
    {
        SRMRenderer *rend { static_cast<SRMRenderer*>(data) };
        rend->pendingPageFlip = false;

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

bool SRMRenderer::startRenderThread() noexcept
{
    std::promise<bool> initPromise;
    std::future<bool> initFuture = initPromise.get_future();

    std::thread([this](std::promise<bool> initPromise)
    {
        threadId = std::this_thread::get_id();
        drmEventCtx.version = DRM_EVENT_CONTEXT_VERSION,
        drmEventCtx.vblank_handler = NULL,
        drmEventCtx.page_flip_handler = &drmPageFlipHandler,
        drmEventCtx.page_flip_handler2 = NULL;
        drmEventCtx.sequence_handler = NULL;

        if (!init())
        {
            log(CZError, CZLN, "Failed to initialize renderer");
            initPromise.set_value(false);
            return;
        }

        logInfo();

        initPromise.set_value(true);

        // Render loop
        while (true)
        {
            // Blocks until repaint or unlock is called
            waitForRepaintRequest();

            currentVSync = conn->m_vsync;

            // Uninitialize
            if (unitPromise.has_value())
            {
                unit();
                return;
            }

            // E.g. if physically unplugged
            if (isDead)
            {
                pendingRepaint = 0;
                atomicChanges = 0;
                continue;
            }

            // Set mode
            if (pendingMode)
            {
                auto modeBackup { conn->m_currentMode };
                conn->m_currentMode = pendingMode;
                pendingMode.reset();
                assert(conn->m_currentMode);

                if (applyCrtcMode())
                {
                    iface->resizeGL(conn, ifaceData);
                    setModePromise.set_value(1); // Ok
                }
                else
                {
                    conn->m_currentMode = modeBackup;
                    if (applyCrtcMode())
                        setModePromise.set_value(0); // Rollback
                    else
                    {
                        log(CZFatal, CZLN, "Failed to apply mode and rollback to previous one (the connector was probably unplugged)");
                        isDead = true;
                        setModePromise.set_value(-1); // Dead
                        continue;
                    }
                }
            }

            // paintGL...
            if (pendingRepaint)
            {
                pendingRepaint = false;
                rendering = true;
                rendRender();
                rendering = false;
                flipPage();
                swapchain.advanceAge();
                continue;
            }
            // Only updates the cursor, gamma, etc
            else if (atomicChanges && currentFb)
            {
                commit(currentFb);
            }


            /*
            if (state == SRMConnector::ChangingMode)
            {
                swapchain.resetAge();

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
                swapchain.resetAge();
                rendResume();
                usleep(1000);
                continue;
            }*/

        }

    }, std::move(initPromise)).detach();

    try {
        return initFuture.get();
    }
    catch (...) {
        return false;
    }
}

bool SRMRenderer::init() noexcept
{
    initContentType();
    initGamma();
    initCursor();

    if (!applyCrtcMode())
        return false;

    iface->initializeGL(conn, ifaceData);
    return true;
}

void SRMRenderer::unit() noexcept
{
    iface->uninitializeGL(conn, ifaceData);

    waitPendingPageFlip(1);

    if (device()->clientCaps().Atomic)
    {
        auto req { SRMAtomicRequest::Make(device()) };
        atomicReqAppendDisable(req);
        req->commit(DRM_MODE_ATOMIC_ALLOW_MODESET | DRM_MODE_ATOMIC_NONBLOCK, this, false);
    }
    else
    {
        drmModeConnectorSetProperty(device()->fd(), conn->m_id, conn->m_propIDs.DPMS, DRM_MODE_DPMS_OFF);
        drmModeSetCrtc(device()->fd(), crtc->id(), 0, 0, 0, NULL, 0, NULL);
    }

    unitPromise.value().set_value(true);
}

bool SRMRenderer::applyCrtcMode() noexcept
{
    Int32 ret;

    waitPendingPageFlip(-1);

    if (!initSwapchain())
        return false;

    if (device()->clientCaps().Atomic)
    {
        // DPMS OFF
        auto req { SRMAtomicRequest::Make(device()) };
        req->addProperty(crtc->id(), crtc->m_propIDs.ACTIVE, 0);
        req->commit(DRM_MODE_ATOMIC_ALLOW_MODESET, this, true);

        // SET MODE AND OTHER PROPS
        req = SRMAtomicRequest::Make(device());
        auto modeBlob = SRMPropertyBlob::Make(device(), &conn->currentMode()->info(), sizeof(drmModeModeInfo));
        req->attachPropertyBlob(modeBlob);
        req->addProperty(crtc->id(), crtc->m_propIDs.MODE_ID, modeBlob->id());
        req->addProperty(primaryPlane->id(), primaryPlane->m_propIDs.FB_ID, swapchain.fb()->id());
        req->addProperty(primaryPlane->id(), primaryPlane->m_propIDs.CRTC_ID, crtc->id());
        req->addProperty(primaryPlane->id(), primaryPlane->m_propIDs.CRTC_X, 0);
        req->addProperty(primaryPlane->id(), primaryPlane->m_propIDs.CRTC_Y, 0);
        req->addProperty(primaryPlane->id(), primaryPlane->m_propIDs.CRTC_W, conn->currentMode()->info().hdisplay);
        req->addProperty(primaryPlane->id(), primaryPlane->m_propIDs.CRTC_H, conn->currentMode()->info().vdisplay);
        req->addProperty(primaryPlane->id(), primaryPlane->m_propIDs.SRC_X, 0);
        req->addProperty(primaryPlane->id(), primaryPlane->m_propIDs.SRC_Y, 0);
        req->addProperty(primaryPlane->id(), primaryPlane->m_propIDs.SRC_W, (UInt64)conn->currentMode()->info().hdisplay << 16);
        req->addProperty(primaryPlane->id(), primaryPlane->m_propIDs.SRC_H, (UInt64)conn->currentMode()->info().vdisplay << 16);
        req->addProperty(conn->id(), conn->m_propIDs.CRTC_ID, crtc->id());
        if (conn->m_propIDs.link_status)
            req->addProperty(conn->id(), conn->m_propIDs.link_status, DRM_MODE_LINK_STATUS_GOOD);

        auto prevCursorIndex { cursorI };
        atomicReqAppendChanges(req, nullptr);
        ret = req->commit(DRM_MODE_ATOMIC_ALLOW_MODESET, this, true);

        // DPMS ON
        req = SRMAtomicRequest::Make(device());
        req->addProperty(crtc->id(), crtc->m_propIDs.ACTIVE, 1);
        req->commit(DRM_MODE_ATOMIC_ALLOW_MODESET, this, true);

        if (ret)
        {
            if (cursorAPI == CursorAPI::Atomic)
                cursorI = prevCursorIndex;

            logAtomic(CZError, CZLN, "Failed to set CRTC mode. DRM Error: {}", strerror(-ret));
            return false;
        }
        else
        {
            atomicChanges = 0;
        }
    }
    else
    {
        drmModeConnectorSetProperty(device()->fd(), conn->m_id, conn->m_propIDs.DPMS, DRM_MODE_DPMS_OFF);

        ret = drmModeSetCrtc(device()->fd(),
                             crtc->id(),
                             swapchain.fb()->id(),
                             0,
                             0,
                             &conn->m_id,
                             1,
                             &conn->currentMode()->m_info);

        drmModeConnectorSetProperty(device()->fd(), conn->m_id, conn->m_propIDs.DPMS, DRM_MODE_DPMS_ON);

        if (ret)
        {
            logLegacy(CZError, CZLN, "Failed to set CRTC mode. DRM Error: {}", strerror(-ret));
            return false;
        }
    }

    return true;
}

bool SRMRenderer::initSwapchain() noexcept
{
    swapchain = {};

    strategy = Self;
    if (initSwapchainSelf()) return true;

    strategy = Prime;
    if (initSwapchainPrime()) return true;

    strategy = Dumb;
    if (initSwapchainDumb()) return true;

    log(CZError, CZLN, "Failed to create swapchain");

    return false;
}

bool SRMRenderer::initSwapchainSelf() noexcept
{
    auto ream { RCore::Get() };

    if (ream->asRS() || device()->reamDevice() != ream->mainDevice())
        return false;

    const auto inFormats { RDRMFormatSet::Intersect(primaryPlane->formats(), device()->reamDevice()->renderFormats()) };

    if (inFormats.formats().empty())
        return false;

    std::vector<const RDRMFormat*> formats;
    formats.reserve(inFormats.formats().size() + 1);

    auto it { inFormats.formats().find(DRM_FORMAT_XRGB8888) };
    if (it != inFormats.formats().end()) formats.emplace_back(&(*it));

    it = inFormats.formats().find(DRM_FORMAT_XBGR8888);
    if (it != inFormats.formats().end()) formats.emplace_back(&(*it));

    for (const auto &fmt : inFormats.formats())
        if (fmt.format() != DRM_FORMAT_XRGB8888 && fmt.format() != DRM_FORMAT_XBGR8888)
            formats.emplace_back(&fmt);

    swapchain.images.resize(swapchain.n);
    swapchain.fbs.resize(swapchain.n);
    swapchain.surfaces.resize(swapchain.n);

    bool ok { false };

    RImageConstraints consts {};
    consts.allocator = device()->reamDevice();
    consts.caps[device()->reamDevice()] = RImageCap_Dst | RImageCap_DRMFb;

    for (const auto *fmt : formats)
    {
        ok = true;

        for (size_t i = 0; i < swapchain.n; i++)
        {
            swapchain.images[i] = RImage::Make(conn->currentMode()->size(), *fmt, &consts);

            if (!swapchain.images[i])
            {
                log(CZTrace, CZLN, "Failed to create swapchain RImage N° {}/{}", i + 1, swapchain.n);
                ok = false;
                break;
            }

            swapchain.fbs[i] = swapchain.images[i]->drmFb(device()->reamDevice());
            swapchain.surfaces[i] = RSurface::WrapImage(swapchain.images[i]);
        }

        if (ok)
            break;
    }

    return ok;
}

bool SRMRenderer::initSwapchainPrime() noexcept
{
    auto ream { RCore::Get() };

    if (ream->asRS() || device()->reamDevice() == ream->mainDevice())
        return false;

    auto textureFormats { RDRMFormatSet::Intersect(device()->reamDevice()->textureFormats(), ream->mainDevice()->renderFormats()) };
    textureFormats.removeModifier(DRM_FORMAT_MOD_INVALID);

    if (textureFormats.formats().empty())
        return false;

    const auto inFormats { RDRMFormatSet::Intersect(primaryPlane->formats(), device()->reamDevice()->renderFormats()) };

    if (inFormats.formats().empty())
        return false;

    std::vector<const RDRMFormat*> formats;
    formats.reserve(inFormats.formats().size() + 1);

    auto it { inFormats.formats().find(DRM_FORMAT_XRGB8888) };
    if (it != inFormats.formats().end()) formats.emplace_back(&(*it));

    it = inFormats.formats().find(DRM_FORMAT_XBGR8888);
    if (it != inFormats.formats().end()) formats.emplace_back(&(*it));

    for (const auto &fmt : inFormats.formats())
        if (fmt.format() != DRM_FORMAT_XRGB8888 && fmt.format() != DRM_FORMAT_XBGR8888)
            formats.emplace_back(&fmt);

    swapchain.fbs.resize(swapchain.n);
    swapchain.primeImages.resize(swapchain.n);
    swapchain.primeSurfaces.resize(swapchain.n);

    bool ok { false };

    RImageConstraints primeConsts {};
    primeConsts.allocator = device()->reamDevice();
    primeConsts.caps[device()->reamDevice()] = RImageCap_Dst | RImageCap_DRMFb;

    for (const auto *fmt : formats)
    {
        ok = true;

        for (size_t i = 0; i < swapchain.n; i++)
        {
            swapchain.primeImages[i] = RImage::Make(conn->currentMode()->size(), *fmt, &primeConsts);

            if (!swapchain.primeImages[i])
            {
                log(CZTrace, CZLN, "Failed to create swapchain RImage {}/{}", i+1, swapchain.n);
                ok = false;
                break;
            }

            swapchain.fbs[i] = swapchain.primeImages[i]->drmFb(device()->reamDevice());
            swapchain.primeSurfaces[i] = RSurface::WrapImage(swapchain.primeImages[i]);
        }

        if (ok)
            break;
    }

    if (!ok)
        return false;

    swapchain.images.resize(swapchain.n);
    swapchain.surfaces.resize(swapchain.n);

    formats.clear();
    formats.reserve(textureFormats.formats().size());

    it = textureFormats.formats().find(DRM_FORMAT_XRGB8888);
    if (it != textureFormats.formats().end()) formats.emplace_back(&(*it));

    it = textureFormats.formats().find(DRM_FORMAT_XBGR8888);
    if (it != textureFormats.formats().end()) formats.emplace_back(&(*it));

    for (const auto &fmt : textureFormats.formats())
        if (fmt.format() != DRM_FORMAT_XRGB8888 && fmt.format() != DRM_FORMAT_XBGR8888)
            formats.emplace_back(&fmt);

    ok = false;

    RImageConstraints consts {};
    consts.allocator = ream->mainDevice();
    consts.caps[ream->mainDevice()] = RImageCap_Dst;
    consts.caps[device()->reamDevice()] = RImageCap_Src;

    for (const auto *fmt : formats)
    {
        ok = true;

        for (size_t i = 0; i < swapchain.n; i++)
        {
            swapchain.images[i] = RImage::Make(conn->currentMode()->size(), *fmt, &consts);

            if (!swapchain.images[i])
            {
                log(CZTrace, CZLN, "Failed to create PRIME swapchain RImage {}/{}", i+1, swapchain.n);
                ok = false;
                break;
            }

            swapchain.surfaces[i] = RSurface::WrapImage(swapchain.images[i]);
        }

        if (ok)
            break;
    }

    return ok;
}

bool SRMRenderer::initSwapchainDumb() noexcept
{
    auto ream { RCore::Get() };

    if (!device()->reamDevice()->caps().DumbBuffer)
        return false;

    const auto inFormats { RDRMFormatSet::Intersect(primaryPlane->formats(), ream->mainDevice()->renderFormats()) };

    if (inFormats.formats().empty())
        return false;

    std::vector<const RDRMFormat*> formats;
    formats.reserve(inFormats.formats().size());
    const std::unordered_set<RFormat> preferred {
        DRM_FORMAT_XRGB8888,
        DRM_FORMAT_XBGR8888,
        DRM_FORMAT_ARGB8888,
        DRM_FORMAT_ABGR8888
    };

    auto it { inFormats.formats().find(DRM_FORMAT_XRGB8888) };
    if (it != inFormats.formats().end()) formats.emplace_back(&(*it));

    it = inFormats.formats().find(DRM_FORMAT_XBGR8888);
    if (it != inFormats.formats().end()) formats.emplace_back(&(*it));

    it = inFormats.formats().find(DRM_FORMAT_ARGB8888);
    if (it != inFormats.formats().end()) formats.emplace_back(&(*it));

    it = inFormats.formats().find(DRM_FORMAT_ABGR8888);
    if (it != inFormats.formats().end()) formats.emplace_back(&(*it));

    for (const auto &fmt : inFormats.formats())
        if (!preferred.contains(fmt.format()))
            formats.emplace_back(&fmt);

    swapchain.fbs.resize(swapchain.n);
    swapchain.dumbBuffers.resize(swapchain.n);
    swapchain.images.resize(swapchain.n);

    bool ok { false };

    RImageConstraints consts {};
    consts.allocator = ream->mainDevice();
    consts.caps[ream->mainDevice()] = RImageCap_Dst;

    for (const auto *fmt : formats)
    {
        ok = true;

        consts.readFormats.clear();
        consts.readFormats.emplace(fmt->format());

        for (size_t i = 0; i < swapchain.n; i++)
        {
            swapchain.images[i] = RImage::Make(conn->currentMode()->size(), *fmt, &consts);

            if (!swapchain.images[i])
            {
                log(CZTrace, CZLN, "Failed to create swapchain RImage {}", i);
                ok = false;
                break;
            }

            swapchain.dumbBuffers[i] = RDumbBuffer::Make(conn->currentMode()->size(), *fmt, device()->reamDevice());

            if (!swapchain.dumbBuffers[i])
            {
                for (auto format : swapchain.images[i]->readFormats())
                {
                    if (format == fmt->format())
                        continue;

                    swapchain.dumbBuffers[i] = RDumbBuffer::Make(conn->currentMode()->size(), {format, {DRM_FORMAT_MOD_LINEAR}}, device()->reamDevice());

                    if (swapchain.dumbBuffers[i])
                        break;
                }
            }

            if (!swapchain.dumbBuffers[i])
            {
                log(CZTrace, CZLN, "Failed to create RDumbBufer {}", i);
                ok = false;
                break;
            }

            swapchain.fbs[i] = swapchain.dumbBuffers[i]->fb();
        }

        if (ok)
            break;
    }

    if (!ok)
        return false;

    return ok;
}

bool SRMRenderer::flipPage() noexcept
{
    switch (strategy)
    {
    case Self:
        flipPageSelf();
        break;
    case Prime:
        flipPagePrime();
        break;
    case Dumb:
        flipPageDumb();
        break;
    }

    commit(swapchain.fb());
    iface->pageFlipped(conn, ifaceData);
    return true;
}

bool SRMRenderer::flipPageSelf() noexcept
{
    if (device()->clientCaps().Atomic && primaryPlane->m_propIDs.IN_FENCE_FD)
    {
        if (swapchain.image()->writeSync())
        {
            swapchain.image()->writeSync()->gpuWait(device()->reamDevice());
            inFence.reset(swapchain.image()->writeSync()->fd().release());
        }
    }

    if (device()->reamDevice()->drmDriver() == RDriver::nvidia && inFence.get() < 0)
        device()->reamDevice()->wait();

    return true;
}

bool SRMRenderer::flipPagePrime() noexcept
{
    auto srcImage { swapchain.image() };
    auto ream { RCore::Get() };

    const bool gpuSync { srcImage->writeSync() && device()->caps().PrimeImport && ream->mainDevice()->caps().SyncExport };

    if (!gpuSync)
        srcImage->allocator()->wait();

    auto pass { swapchain.primeSurface()->beginPass(RPassCap_Painter, device()->reamDevice()) };
    auto *p { pass->getPainter() };
    p->setBlendMode(RBlendMode::Src);
    RDrawImageInfo info {};
    info.image = srcImage;
    info.src = SkRect::Make(srcImage->size());
    info.dst = SkIRect::MakeSize(srcImage->size());
    p->drawImage(info, &conn->damage);
    pass.reset();

    auto primeImage { swapchain.primeImage() };

    if (device()->clientCaps().Atomic && primaryPlane->m_propIDs.IN_FENCE_FD)
    {
        if (primeImage->writeSync())
            inFence.reset(primeImage->writeSync()->fd().release());
    }

    return true;
}

bool SRMRenderer::flipPageDumb() noexcept
{
    auto dumb { swapchain.dumbBuffer() };
    RPixelBufferRegion info {};
    info.pixels = dumb->pixels();
    info.stride = dumb->stride();
    info.format = dumb->formatInfo().format;
    info.region = conn->damage;
    return swapchain.image()->readPixels(info);
}

void SRMRenderer::waitForRepaintRequest() noexcept
{
    const bool needsWait { (!pendingRepaint && atomicChanges == 0) || device()->core()->isSuspended() };

    if (needsWait)
    {
        atomicChanges.set(0);
        repaintSemaphore.acquire();
    }
}

bool SRMRenderer::waitPendingPageFlip(int iterLimit) noexcept
{
    pollfd fds {};
    fds.fd = device()->fd();
    fds.events = POLLIN;

    if (pendingPageFlip)
    {
        while (pendingPageFlip)
        {
            if (iterLimit == 0)
                return false;

            const std::lock_guard<std::recursive_mutex> lock { device()->m_pageFlipMutex };

            // Double check if the pageflip was notified in another thread
            if (!pendingPageFlip)
                return true;

            poll(&fds, 1, iterLimit == -1 ? 500 : 1);
            drmHandleEvent(fds.fd, &drmEventCtx);

            if (iterLimit > 0)
                iterLimit--;
        }
    }
    else
    {
        const std::lock_guard<std::recursive_mutex> lock { device()->m_pageFlipMutex };

        while (poll(&fds, 1, 0) > 0)
            drmHandleEvent(fds.fd, &drmEventCtx);
    }

    return true;
}

void SRMRenderer::commit(std::shared_ptr<RDRMFramebuffer> fb) noexcept
{
    assert(fb);

    int ret { 0 };

    if (pendingPageFlip || swapchain.n == 1 || swapchain.n > 2)
        waitPendingPageFlip(-1);

    if (device()->clientCaps().Atomic)
    {
        const std::lock_guard<std::recursive_mutex> lock { propsMutex };

        const bool asyncFlip { !currentVSync && atomicChanges.get() == 0 && !primaryPlane->m_syncOnlyModifiers.contains(fb->modifier()) };

        // DRM_MODE_PAGE_FLIP_ASYNC only accepts changing the fb of the primary plane
        if (asyncFlip)
        {
            auto req { SRMAtomicRequest::Make(device()) };
            atomicReqAppendPrimaryPlane(req, fb);
            ret = req->commit(DRM_MODE_PAGE_FLIP_EVENT | DRM_MODE_PAGE_FLIP_ASYNC | DRM_MODE_ATOMIC_NONBLOCK, this, false);

            if (ret == -EINVAL)
            {
                if (fb->modifier() != DRM_FORMAT_MOD_INVALID)
                {
                    logAtomic(CZError, "Async pageflip failed: {}. Modifier {} added to sync-only list.", strerror(-ret), RDRMFormat::ModifierName(fb->modifier()));
                    primaryPlane->m_syncOnlyModifiers.emplace(fb->modifier());
                }
            }
        }

        // Sync flip
        if (!asyncFlip || ret)
        {
            auto req { SRMAtomicRequest::Make(device()) };
            atomicReqAppendChanges(req, fb);
            const auto prevCursorIndex { cursorI };
            ret = req->commit(DRM_MODE_PAGE_FLIP_EVENT | DRM_MODE_ATOMIC_NONBLOCK, this, false);

            if (ret)
            {
                if (cursorPlane)
                    cursorI = prevCursorIndex;
            }
            else
                atomicChanges = 0;
        }

        if (ret && ret != -EACCES)
            logAtomic(CZError, CZLN, "Failed to page flip. DRM Error: {}", strerror(-ret));
    }
    else
    {
        const auto primaryPlaneFb { fb->id() };
        const bool asyncFlip { !currentVSync && !primaryPlane->m_syncOnlyModifiers.contains(fb->modifier()) };

        if (asyncFlip)
        {
            ret = drmModePageFlip(device()->fd(), crtc->id(), primaryPlaneFb, DRM_MODE_PAGE_FLIP_ASYNC | DRM_MODE_PAGE_FLIP_EVENT, this);

            if (ret == -EINVAL)
            {
                if (fb->modifier() != DRM_FORMAT_MOD_INVALID)
                {
                    logLegacy(CZError, "Async pageflip failed: {}. Modifier {} added to sync-only list.", strerror(-ret), RDRMFormat::ModifierName(fb->modifier()));
                    primaryPlane->m_syncOnlyModifiers.emplace(fb->modifier());
                }
            }
        }

        if (!asyncFlip || ret)
            ret = drmModePageFlip(device()->fd(), crtc->id(), primaryPlaneFb, DRM_MODE_PAGE_FLIP_EVENT, this);

        if (ret && ret != -EACCES)
            logLegacy(CZError, CZLN, "Failed to page flip. DRM Error: {}", strerror(-ret));
    }


    if (ret)
    {
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
    else
    {
        currentFb = fb;
        pendingPageFlip = true;
    }

    if (swapchain.n == 2 || firstPageFlip)
    {
        firstPageFlip = false;
        waitPendingPageFlip(-1);
    }
}

bool SRMRenderer::rendRender() noexcept
{
    const auto currentImageRect { SkIRect::MakeSize(swapchain.image()->size()) };
    conn->damage.setRect(currentImageRect);
    iface->paintGL(conn, ifaceData);
    conn->damage.op(currentImageRect, SkRegion::kIntersect_Op);
    return false;
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

void SRMRenderer::atomicReqAppendChanges(std::shared_ptr<SRMAtomicRequest> req, std::shared_ptr<RDRMFramebuffer> fb) noexcept
{
    if (fb)
        atomicReqAppendPrimaryPlane(req, fb);

    if (atomicChanges.has(CHContentType))
        req->addProperty(conn->id(), conn->m_propIDs.content_type, static_cast<UInt64>(conn->contentType()));

    if (atomicChanges.has(CHGammaLUT))
        req->addProperty(crtc->id(), crtc->m_propIDs.GAMMA_LUT, gammaBlob ? gammaBlob->id() : 0);

    if (cursorAPI == CursorAPI::Atomic)
    {
        bool updatedFB { false };

        if (atomicChanges.has(CHCursorBuffer))
        {
            cursorI = 1 - cursorI;

            if (cursorVisible)
            {
                req->addProperty(cursorPlane->id(), cursorPlane->m_propIDs.FB_ID, cursor[cursorI].fb->id());
                updatedFB = 1;
            }
        }

        UInt8 updatedVisibility = 0;

        if (atomicChanges.has(CHCursorVisibility))
        {
            if (cursorVisible)
            {
                updatedVisibility = 1;

                if (!updatedFB)
                    req->addProperty(cursorPlane->id(), cursorPlane->m_propIDs.FB_ID, cursor[cursorI].fb->id());

                req->addProperty(cursorPlane->id(), cursorPlane->m_propIDs.CRTC_ID, crtc->id());
                req->addProperty(cursorPlane->id(), cursorPlane->m_propIDs.CRTC_X, cursorPos.x());
                req->addProperty(cursorPlane->id(), cursorPlane->m_propIDs.CRTC_Y, cursorPos.y());
                req->addProperty(cursorPlane->id(), cursorPlane->m_propIDs.CRTC_W, 64);
                req->addProperty(cursorPlane->id(), cursorPlane->m_propIDs.CRTC_H, 64);
                req->addProperty(cursorPlane->id(), cursorPlane->m_propIDs.SRC_X, 0);
                req->addProperty(cursorPlane->id(), cursorPlane->m_propIDs.SRC_Y, 0);
                req->addProperty(cursorPlane->id(), cursorPlane->m_propIDs.SRC_W, (UInt64)64 << 16);
                req->addProperty(cursorPlane->id(), cursorPlane->m_propIDs.SRC_H, (UInt64)64 << 16);
            }
            else
            {
                req->addProperty(cursorPlane->id(), cursorPlane->m_propIDs.CRTC_ID, 0);
                req->addProperty(cursorPlane->id(), cursorPlane->m_propIDs.FB_ID, 0);
            }
        }

        if (atomicChanges.has(CHCursorPosition))
        {
            if (!updatedVisibility)
            {
                req->addProperty(cursorPlane->id(), cursorPlane->m_propIDs.CRTC_X, cursorPos.x());
                req->addProperty(cursorPlane->id(), cursorPlane->m_propIDs.CRTC_Y, cursorPos.y());
            }
        }
    } 
}

void SRMRenderer::atomicReqAppendPrimaryPlane(std::shared_ptr<SRMAtomicRequest> req, std::shared_ptr<RDRMFramebuffer> fb) noexcept
{
    // Note: Both fb and crtc must be set or neither
    req->addProperty(primaryPlane->id(), primaryPlane->m_propIDs.FB_ID, fb ? fb->id() : 0);

    if (primaryPlane->m_propIDs.IN_FENCE_FD)
       req->addProperty(primaryPlane->id(), primaryPlane->m_propIDs.IN_FENCE_FD, inFence.get());

    // Apparently the kernel only uses the fd to access a dma fence but doesn't own it
    // so by attaching it here, it will be closed after the commit, when the request is destroyed
    req->attachFd(inFence.release());
}

void SRMRenderer::atomicReqAppendDisable(std::shared_ptr<SRMAtomicRequest> req) noexcept
{
    req->addProperty(crtc->id(), crtc->m_propIDs.ACTIVE, 0);
    req->addProperty(crtc->id(), crtc->m_propIDs.MODE_ID, 0);
    req->addProperty(conn->id(), conn->m_propIDs.CRTC_ID, 0);
    req->addProperty(primaryPlane->id(), primaryPlane->m_propIDs.CRTC_ID, 0);
    req->addProperty(primaryPlane->id(), primaryPlane->m_propIDs.FB_ID, 0);
}

void SRMRenderer::logInfo() noexcept
{
    if (SRMLog.level() >= CZInfo)
    {
        auto ream { RCore::Get() };
        printf("\n");
        SRMLog(CZInfo, "---------------- Connector Initialized ----------------");
        SRMLog(CZInfo, "Name: {} {} {}", conn->name(), conn->model(), conn->make());
        SRMLog(CZInfo, "Mode: {} x {} @ {}", conn->currentMode()->size().width(), conn->currentMode()->size().height(), conn->currentMode()->refreshRate());
        SRMLog(CZInfo, "Strategy: {}", StrategyString(strategy));
        SRMLog(CZInfo, "DRM API: {}", device()->clientCaps().Atomic ? "Atomic" : "Legacy");
        SRMLog(CZInfo, "Device: {} - {}", device()->nodeName(), device()->reamDevice()->drmDriverName());
        SRMLog(CZInfo, "Renderer: {} - {}", ream->mainDevice()->srmDevice()->nodeName(), ream->mainDevice()->drmDriverName());
        SRMLog(CZInfo, "Surface Format: {} - {}", RDRMFormat::FormatName(swapchain.images[0]->formatInfo().format), RDRMFormat::ModifierName(swapchain.images[0]->modifier()));
        SRMLog(CZInfo, "Buffering: {}", swapchain.n);
        SRMLog(CZInfo, "Cursor Plane: {}", cursor[0].bo != nullptr);
        SRMLog(CZInfo, "-------------------------------------------------------\n");
    }
}

SRMDevice *SRMRenderer::device() const noexcept
{
    return conn->device();
}
