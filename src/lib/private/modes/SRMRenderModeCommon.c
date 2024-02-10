#include <private/SRMCrtcPrivate.h>
#include <private/SRMPlanePrivate.h>
#include <private/modes/SRMRenderModeCommon.h>
#include <private/SRMDevicePrivate.h>
#include <private/SRMConnectorPrivate.h>
#include <private/SRMConnectorModePrivate.h>

#include <SRMCore.h>
#include <SRMLog.h>
#include <stdlib.h>
#include <xf86drmMode.h>
#include <sys/poll.h>
#include <unistd.h>

// Choose EGL configurations

Int8 srmRenderModeCommonMatchConfigToVisual(EGLDisplay egl_display, EGLint visual_id, EGLConfig *configs, int count)
{
    for (int i = 0; i < count; ++i)
    {
        EGLint id;

        if (!eglGetConfigAttrib(egl_display, configs[i], EGL_NATIVE_VISUAL_ID, &id))
            continue;

        if (id == visual_id)
            return i;
    }

    return -1;
}

Int8  srmRenderModeCommonChooseEGLConfiguration(EGLDisplay egl_display, const EGLint *attribs, EGLint visual_id, EGLConfig *config_out)
{
    EGLint count = 0;
    EGLint matched = 0;
    EGLConfig *configs;
    int config_index = -1;

    if (!eglGetConfigs(egl_display, NULL, 0, &count) || count < 1)
    {
        SRMError("No EGL configs to choose from.");
        return 0;
    }

    configs = (void**)malloc(count * sizeof *configs);

    if (!configs)
        return 0;

    if (!eglChooseConfig(egl_display, attribs, configs, count, &matched) || !matched)
    {
        SRMError("No EGL configs with appropriate attributes.");
        goto out;
    }

    if (!visual_id)
    {
        config_index = 0;
    }

    if (config_index == -1)
    {
        config_index = srmRenderModeCommonMatchConfigToVisual(egl_display, visual_id, configs, matched);
    }

    if (config_index != -1)
    {
        *config_out = configs[config_index];
    }

out:
    free(configs);
    if (config_index == -1)
        return 0;

    return 1;
}

void srmRenderModeCommonPageFlipHandler(Int32 fd, UInt32 seq, UInt32 sec, UInt32 usec, void *data)
{
    SRM_UNUSED(fd);

    if (data)
    {
        SRMConnector *connector = data;
        connector->pendingPageFlip = 0;
        connector->onlyAsyncCursorUpdate = 0;

        if (connector->state == SRM_CONNECTOR_STATE_UNINITIALIZED)
            return;

        if (connector->currentVSync)
        {
            connector->presentationTime.flags = SRM_PRESENTATION_TIME_FLAGS_HW_CLOCK |
                                                SRM_PRESENTATION_TIME_FLAGS_HW_COMPLETION |
                                                SRM_PRESENTATION_TIME_FLAGS_VSYNC;

            connector->presentationTime.frame = seq;
            connector->presentationTime.time.tv_sec = sec;
            connector->presentationTime.time.tv_nsec = usec * 1000;
            connector->presentationTime.period = connector->currentMode->info.vrefresh == 0 ? 0 : 1000000000/connector->currentMode->info.vrefresh;
        }
        else
        {
            connector->presentationTime.flags = 0;
            connector->presentationTime.frame = 0;
            connector->presentationTime.period = 0;

            Int64 prevUsec = (connector->presentationTime.time.tv_sec * 1000000LL) + (connector->presentationTime.time.tv_nsec / 1000LL);
            clock_gettime(connector->device->clock, &connector->presentationTime.time);

            if (connector->maxRefreshRate < 0)
                return;

            Int64 currUsec = (connector->presentationTime.time.tv_sec * 1000000LL) + (connector->presentationTime.time.tv_nsec / 1000LL);

            Int64 periodUsec;

            // Limit FPS to 2 * vrefresh
            if (connector->maxRefreshRate == 0)
                periodUsec = (connector->currentMode->info.vrefresh == 0 ? 0 : 490000/(connector->currentMode->info.vrefresh));
            else
                periodUsec = (connector->currentMode->info.vrefresh == 0 ? 0 : 1000000/(connector->maxRefreshRate));

            if (fd == -1 && periodUsec > 0)
            {
                Int64 diffUsec = currUsec - prevUsec;

                if (diffUsec >= 0 && diffUsec < periodUsec)
                {
                    usleep(periodUsec - diffUsec);
                    clock_gettime(connector->device->clock, &connector->presentationTime.time);
                }
            }

        }
    }
}

UInt8 srmRenderModeCommonCreateCursor(SRMConnector *connector)
{
    connector->cursorBO = gbm_bo_create(connector->device->gbm,
                                        64,
                                        64,
                                        GBM_FORMAT_ARGB8888,
                                        GBM_BO_USE_CURSOR | GBM_BO_USE_WRITE);

    if (connector->cursorBO)
    {
        if (connector->device->clientCapAtomic)
        {
            Int32 ret = drmModeAddFB(connector->device->fd,
                         gbm_bo_get_width(connector->cursorBO),
                         gbm_bo_get_height(connector->cursorBO),
                         32,
                         gbm_bo_get_bpp(connector->cursorBO),
                         gbm_bo_get_stride(connector->cursorBO),
                         gbm_bo_get_handle(connector->cursorBO).u32,
                         &connector->cursorFB);

            if (ret)
            {
                SRMError("Failed to setup hw cursor for connector %d.", connector->id);
                goto fail;
            }
        }
    }
    else
        return 0;

    connector->cursorBOPending = gbm_bo_create(connector->device->gbm,
                                        64,
                                        64,
                                        GBM_FORMAT_ARGB8888,
                                        GBM_BO_USE_CURSOR | GBM_BO_USE_WRITE);

    if (connector->cursorBOPending)
    {
        if (connector->device->clientCapAtomic)
        {
            Int32 ret = drmModeAddFB(connector->device->fd,
                         gbm_bo_get_width(connector->cursorBOPending),
                         gbm_bo_get_height(connector->cursorBOPending),
                         32,
                         gbm_bo_get_bpp(connector->cursorBOPending),
                         gbm_bo_get_stride(connector->cursorBOPending),
                         gbm_bo_get_handle(connector->cursorBOPending).u32,
                         &connector->cursorFBPending);

            if (ret)
            {
                SRMError("Failed to setup hw cursor for connector %d.", connector->id);
                goto fail;
            }

            return 1;
        }

        return 1;
    }

    fail:

    if (connector->cursorFB)
    {
        drmModeRmFB(connector->device->fd, connector->cursorFB);
        connector->cursorFB = 0;
    }

    if (connector->cursorBO)
    {
        gbm_bo_destroy(connector->cursorBO);
        connector->cursorBO = NULL;
    }

    if (connector->cursorFBPending)
    {
        drmModeRmFB(connector->device->fd, connector->cursorFBPending);
        connector->cursorFBPending = 0;
    }

    if (connector->cursorBOPending)
    {
        gbm_bo_destroy(connector->cursorBOPending);
        connector->cursorBOPending = NULL;
    }

    SRMError("Failed to setup hw cursor for connector %d.", connector->id);
    return 0;
}

UInt8 srmRenderModeCommonWaitRepaintRequest(SRMConnector *connector)
{
    pthread_mutex_lock(&connector->repaintMutex);

    if ((!connector->repaintRequested && !connector->atomicChanges) || srmCoreIsSuspended(connector->device->core))
    {
        connector->atomicChanges = 0;
        pthread_cond_wait(&connector->repaintCond, &connector->repaintMutex);
    }

    pthread_mutex_unlock(&connector->repaintMutex);

    pthread_mutex_lock(&connector->stateMutex);
    if (connector->state == SRM_CONNECTOR_STATE_UNINITIALIZING)
    {
        pthread_mutex_unlock(&connector->stateMutex);
        connector->pendingPageFlip = 1;
        srmRenderModeCommonWaitPageFlip(connector, 3);
        connector->interface->uninitializeGL(connector, connector->interfaceData);
        connector->renderInterface.uninitialize(connector);
        eglReleaseThread();
        connector->state = SRM_CONNECTOR_STATE_UNINITIALIZED;
        return 0;
    }
    pthread_mutex_unlock(&connector->stateMutex);

    return 1;
}

void srmRenderModeCommitAtomicChanges(SRMConnector *connector, drmModeAtomicReqPtr req, UInt8 clearFlags)
{
    pthread_mutex_lock(&connector->propsMutex);

    if (connector->currentCursorPlane)
    {
        UInt8 updatedFB = 0;

        if (connector->atomicChanges & SRM_ATOMIC_CHANGE_CURSOR_BUFFER)
        {
            if (clearFlags)
                connector->atomicChanges &= ~SRM_ATOMIC_CHANGE_CURSOR_BUFFER;

            struct gbm_bo *tmpBo = connector->cursorBO;
            connector->cursorBO = connector->cursorBOPending;
            connector->cursorBOPending = tmpBo;

            UInt32 tmpFb = connector->cursorFB;
            connector->cursorFB = connector->cursorFBPending;
            connector->cursorFBPending = tmpFb;

            if (connector->cursorVisible)
            {
                drmModeAtomicAddProperty(req,
                                        connector->currentCursorPlane->id,
                                        connector->currentCursorPlane->propIDs.FB_ID,
                                        connector->cursorFB);
                updatedFB = 1;
            }
        }

        if (connector->atomicChanges & SRM_ATOMIC_CHANGE_CURSOR_VISIBILITY)
        {
            if (clearFlags)
                connector->atomicChanges &= ~SRM_ATOMIC_CHANGE_CURSOR_VISIBILITY;

            if (connector->cursorVisible)
            {
                if (!updatedFB)
                {
                    drmModeAtomicAddProperty(req,
                                            connector->currentCursorPlane->id,
                                            connector->currentCursorPlane->propIDs.FB_ID,
                                            connector->cursorFB);
                }

                drmModeAtomicAddProperty(req,
                                        connector->currentCursorPlane->id,
                                        connector->currentCursorPlane->propIDs.CRTC_ID,
                                        connector->currentCrtc->id);

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
            SRMError("Failed to create gamma lut blob for connector %d.", connector->id);
	    }
        else
        {
            drmModeAtomicAddProperty(req,
                connector->currentCrtc->id,
                connector->currentCrtc->propIDs.GAMMA_LUT,
                connector->gammaBlobId);
        }
    }

    pthread_mutex_unlock(&connector->propsMutex);
}

void srmRenderModeCommonDestroyCursor(SRMConnector *connector)
{
    if (connector->cursorVisible)
    {
        if (connector->device->clientCapAtomic)
        {
            drmModeAtomicReqPtr req;
            req = drmModeAtomicAlloc();

            drmModeAtomicAddProperty(req,
                                     connector->currentCursorPlane->id,
                                     connector->currentCursorPlane->propIDs.CRTC_ID,
                                     0);

            drmModeAtomicAddProperty(req,
                                     connector->currentCursorPlane->id,
                                     connector->currentCursorPlane->propIDs.FB_ID,
                                     0);

            drmModeAtomicCommit(connector->device->fd, req, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);
            drmModeAtomicFree(req);
        }
    }

    if (connector->cursorFB)
    {
        drmModeRmFB(connector->device->fd, connector->cursorFB);
        connector->cursorFB = 0;
    }

    if (connector->cursorBO)
    {
        gbm_bo_destroy(connector->cursorBO);
        connector->cursorBO = NULL;
    }

    if (connector->cursorFBPending)
    {
        drmModeRmFB(connector->device->fd, connector->cursorFBPending);
        connector->cursorFBPending = 0;
    }

    if (connector->cursorBOPending)
    {
        gbm_bo_destroy(connector->cursorBOPending);
        connector->cursorBOPending = NULL;
    }

    connector->atomicChanges &= ~(SRM_ATOMIC_CHANGE_CURSOR_VISIBILITY | SRM_ATOMIC_CHANGE_CURSOR_POSITION | SRM_ATOMIC_CHANGE_CURSOR_BUFFER);
    connector->cursorVisible = 0;
}

Int32 srmRenderModeAtomicCommit(Int32 fd, drmModeAtomicReqPtr req, UInt32 flags, void *data, UInt8 forceRetry)
{
    if (!forceRetry)
        return drmModeAtomicCommit(fd, req, flags, data);

    Int32 ret;

    retry:
    ret = drmModeAtomicCommit(fd, req, flags, data);

    // -EBUSY
    if (ret == -16)
    {
        usleep(2000);
        goto retry;
    }

    return ret;
}

Int32 srmRenderModeCommonUpdateMode(SRMConnector *connector, UInt32 fb)
{
    connector->lastFb = fb;
    Int32 ret;

    if (connector->targetMode->info.hdisplay == connector->currentMode->info.hdisplay &&
        connector->targetMode->info.vdisplay == connector->currentMode->info.vdisplay)
    {
        connector->currentMode = connector->targetMode;

        if (connector->device->clientCapAtomic)
        {
            // Unset mode
            ret = srmRenderModeAtomicResetConnectorProps(connector);

            if (ret)
            {
                SRMError("Failed unset mode on device %s connector %d. Error: %d. (Atomic)",
                         connector->device->name,
                         connector->id, ret);
            }

            // Set mode
            drmModeAtomicReqPtr req = drmModeAtomicAlloc();

            if (connector->currentModeBlobId)
            {
                drmModeDestroyPropertyBlob(connector->device->fd, connector->currentModeBlobId);
                connector->currentModeBlobId = 0;
            }

            drmModeCreatePropertyBlob(connector->device->fd,
                                      &connector->currentMode->info,
                                      sizeof(drmModeModeInfo),
                                      &connector->currentModeBlobId);

            drmModeAtomicAddProperty(req,
                                     connector->currentCrtc->id,
                                     connector->currentCrtc->propIDs.MODE_ID,
                                     connector->currentModeBlobId);

            drmModeAtomicAddProperty(req,
                                     connector->currentCrtc->id,
                                     connector->currentCrtc->propIDs.ACTIVE,
                                     1);

            // Connector

            drmModeAtomicAddProperty(req,
                                     connector->id,
                                     connector->propIDs.CRTC_ID,
                                     connector->currentCrtc->id);

            drmModeAtomicAddProperty(req,
                                     connector->id,
                                     connector->propIDs.link_status,
                                     DRM_MODE_LINK_STATUS_GOOD);

            // Plane

            drmModeAtomicAddProperty(req,
                                     connector->currentPrimaryPlane->id,
                                     connector->currentPrimaryPlane->propIDs.CRTC_ID,
                                     connector->currentCrtc->id);

            drmModeAtomicAddProperty(req,
                                     connector->currentPrimaryPlane->id,
                                     connector->currentPrimaryPlane->propIDs.CRTC_X,
                                     0);

            drmModeAtomicAddProperty(req,
                                     connector->currentPrimaryPlane->id,
                                     connector->currentPrimaryPlane->propIDs.CRTC_Y,
                                     0);

            drmModeAtomicAddProperty(req,
                                     connector->currentPrimaryPlane->id,
                                     connector->currentPrimaryPlane->propIDs.CRTC_W,
                                     connector->currentMode->info.hdisplay);

            drmModeAtomicAddProperty(req,
                                     connector->currentPrimaryPlane->id,
                                     connector->currentPrimaryPlane->propIDs.CRTC_H,
                                     connector->currentMode->info.vdisplay);

            drmModeAtomicAddProperty(req,
                                     connector->currentPrimaryPlane->id,
                                     connector->currentPrimaryPlane->propIDs.FB_ID,
                                     fb);

            drmModeAtomicAddProperty(req,
                                     connector->currentPrimaryPlane->id,
                                     connector->currentPrimaryPlane->propIDs.SRC_X,
                                     0);

            drmModeAtomicAddProperty(req,
                                     connector->currentPrimaryPlane->id,
                                     connector->currentPrimaryPlane->propIDs.SRC_Y,
                                     0);

            drmModeAtomicAddProperty(req,
                                     connector->currentPrimaryPlane->id,
                                     connector->currentPrimaryPlane->propIDs.SRC_W,
                                     (UInt64)connector->currentMode->info.hdisplay << 16);

            drmModeAtomicAddProperty(req,
                                     connector->currentPrimaryPlane->id,
                                     connector->currentPrimaryPlane->propIDs.SRC_H,
                                     (UInt64)connector->currentMode->info.vdisplay << 16);

            srmRenderModeCommitAtomicChanges(connector, req, 1);

            ret = srmRenderModeAtomicCommit(connector->device->fd,
                                req,
                                DRM_MODE_ATOMIC_ALLOW_MODESET,
                                connector, 1);

            if (ret)
            {
                SRMError("Failed set mode with same size on device %s connector %d. Error: %d. (atomic)",
                         connector->device->name,
                         connector->id, ret);
            }

            drmModeAtomicFree(req);
        }
        else
        {
            connector->pendingPageFlip = 1;
            srmRenderModeCommonWaitPageFlip(connector, 3);
            connector->pendingPageFlip = 0;

            int ret;

            drmModeSetCrtc(connector->device->fd,
                           connector->currentCrtc->id,
                           0,
                           0,
                           0,
                           NULL,
                           0,
                           NULL);

            retry:
            ret = drmModeSetCrtc(connector->device->fd,
                               connector->currentCrtc->id,
                               fb,
                               0,
                               0,
                               &connector->id,
                               1,
                               &connector->currentMode->info);
            if (ret)
            {
                SRMError("Failed unset mode on device %s connector %d. Error: %d. (legacy)",
                         connector->device->name,
                         connector->id, ret);
                goto retry;
            }
        }

        connector->interface->resizeGL(connector,
                                       connector->interfaceData);

        return 0;
    }
    else
    {
        if (connector->device->clientCapAtomic)
        {
            // Unset mode
            ret = srmRenderModeAtomicResetConnectorProps(connector);

            if (ret)
            {
                SRMError("Failed unset mode on device %s connector %d. Error: %d. (atomic)",
                         connector->device->name,
                         connector->id, ret);
            }
        }
        else
        {
            connector->pendingPageFlip = 1;
            srmRenderModeCommonWaitPageFlip(connector, 3);
            connector->pendingPageFlip = 0;

            ret = drmModeSetCrtc(connector->device->fd,
                               connector->currentCrtc->id,
                               0,
                               0,
                               0,
                               NULL,
                               0,
                               NULL);

            if (ret)
            {
                SRMError("Failed unset mode on device %s connector %d. Error: %d. (legacy)",
                         connector->device->name,
                         connector->id, ret);
            }
        }

        return 1;
    }
}

void srmRenderModeCommonUninitialize(SRMConnector *connector)
{
    Int32 ret;

    if (connector->device->clientCapAtomic)
    {
        ret = srmRenderModeAtomicResetConnectorProps(connector);

        if (ret)
        {
            SRMError("Failed uninitialize device %s connector %d. Error: %d. (atomic)",
                     connector->device->name,
                     connector->id, ret);
        }
    }
    else
    {
        ret = drmModeSetCrtc(connector->device->fd,
                           connector->currentCrtc->id,
                           0,
                           0,
                           0,
                           NULL,
                           0,
                           NULL);

        if (ret)
        {
            SRMError("Failed uninitialize device %s connector %d. Error: %d. (legacy)",
                     connector->device->name,
                     connector->id, ret);
        }
    }
}

void srmRenderModeCommonPauseRendering(SRMConnector *connector)
{
    Int32 ret;

    if (connector->device->clientCapAtomic)
    {
        ret = srmRenderModeAtomicResetConnectorProps(connector);

        if (ret)
        {
            SRMWarning("Failed to reset CRTC device %s connector %d. Error: %d (not DRM master). (atomic)",
                     connector->device->name,
                     connector->id, ret);

        }
    }
    else
    {
        ret = drmModeSetCrtc(connector->device->fd,
                       connector->currentCrtc->id,
                       0,
                       0,
                       0,
                       NULL,
                       0,
                       NULL);

        if (ret)
        {
            SRMError("Failed to reset CRTC device %s connector %d. Error: %d (not DRM master). (legacy)",
                     connector->device->name,
                     connector->id, ret);
        }
    }
}

void srmRenderModeCommonResumeRendering(SRMConnector *connector, UInt32 fb)
{
    Int32 ret;
    connector->lastFb = fb;

    if (connector->device->clientCapAtomic)
    {
        drmModeAtomicReqPtr req;
        req = drmModeAtomicAlloc();

        if (connector->currentModeBlobId)
        {
            drmModeDestroyPropertyBlob(connector->device->fd, connector->currentModeBlobId);
            connector->currentModeBlobId = 0;
        }

        drmModeCreatePropertyBlob(connector->device->fd,
                                  &connector->currentMode->info,
                                  sizeof(drmModeModeInfo),
                                  &connector->currentModeBlobId);

        drmModeAtomicAddProperty(req,
                                 connector->currentCrtc->id,
                                 connector->currentCrtc->propIDs.MODE_ID,
                                 connector->currentModeBlobId);

        drmModeAtomicAddProperty(req,
                                 connector->currentCrtc->id,
                                 connector->currentCrtc->propIDs.ACTIVE,
                                 1);

        // Connector

        drmModeAtomicAddProperty(req,
                                 connector->id,
                                 connector->propIDs.CRTC_ID,
                                 connector->currentCrtc->id);

        drmModeAtomicAddProperty(req,
                                 connector->id,
                                 connector->propIDs.link_status,
                                 DRM_MODE_LINK_STATUS_GOOD);

        // Plane

        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane->id,
                                 connector->currentPrimaryPlane->propIDs.CRTC_ID,
                                 connector->currentCrtc->id);

        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane->id,
                                 connector->currentPrimaryPlane->propIDs.CRTC_X,
                                 0);

        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane->id,
                                 connector->currentPrimaryPlane->propIDs.CRTC_Y,
                                 0);

        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane->id,
                                 connector->currentPrimaryPlane->propIDs.CRTC_W,
                                 connector->currentMode->info.hdisplay);

        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane->id,
                                 connector->currentPrimaryPlane->propIDs.CRTC_H,
                                 connector->currentMode->info.vdisplay);

        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane->id,
                                 connector->currentPrimaryPlane->propIDs.FB_ID,
                                 fb);

        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane->id,
                                 connector->currentPrimaryPlane->propIDs.SRC_X,
                                 0);

        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane->id,
                                 connector->currentPrimaryPlane->propIDs.SRC_Y,
                                 0);

        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane->id,
                                 connector->currentPrimaryPlane->propIDs.SRC_W,
                                 (UInt64)connector->currentMode->info.hdisplay << 16);

        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane->id,
                                 connector->currentPrimaryPlane->propIDs.SRC_H,
                                 (UInt64)connector->currentMode->info.vdisplay << 16);

        srmRenderModeCommitAtomicChanges(connector, req, 1);

        // Commit
        ret = srmRenderModeAtomicCommit(connector->device->fd,
                                        req,
                                        DRM_MODE_ATOMIC_ALLOW_MODESET,
                                        connector, 1);

        drmModeAtomicFree(req);

        if (ret)
        {
            SRMError("Failed to resume crtc mode on device %s connector %d.",
                     connector->device->name,
                     connector->id);
        }
    }
    else
    {
        ret = drmModeSetCrtc(connector->device->fd,
                                   connector->currentCrtc->id,
                                   fb,
                                   0,
                                   0,
                                   &connector->id,
                                   1,
                                   &connector->currentMode->info);

        if (ret)
        {
            SRMError("Failed to resume crtc mode on device %s connector %d.",
                     connector->device->name,
                     connector->id);
        }
    }
}

Int32 srmRenderModeCommonInitCrtc(SRMConnector *connector, UInt32 fb)
{
    Int32 ret;
    connector->lastFb = fb;

    if (connector->state == SRM_CONNECTOR_STATE_INITIALIZING)
    {
        connector->interface->initializeGL(connector,
                                           connector->interfaceData);
    }
    else if (connector->state == SRM_CONNECTOR_STATE_CHANGING_MODE)
    {
        connector->interface->resizeGL(connector,
                                       connector->interfaceData);
    }

    if (connector->device->clientCapAtomic)
    {
        drmModeAtomicReqPtr req;
        req = drmModeAtomicAlloc();

        if (connector->currentModeBlobId)
        {
            drmModeDestroyPropertyBlob(connector->device->fd, connector->currentModeBlobId);
            connector->currentModeBlobId = 0;
        }

        drmModeCreatePropertyBlob(connector->device->fd,
                                  &connector->currentMode->info,
                                  sizeof(drmModeModeInfo),
                                  &connector->currentModeBlobId);

        drmModeAtomicAddProperty(req,
                                 connector->currentCrtc->id,
                                 connector->currentCrtc->propIDs.MODE_ID,
                                 connector->currentModeBlobId);

        drmModeAtomicAddProperty(req,
                                 connector->currentCrtc->id,
                                 connector->currentCrtc->propIDs.ACTIVE,
                                 1);

        // Connector

        drmModeAtomicAddProperty(req,
                                 connector->id,
                                 connector->propIDs.CRTC_ID,
                                 connector->currentCrtc->id);

        drmModeAtomicAddProperty(req,
                                 connector->id,
                                 connector->propIDs.link_status,
                                 DRM_MODE_LINK_STATUS_GOOD);

        // Plane

        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane->id,
                                 connector->currentPrimaryPlane->propIDs.CRTC_ID,
                                 connector->currentCrtc->id);

        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane->id,
                                 connector->currentPrimaryPlane->propIDs.CRTC_X,
                                 0);

        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane->id,
                                 connector->currentPrimaryPlane->propIDs.CRTC_Y,
                                 0);

        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane->id,
                                 connector->currentPrimaryPlane->propIDs.CRTC_W,
                                 connector->currentMode->info.hdisplay);

        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane->id,
                                 connector->currentPrimaryPlane->propIDs.CRTC_H,
                                 connector->currentMode->info.vdisplay);

        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane->id,
                                 connector->currentPrimaryPlane->propIDs.FB_ID,
                                 fb);

        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane->id,
                                 connector->currentPrimaryPlane->propIDs.SRC_X,
                                 0);

        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane->id,
                                 connector->currentPrimaryPlane->propIDs.SRC_Y,
                                 0);

        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane->id,
                                 connector->currentPrimaryPlane->propIDs.SRC_W,
                                 (UInt64)connector->currentMode->info.hdisplay << 16);

        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane->id,
                                 connector->currentPrimaryPlane->propIDs.SRC_H,
                                 (UInt64)connector->currentMode->info.vdisplay << 16);

        srmRenderModeCommitAtomicChanges(connector, req, 1);

        // Commit
        ret = srmRenderModeAtomicCommit(connector->device->fd,
                                        req,
                                        DRM_MODE_ATOMIC_ALLOW_MODESET,
                                        connector, 1);

        drmModeAtomicFree(req);

        if (ret)
        {
            SRMError("Failed to set crtc mode on device %s connector %d (atomic).",
                    connector->device->name,
                    connector->id);
        }
        else
            return 1;
    }

    /* Occasionally, the Atomic API fails to set the connector CRTC for reasons unknown.
     * As a workaround, we enforce the use of the legacy API in this case. */

    ret = drmModeSetCrtc(connector->device->fd,
                    connector->currentCrtc->id,
                    fb,
                    0,
                    0,
                    &connector->id,
                    1,
                    &connector->currentMode->info);

    if (ret)
    {
        SRMError("Failed to set crtc mode on device %s connector %d.",
                    connector->device->name,
                    connector->id);
        return 0;
    }
    
    return 1;
}

void srmRenderModeCommonPageFlip(SRMConnector *connector, UInt32 fb)
{
    Int32 ret = 0;

    UInt32 buffersCount = srmConnectorGetBuffersCount(connector);

    if ((connector->pendingPageFlip && !connector->currentVSync) || buffersCount == 1 || buffersCount > 2)
        srmRenderModeCommonWaitPageFlip(connector, -1);

    UInt8 fbChanged = 0;

    if (fb != connector->lastFb || connector->firstPageFlip)
    {
        fbChanged = 1;
        connector->lastFb = fb;
    }

    if (connector->device->clientCapAtomic)
    {
        if (connector->currentVSync)
        {
            drmModeAtomicReqPtr req;
            req = drmModeAtomicAlloc();
            srmRenderModeCommitAtomicChanges(connector, req, 0);
            drmModeAtomicAddProperty(req,
                                     connector->currentPrimaryPlane->id,
                                     connector->currentPrimaryPlane->propIDs.FB_ID,
                                     connector->lastFb);
            ret = srmRenderModeAtomicCommit(connector->device->fd,
                                        req,
                                        DRM_MODE_PAGE_FLIP_EVENT | DRM_MODE_ATOMIC_NONBLOCK,
                                        connector, 0);
            if (ret == 0)
                connector->atomicChanges = 0;

            drmModeAtomicFree(req);
            connector->pendingPageFlip = 1;
        }
        else
        {
            if (fbChanged)
            {
                drmModeAtomicReqPtr req;
                req = drmModeAtomicAlloc();
                drmModeAtomicAddProperty(req,
                                         connector->currentPrimaryPlane->id,
                                         connector->currentPrimaryPlane->propIDs.FB_ID,
                                         connector->lastFb);
                ret = srmRenderModeAtomicCommit(connector->device->fd,
                                                req,
                                                DRM_MODE_PAGE_FLIP_ASYNC | DRM_MODE_ATOMIC_NONBLOCK,
                                                connector, 0);
                drmModeAtomicFree(req);
            }

            if (!connector->pendingPageFlip && (connector->atomicChanges || ret == -22))
            {
                drmModeAtomicReqPtr req;
                req = drmModeAtomicAlloc();
                srmRenderModeCommitAtomicChanges(connector, req, 0);

                UInt8 syncPageFlip = 0;

                // If async fails, fallback to sync
                if (ret == -22)
                {
                    syncPageFlip = 1;
                    drmModeAtomicAddProperty(req,
                                             connector->currentPrimaryPlane->id,
                                             connector->currentPrimaryPlane->propIDs.FB_ID,
                                             connector->lastFb);
                }

                ret = srmRenderModeAtomicCommit(connector->device->fd,
                                                req,
                                                DRM_MODE_PAGE_FLIP_EVENT | DRM_MODE_ATOMIC_NONBLOCK,
                                                connector, 0);

                drmModeAtomicFree(req);

                // Clear flags on success
                if (ret == 0)
                {
                    connector->pendingPageFlip = 1;
                    connector->atomicChanges = 0;

                    if (!syncPageFlip)
                        connector->onlyAsyncCursorUpdate = 1;
                }

            }

            if (!connector->pendingPageFlip)
                connector->drmEventCtx.page_flip_handler(-1, 0, 0, 0, connector);
        }
    }
    else
    {
        if (connector->currentVSync)
        {
            ret = drmModePageFlip(connector->device->fd,
                                  connector->currentCrtc->id,
                                  connector->lastFb,
                                  DRM_MODE_PAGE_FLIP_EVENT,
                                  connector);
            connector->pendingPageFlip = 1;
        }
        else
        {
            ret = drmModePageFlip(connector->device->fd,
                                  connector->currentCrtc->id,
                                  connector->lastFb,
                                  DRM_MODE_PAGE_FLIP_ASYNC,
                                  connector);

            connector->pendingPageFlip = 0;

            if (ret == -22)
            {
                ret = drmModePageFlip(connector->device->fd,
                                      connector->currentCrtc->id,
                                      connector->lastFb,
                                      DRM_MODE_PAGE_FLIP_EVENT,
                                      connector);
                connector->pendingPageFlip = 1;
            }

            if (!connector->pendingPageFlip)
                connector->drmEventCtx.page_flip_handler(-1, 0, 0, 0, connector);
        }
    }

    Int32 limit = -1;

    if (ret)
    {
        if (ret == - 16)
            limit = 1;
        else
            connector->pendingPageFlip = 0;
        SRMError("Failed to page flip on device %s connector %d. Error: %d.",
                 connector->device->name,
                 connector->id, ret);
    }

    if (buffersCount == 2 || connector->firstPageFlip)
    {
        connector->firstPageFlip = 0;
        srmRenderModeCommonWaitPageFlip(connector, limit);
    }
}

void srmRenderModeCommonWaitPageFlip(SRMConnector *connector, Int32 iterLimit)
{
    struct pollfd fds;
    fds.fd = connector->device->fd;
    fds.events = POLLIN;
    fds.revents = 0;

    if (connector->onlyAsyncCursorUpdate)
        iterLimit = 1;

    while (connector->pendingPageFlip)
    {
        if (connector->state != SRM_CONNECTOR_STATE_INITIALIZED || iterLimit == 0)
            break;

        // Prevent multiple threads invoking the drmHandleEvent at a time wich causes bugs
        // If more than 1 connector is requesting a page flip, both can be handled here
        // since the struct passed to drmHandleEvent is standard and could be handling events
        // from any connector (E.g. pageFlipHandler(conn1) or pageFlipHandler(conn2))
        pthread_mutex_lock(&connector->device->pageFlipMutex);

        // Double check if the pageflip was notified in another thread
        if (!connector->pendingPageFlip)
        {
            pthread_mutex_unlock(&connector->device->pageFlipMutex);
            break;
        }

        poll(&fds, 1, iterLimit == -1 ? 500 : 0);
        drmHandleEvent(fds.fd, &connector->drmEventCtx);

        if (iterLimit > 0)
            iterLimit--;

        pthread_mutex_unlock(&connector->device->pageFlipMutex);
    }
}

Int32 srmRenderModeAtomicResetConnectorProps(SRMConnector *connector)
{
    connector->pendingPageFlip = 1;
    srmRenderModeCommonWaitPageFlip(connector, 3);
    connector->pendingPageFlip = 0;

    Int32 ret;
    drmModeAtomicReqPtr req;
    req = drmModeAtomicAlloc();

    // Unset mode

    drmModeAtomicAddProperty(req,
                             connector->currentCrtc->id,
                             connector->currentCrtc->propIDs.MODE_ID,
                             0);

    drmModeAtomicAddProperty(req,
                             connector->currentCrtc->id,
                             connector->currentCrtc->propIDs.ACTIVE,
                             0);

    // Connector

    drmModeAtomicAddProperty(req,
                             connector->id,
                             connector->propIDs.CRTC_ID,
                             0);

    drmModeAtomicAddProperty(req,
                             connector->id,
                             connector->propIDs.link_status,
                             DRM_MODE_LINK_STATUS_BAD);

    // Plane

    drmModeAtomicAddProperty(req,
                             connector->currentPrimaryPlane->id,
                             connector->currentPrimaryPlane->propIDs.CRTC_ID,
                             0);

    drmModeAtomicAddProperty(req,
                             connector->currentPrimaryPlane->id,
                             connector->currentPrimaryPlane->propIDs.CRTC_X,
                             0);

    drmModeAtomicAddProperty(req,
                             connector->currentPrimaryPlane->id,
                             connector->currentPrimaryPlane->propIDs.CRTC_Y,
                             0);

    drmModeAtomicAddProperty(req,
                             connector->currentPrimaryPlane->id,
                             connector->currentPrimaryPlane->propIDs.CRTC_W,
                             0);

    drmModeAtomicAddProperty(req,
                             connector->currentPrimaryPlane->id,
                             connector->currentPrimaryPlane->propIDs.CRTC_H,
                             0);

    drmModeAtomicAddProperty(req,
                             connector->currentPrimaryPlane->id,
                             connector->currentPrimaryPlane->propIDs.FB_ID,
                             0);

    drmModeAtomicAddProperty(req,
                             connector->currentPrimaryPlane->id,
                             connector->currentPrimaryPlane->propIDs.SRC_X,
                             0);

    drmModeAtomicAddProperty(req,
                             connector->currentPrimaryPlane->id,
                             connector->currentPrimaryPlane->propIDs.SRC_Y,
                             0);

    drmModeAtomicAddProperty(req,
                             connector->currentPrimaryPlane->id,
                             connector->currentPrimaryPlane->propIDs.SRC_W,
                             0);

    drmModeAtomicAddProperty(req,
                             connector->currentPrimaryPlane->id,
                             connector->currentPrimaryPlane->propIDs.SRC_H,
                             0);

    srmRenderModeCommitAtomicChanges(connector, req, 1);

    ret = srmRenderModeAtomicCommit(connector->device->fd,
                                    req,
                                    DRM_MODE_ATOMIC_ALLOW_MODESET,
                                    connector, 1);

    drmModeAtomicFree(req);

    return ret;
}
