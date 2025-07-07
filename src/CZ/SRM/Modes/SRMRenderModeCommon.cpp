#if 1 == 2
#include <private/modes/SRMRenderModeCommon.h>
#include <private/SRMCorePrivate.h>
#include <private/SRMCrtcPrivate.h>
#include <private/SRMPlanePrivate.h>
#include <private/SRMDevicePrivate.h>
#include <private/SRMConnectorPrivate.h>
#include <private/SRMConnectorModePrivate.h>
#include <SRMList.h>

#include <SRMCore.h>
#include <SRMLog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xf86drmMode.h>
#include <poll.h>
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

Int8 srmRenderModeCommonChooseEGLConfiguration(EGLDisplay egl_display, const EGLint *attribs, EGLint visual_id, EGLConfig *config_out)
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
        config_index = 0;

    if (config_index == -1)
        config_index = srmRenderModeCommonMatchConfigToVisual(egl_display, visual_id, configs, matched);

    if (config_index != -1)
        *config_out = configs[config_index];

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

            if (periodUsec > 0)
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
    if (connector->device->driver == SRM_DEVICE_DRIVER_nvidia)
    {
        char *env = getenv("SRM_NVIDIA_CURSOR");

        if (!env || atoi(env) != 1)
            return 0;
    }

    if (connector->device->core->disableCursorPlanes)
        return 0;

    Int32 ret;

    connector->cursorIndex = 0;

    UInt8 blankBuffer[64*64*4];
    memset(blankBuffer, 0, sizeof(blankBuffer));
    UInt64 mod = DRM_FORMAT_MOD_LINEAR;

    for (int i = 0; i < 2; i++)
    {        
        connector->cursor[i].bo = gbm_bo_create(
            connector->device->gbm,
            64, 64,
            GBM_FORMAT_ARGB8888,
            GBM_BO_USE_CURSOR | GBM_BO_USE_WRITE);

        if (!connector->cursor[i].bo)
            connector->cursor[i].bo = gbm_bo_create_with_modifiers2(
                connector->device->gbm,
                64, 64,
                GBM_FORMAT_ARGB8888,
                &mod,
                1,
                GBM_BO_USE_CURSOR | GBM_BO_USE_WRITE);

        if (!connector->cursor[i].bo)
            goto fail;

        if (!connector->currentCursorPlane)
            continue;

        ret = drmModeAddFB(connector->device->fd,
                                 gbm_bo_get_width(connector->cursor[i].bo),
                                 gbm_bo_get_height(connector->cursor[i].bo),
                                 32,
                                 gbm_bo_get_bpp(connector->cursor[i].bo),
                                 gbm_bo_get_stride(connector->cursor[i].bo),
                                 gbm_bo_get_handle(connector->cursor[i].bo).u32,
                                 &connector->cursor[i].fb);

        if (ret)
            goto fail;
    }

    if (!connector->currentCursorPlane)
    {
        ret = drmModeSetCursor(connector->device->fd,
                               connector->currentCrtc->id,
                               gbm_bo_get_handle(connector->cursor[0].bo).u32,
                               64, 64);
        if (ret)
            goto fail;
    }

    return 1;

    fail:

    for (int i = 0; i < 2; i++)
    {
        if (connector->cursor[i].fb != 0)
        {
            drmModeRmFB(connector->device->fd, connector->cursor[i].fb);
            connector->cursor[i].fb = 0;
        }

        if (connector->cursor[i].bo)
        {
            gbm_bo_destroy(connector->cursor[i].bo);
            connector->cursor[i].bo = NULL;
        }
    }

    SRMError("[%s] [%s] Failed to create HW cursor.", connector->device->shortName, connector->name);
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
                             connector->fenceFD);
}

void srmRenderModeCommonDestroyCursor(SRMConnector *connector)
{
    if (connector->cursorVisible)
    {
        if (connector->currentCursorPlane)
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

    for (int i = 0; i < 2; i++)
    {
        if (connector->cursor[i].fb != 0)
        {
            drmModeRmFB(connector->device->fd, connector->cursor[i].fb);
            connector->cursor[i].fb = 0;
        }

        if (connector->cursor[i].bo)
        {
            gbm_bo_destroy(connector->cursor[i].bo);
            connector->cursor[i].bo = NULL;
        }
    }

    connector->atomicChanges &= ~(SRM_ATOMIC_CHANGE_CURSOR_VISIBILITY | SRM_ATOMIC_CHANGE_CURSOR_POSITION | SRM_ATOMIC_CHANGE_CURSOR_BUFFER);
    connector->cursorVisible = 0;
    connector->cursorIndex = 0;
}

Int32 srmRenderModeAtomicCommit(Int32 fd, drmModeAtomicReqPtr req, UInt32 flags, void *data, UInt8 forceRetry)
{
    if (!forceRetry)
        return drmModeAtomicCommit(fd, req, flags, data);

    Int32 ret;

    retry:
    ret = drmModeAtomicCommit(fd, req, flags | DRM_MODE_ATOMIC_TEST_ONLY, data);

    // -EBUSY
    if (ret == -16)
    {
        usleep(2000);
        goto retry;
    }

    return drmModeAtomicCommit(fd, req, flags, data);
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
                SRMError("[%s] [%s] Failed unset mode. DRM Error: %d. (atomic)",
                         connector->device->shortName,
                         connector->name, ret);
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

            UInt32 prevCursorIndex = connector->cursorIndex;
            srmRenderModeCommitAtomicChanges(connector, req, 0);

            ret = srmRenderModeAtomicCommit(connector->device->fd,
                                req,
                                DRM_MODE_ATOMIC_ALLOW_MODESET,
                                connector, 1);

            if (ret)
            {
                if (connector->currentCursorPlane)
                    connector->cursorIndex = prevCursorIndex;
                SRMError("[%s] [%s] Failed set mode with same size. DRM Error: %d. (atomic)",
                         connector->device->shortName,
                         connector->name, ret);
            }
            else
                connector->atomicChanges = 0;

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
                SRMError("[%s] [%s] Failed unset mode. DRM Error: %d. (legacy)",
                         connector->device->shortName,
                         connector->name, ret);
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
                SRMError("[%s] [%s] Failed unset mode. DRM Error: %d. (atomic)",
                         connector->device->shortName,
                         connector->name, ret);
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
                SRMError("[%s] [%s] Failed unset mode. DRM Error: %d. (legacy)",
                         connector->device->shortName,
                         connector->name, ret);
            }
        }

        return 1;
    }
}

void srmRenderModeCommonUninitialize(SRMConnector *connector)
{
    if (connector->state == SRM_CONNECTOR_STATE_INITIALIZING)
        return;

    Int32 ret;

    if (connector->device->clientCapAtomic)
    {
        ret = srmRenderModeAtomicResetConnectorProps(connector);

        if (ret)
        {
            SRMError("[%s] [%s] Failed to reset CRTC. DRM Error: %d. (atomic)",
                     connector->device->shortName,
                     connector->name, ret);
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
            SRMError("[%s] [%s] Failed to reset CRTC. DRM Error: %d. (legacy)",
                     connector->device->shortName,
                     connector->name, ret);
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
            SRMWarning("[%s] [%s] Failed to reset CRTC. DRM Error: %d (not DRM master). (atomic)",
                     connector->device->shortName,
                     connector->name, ret);
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
            SRMError("[%s] [%s] Failed to reset CRTC. DRM Error: %d (not DRM master). (legacy)",
                     connector->device->shortName,
                     connector->name, ret);
        }
    }
}

UInt8 srmRenderModeCommonResumeRendering(SRMConnector *connector, UInt32 fb)
{
    Int32 ret = -1;
    connector->lastFb = fb;
    srmRenderModeCommonSyncState(connector);

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

        UInt32 prevCursorIndex = connector->cursorIndex;
        srmRenderModeCommitAtomicChanges(connector, req, 0);

        ret = srmRenderModeAtomicCommit(connector->device->fd,
                                        req,
                                        DRM_MODE_ATOMIC_ALLOW_MODESET,
                                        connector, 1);

        drmModeAtomicFree(req);

        if (ret)
        {
            if (connector->currentCursorPlane)
                connector->cursorIndex = prevCursorIndex;

            SRMError("[%s] [%s] Failed to restore CRTC mode. DRM Error: %d.",
                     connector->device->shortName,
                     connector->name, ret);
        }
        else
        {
            connector->atomicChanges = 0;
            connector->pendingModeSetting = 0;
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
            SRMError("[%s] [%s] Failed to restore CRTC mode. DRM Error: %d.",
                     connector->device->shortName,
                     connector->name, ret);
        }
        else
            connector->pendingModeSetting = 0;
    }

    return ret == 0;
}


Int32 srmRenderModeCommonInitCrtc(SRMConnector *connector, UInt32 fb)
{
    Int32 ret;
    connector->lastFb = fb;

    if (connector->device->clientCapAtomic)
    {
        drmModeAtomicReqPtr req;
        req = drmModeAtomicAlloc();

        // Plane

        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane->id,
                                 connector->currentPrimaryPlane->propIDs.FB_ID,
                                 fb);

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

        // CRTC

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


        UInt32 prevCursorIndex = connector->cursorIndex;
        srmRenderModeCommitAtomicChanges(connector, req, 0);

        // Commit
        ret = srmRenderModeAtomicCommit(connector->device->fd,
                                        req,
                                        DRM_MODE_ATOMIC_ALLOW_MODESET,
                                        connector, 1);

        drmModeAtomicFree(req);

        if (ret)
        {
            if (connector->currentCursorPlane)
                connector->cursorIndex = prevCursorIndex;
            SRMError("[%s] [%s] Failed to set CRTC mode (atomic). DRM Error: %d.",
                    connector->device->shortName,
                    connector->name, ret);
        }
        else
        {
            connector->atomicChanges = 0;
            goto skipLegacy;
        }
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
        SRMError("[%s] [%s] Failed to set CRTC mode. DRM Error: %d.",
                    connector->device->shortName,
                    connector->name, ret);
        return 0;
    }

    skipLegacy:

    if (connector->state == SRM_CONNECTOR_STATE_INITIALIZING)
    {
        connector->interface->initializeGL(connector,
                                           connector->interfaceData);
        glFinish();
    }
    else if (connector->state == SRM_CONNECTOR_STATE_CHANGING_MODE)
    {
        connector->interface->resizeGL(connector,
                                       connector->interfaceData);
        glFinish();
    }
    
    return 1;
}

void srmRenderModeCommonPageFlip(SRMConnector *connector, UInt32 fb)
{
    Int32 ret = 0;

    UInt32 buffersCount = srmConnectorGetBuffersCount(connector);
    UInt8 customScanoutBuffer = connector->userScanoutBufferRef[0] != NULL;

    if (customScanoutBuffer || connector->pendingPageFlip || buffersCount == 1 || buffersCount > 2)
        srmRenderModeCommonWaitPageFlip(connector, -1);

    connector->lastFb = fb;

    if (connector->device->clientCapAtomic)
    {
        pthread_mutex_lock(&connector->propsMutex);

        if (connector->currentVSync)
        {
            drmModeAtomicReqPtr req;
            req = drmModeAtomicAlloc();

            UInt32 prevCursorIndex = connector->cursorIndex;
            srmRenderModeCommitAtomicChanges(connector, req, 0);
            drmModeAtomicAddProperty(req,
                                     connector->currentPrimaryPlane->id,
                                     connector->currentPrimaryPlane->propIDs.FB_ID,
                                     connector->lastFb);
            ret = srmRenderModeAtomicCommit(connector->device->fd,
                                        req,
                                        DRM_MODE_PAGE_FLIP_EVENT | DRM_MODE_ATOMIC_NONBLOCK,
                                        connector, 0);
            if (ret && connector->currentCursorPlane)
                connector->cursorIndex = prevCursorIndex;
            else
                connector->atomicChanges = 0;

            drmModeAtomicFree(req);
            connector->pendingPageFlip = 1;
        }
        else
        {
            if (connector->atomicChanges == 0)
            {
                drmModeAtomicReqPtr req;
                req = drmModeAtomicAlloc();
                drmModeAtomicAddProperty(req,
                                         connector->currentPrimaryPlane->id,
                                         connector->currentPrimaryPlane->propIDs.FB_ID,
                                         connector->lastFb);
                ret = srmRenderModeAtomicCommit(connector->device->fd,
                                                req,
                                                DRM_MODE_PAGE_FLIP_EVENT | DRM_MODE_PAGE_FLIP_ASYNC | DRM_MODE_ATOMIC_NONBLOCK,
                                                connector, 0);
                drmModeAtomicFree(req);
            }

            if (connector->atomicChanges || ret == -22)
            {
                drmModeAtomicReqPtr req;
                req = drmModeAtomicAlloc();
                UInt32 prevCursorIndex = connector->cursorIndex;
                srmRenderModeCommitAtomicChanges(connector, req, 0);
                drmModeAtomicAddProperty(req,
                                         connector->currentPrimaryPlane->id,
                                         connector->currentPrimaryPlane->propIDs.FB_ID,
                                         connector->lastFb);
                ret = srmRenderModeAtomicCommit(connector->device->fd,
                                                req,
                                                DRM_MODE_PAGE_FLIP_EVENT | DRM_MODE_ATOMIC_NONBLOCK,
                                                connector, 0);
                drmModeAtomicFree(req);

                if (ret && connector->currentCursorPlane)
                    connector->cursorIndex = prevCursorIndex;
                else
                    connector->atomicChanges = 0;
            }

            connector->pendingPageFlip = 1;
        }

        pthread_mutex_unlock(&connector->propsMutex);
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
        }
        else
        {
            ret = drmModePageFlip(connector->device->fd,
                                  connector->currentCrtc->id,
                                  connector->lastFb,
                                  DRM_MODE_PAGE_FLIP_ASYNC | DRM_MODE_PAGE_FLIP_EVENT,
                                  connector);

            if (ret == -22)
            {
                ret = drmModePageFlip(connector->device->fd,
                                      connector->currentCrtc->id,
                                      connector->lastFb,
                                      DRM_MODE_PAGE_FLIP_EVENT,
                                      connector);
            }
        }

        connector->pendingPageFlip = 1;
    }


    if (ret)
    {
        connector->pendingPageFlip = 0;
        SRMError("[%s] [%s] Failed to page flip. DRM Error: %d.",
                 connector->device->shortName,
                 connector->name, ret);

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
        }
    }

    if (customScanoutBuffer || buffersCount == 2 || connector->firstPageFlip)
    {
        connector->firstPageFlip = 0;
        srmRenderModeCommonWaitPageFlip(connector, -1);
    }
}

void srmRenderModeCommonWaitPageFlip(SRMConnector *connector, Int32 iterLimit)
{
    struct pollfd fds;
    fds.fd = connector->device->fd;
    fds.events = POLLIN;
    fds.revents = 0;

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

        poll(&fds, 1, iterLimit == -1 ? 500 : 1);
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

    srmRenderModeCommitAtomicChanges(connector, req, 0);

    ret = srmRenderModeAtomicCommit(connector->device->fd,
                                    req,
                                    DRM_MODE_ATOMIC_ALLOW_MODESET,
                                    connector, 1);

    drmModeAtomicFree(req);

    return ret;
}

/* This ensures properties are restored after resuming */
void srmRenderModeCommonSyncState(SRMConnector *connector)
{
    if (!connector->currentCrtc)
        return;

    if (connector->cursor[0].bo)
    {
        if (connector->currentCursorPlane)
            connector->atomicChanges |= SRM_ATOMIC_CHANGE_CURSOR_POSITION | SRM_ATOMIC_CHANGE_CURSOR_VISIBILITY;
        else
        {
            if (connector->cursorVisible)
                drmModeSetCursor(connector->device->fd,
                                 connector->currentCrtc->id,
                                 gbm_bo_get_handle(connector->cursor[connector->cursorIndex].bo).u32,
                                 64,
                                 64);
            else
                drmModeSetCursor(connector->device->fd,
                                 connector->currentCrtc->id,
                                 0,
                                 0,
                                 0);

            drmModeMoveCursor(connector->device->fd,
                              connector->currentCrtc->id,
                              connector->cursorX,
                              connector->cursorY);
        }
    }
    else
    {
        drmModeSetCursor(connector->device->fd,
                         connector->currentCrtc->id,
                         0,
                         0,
                         0);
    }

    if (connector->device->clientCapAtomic)
    {
        if (connector->propIDs.content_type)
            connector->atomicChanges |= SRM_ATOMIC_CHANGE_CONTENT_TYPE;

        if (connector->gamma)
            connector->atomicChanges |= SRM_ATOMIC_CHANGE_GAMMA_LUT;
    }
    else
    {
        if (connector->propIDs.content_type)
            drmModeConnectorSetProperty(connector->device->fd,
                                        connector->id,
                                        connector->propIDs.content_type,
                                        connector->contentType);

        /* This is always != NULL if gammaSize > 0 */
        if (connector->gamma)
        {
            UInt16 *table = (UInt16*)connector->gamma;
            UInt64 gammaSize = srmConnectorGetGammaSize(connector);
            if (drmModeCrtcSetGamma(connector->device->fd,
                                    connector->currentCrtc->id,
                                    (UInt32)gammaSize,
                                    table,
                                    table + gammaSize,
                                    table + gammaSize + gammaSize))
            {
                SRMError("[%s] [%s] Failed to set gamma using legacy API drmModeCrtcSetGamma().",
                         connector->device->shortName, connector->name);
            }
        }
    }
}

void srmRenderModeCommonSearchNonLinearModifier(SRMConnector *connector)
{
    connector->currentFormat.format = DRM_FORMAT_XRGB8888;
    connector->currentFormat.modifier = DRM_FORMAT_MOD_INVALID;

    if (!connector->device->clientCapAtomic || !connector->device->capAddFb2Modifiers || !connector->allowModifiers)
        return;

    UInt8 hasLinear = srmFormatIsInList(srmDeviceGetDMARenderFormats(connector->device), DRM_FORMAT_XRGB8888, DRM_FORMAT_MOD_LINEAR);

    SRMListForeach(it, connector->currentPrimaryPlane->inFormats)
    {
        SRMFormat *fmt = srmListItemGetData(it);

        if (fmt->format == DRM_FORMAT_XRGB8888
            && fmt->modifier != DRM_FORMAT_MOD_LINEAR
            && srmFormatIsInList(srmDeviceGetDMARenderFormats(connector->device), fmt->format, fmt->modifier))
        {
            SRMDebug("[%s] [%s] Using format: %s - %s.",
                connector->device->shortName,
                connector->name,
                drmGetFormatName(fmt->format),
                drmGetFormatModifierName(fmt->modifier));
            connector->currentFormat.modifier = fmt->modifier;
            break;
        }
    }

    if (connector->currentFormat.modifier == DRM_FORMAT_MOD_INVALID && hasLinear)
    {
        connector->currentFormat.modifier = DRM_FORMAT_MOD_LINEAR;
        SRMDebug("[%s] [%s] Using format: %s - %s.",
                 connector->device->shortName,
                 connector->name,
                 drmGetFormatName(connector->currentFormat.format),
                 drmGetFormatModifierName(connector->currentFormat.modifier));
    }
}

void srmRenderModeCommonCreateConnectorGBMSurface(SRMConnector *connector, struct gbm_surface **surface)
{
    if (connector->currentFormat.modifier != DRM_FORMAT_MOD_INVALID)
    {
        *surface = srmBufferCreateGBMSurface(
            connector->device->gbm,
            connector->currentMode->info.hdisplay,
            connector->currentMode->info.vdisplay,
            connector->currentFormat.format,
            connector->currentFormat.modifier,
            GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

        if (*surface)
            return;

        connector->currentFormat.modifier = DRM_FORMAT_MOD_INVALID;
    }

    *surface = srmBufferCreateGBMSurface(
        connector->device->gbm,
        connector->currentMode->info.hdisplay,
        connector->currentMode->info.vdisplay,
        GBM_FORMAT_XRGB8888,
        DRM_FORMAT_MOD_INVALID,
        GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
}

void srmRenderModeCommonCreateConnectorGBMBo(SRMConnector *connector, struct gbm_bo **bo)
{
    if (connector->currentFormat.modifier != DRM_FORMAT_MOD_INVALID)
    {
        *bo = srmBufferCreateGBMBo(
            connector->device->gbm,
            connector->currentMode->info.hdisplay,
            connector->currentMode->info.vdisplay,
            connector->currentFormat.format,
            connector->currentFormat.modifier,
            GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

        if (*bo)
            return;

        connector->currentFormat.modifier = DRM_FORMAT_MOD_INVALID;
    }

    *bo = srmBufferCreateGBMBo(
        connector->device->gbm,
        connector->currentMode->info.hdisplay,
        connector->currentMode->info.vdisplay,
        GBM_FORMAT_XRGB8888,
        connector->currentFormat.modifier,
        GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
}

Int32 srmRenderModeCommonCalculateBuffering(SRMConnector *connector, const char *modeName)
{
    char envName[48];
    sprintf(envName, "SRM_RENDER_MODE_%s_FB_COUNT", modeName);
    char *env = getenv(envName);
    Int32 buffering = 2;

    if (env)
    {
        Int32 c = atoi(env);

        if (c > 1 && c <= SRM_MAX_BUFFERING)
            buffering = c;
    }

    SRMDebug("[%s] [%s] [%s MODE] Buffering: %d.",
        connector->device->shortName, connector->name, modeName, buffering);

    return buffering;
}

void srmRenderModeCommonCreateSync(SRMConnector *connector)
{
    srmRenderModeCommonDestroySync(connector);

    SRMEGLDeviceFunctions *f = &connector->device->eglFunctions;

    if (!connector->device->clientCapAtomic || !connector->currentPrimaryPlane->propIDs.IN_FENCE_FD || !f->eglDupNativeFenceFDANDROID)
        goto fallback;

    static const EGLint attribs[] =
    {
        EGL_SYNC_NATIVE_FENCE_FD_ANDROID, EGL_NO_NATIVE_FENCE_FD_ANDROID,
        EGL_NONE,
    };

    EGLSyncKHR fence = f->eglCreateSyncKHR(connector->device->eglDisplay, EGL_SYNC_NATIVE_FENCE_ANDROID, attribs);

    if (fence == EGL_NO_SYNC_KHR)
        goto fallback;

    glFlush();
    connector->fenceFD = f->eglDupNativeFenceFDANDROID(connector->device->eglDisplay, fence);
    f->eglDestroySyncKHR(connector->device->eglDisplay, fence);

    if (connector->fenceFD != -1)
        return;

fallback:
    glFinish();
}

struct gbm_bo *srmRenderModeCommonSurfaceLockFrontBufferSafe(struct gbm_surface *surface)
{
    struct gbm_bo *bo = gbm_surface_lock_front_buffer(surface);

    if (bo)
        gbm_bo_set_user_data(bo, (void*)-1, NULL);

    return bo;
}

void srmRenderModeCommonSurfaceReleaseBufferSafe(struct gbm_surface *surface, struct gbm_bo *bo)
{
    if (gbm_bo_get_user_data(bo) == NULL)
        return;

    gbm_bo_set_user_data(bo, NULL, NULL);
    gbm_surface_release_buffer(surface, bo);
}

void srmRenderModeCommonDestroySync(SRMConnector *connector)
{
    SRMEGLDeviceFunctions *f = &connector->device->eglFunctions;

    if (!connector->device->clientCapAtomic || !f->eglDupNativeFenceFDANDROID)
        return;

    if (connector->fenceFD != -1)
    {
        close(connector->fenceFD);
        connector->fenceFD = -1;
    }
}

UInt8 srmRenderModeCommonCreateDRMFBsFromBOs(SRMConnector *connector, const char *mode, UInt32 count, struct gbm_bo **bos, UInt32 *fbs)
{
    Int32 ret;

    for (UInt32 i = 0; i < count; i++)
    {
        UInt32 boHandles[4] = { 0 };
        UInt32 pitches[4] = { 0 };
        UInt32 offsets[4] = { 0 };
        UInt64 mods[4] = { 0 };

        if (!bos[i])
            return 0;

        for (int plane = 0; plane < gbm_bo_get_plane_count(bos[i]); plane++)
        {
            boHandles[plane] = gbm_bo_get_handle_for_plane(bos[i], plane).u32;
            pitches[plane] = gbm_bo_get_stride_for_plane(bos[i], plane);
            offsets[plane] = gbm_bo_get_offset(bos[i], plane);
            mods[plane] = gbm_bo_get_modifier(bos[i]);
        }

        if (connector->currentFormat.modifier != DRM_FORMAT_MOD_INVALID)
        {
            ret = drmModeAddFB2WithModifiers(
                connector->device->fd,
                connector->currentMode->info.hdisplay,
                connector->currentMode->info.vdisplay,
                gbm_bo_get_format(bos[i]),
                boHandles,
                pitches,
                offsets,
                mods,
                &fbs[i],
                DRM_MODE_FB_MODIFIERS);

            if (ret)
            {
                SRMError("[%s] [%s] [%s MODE] Failed o create DRM framebuffer %d with drmModeAddFB2WithModifiers, trying drmModeAddFB2. DRM Error: %d.",
                         connector->device->shortName, connector->name, mode, i, ret);
            }
            else
                continue;
        }

        ret = drmModeAddFB2(
            connector->device->fd,
            connector->currentMode->info.hdisplay,
            connector->currentMode->info.vdisplay,
            gbm_bo_get_format(bos[i]),
            boHandles,
            pitches,
            offsets,
            &fbs[i],
            0);

        if (ret)
        {
            SRMError("[%s] [%s] [%s MODE] Failed o create DRM framebuffer %d with drmModeAddFB2, trying drmModeAddFB. DRM Error: %d.",
                     connector->device->shortName, connector->name, mode, i, ret);
        }
        else
            continue;

        ret = drmModeAddFB(
            connector->device->fd,
            connector->currentMode->info.hdisplay,
            connector->currentMode->info.vdisplay,
            24,
            gbm_bo_get_bpp(bos[i]),
            gbm_bo_get_stride(bos[i]),
            gbm_bo_get_handle(bos[i]).u32,
            &fbs[i]);

        if (ret)
        {
            SRMError("[%s] [%s] [%s MODE] Failed o create DRM framebuffer %d. DRM Error: %d.",
                     connector->device->shortName, connector->name, mode, i, ret);
            return 0;
        }
    }

    return 1;
}
#endif
