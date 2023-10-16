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

void srmRenderModeCommonPageFlipHandler(int a, unsigned int b, unsigned int c, unsigned int d, void *data)
{
    SRM_UNUSED(a);
    SRM_UNUSED(b);
    SRM_UNUSED(c);
    SRM_UNUSED(d);

    if (data)
    {
        SRMConnector *connector = data;
        connector->pendingPageFlip = 0;
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

    if ((!connector->repaintRequested && !connector->atomicCursorHasChanges) || srmCoreIsSuspended(connector->device->core))
    {
        connector->atomicCursorHasChanges = 0;
        pthread_cond_wait(&connector->repaintCond, &connector->repaintMutex);
    }

    pthread_mutex_unlock(&connector->repaintMutex);

    pthread_mutex_lock(&connector->stateMutex);
    if (connector->state == SRM_CONNECTOR_STATE_UNINITIALIZING)
    {
        pthread_mutex_unlock(&connector->stateMutex);
        connector->interface->uninitializeGL(connector, connector->interfaceData);
        connector->renderInterface.uninitialize(connector);
        pthread_mutex_lock(&connector->stateMutex);
        connector->state = SRM_CONNECTOR_STATE_UNINITIALIZED;
        pthread_mutex_unlock(&connector->stateMutex);
        return 0;
    }
    pthread_mutex_unlock(&connector->stateMutex);

    return 1;
}

void srmRenderModeCommitCursorChanges(SRMConnector *connector, drmModeAtomicReqPtr req)
{
    if (!connector->currentCursorPlane)
        return;

    UInt8 updatedFB = 0;

    if (connector->atomicCursorHasChanges & SRM_CURSOR_ATOMIC_CHANGE_BUFFER)
    {
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

    if (connector->atomicCursorHasChanges & SRM_CURSOR_ATOMIC_CHANGE_VISIBILITY)
    {
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

    if (connector->atomicCursorHasChanges & SRM_CURSOR_ATOMIC_CHANGE_POSITION)
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

    connector->atomicCursorHasChanges = 0;
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

    connector->atomicCursorHasChanges = 0;
    connector->cursorVisible = 0;
}

Int32 srmRenderModeAtomicCommit(Int32 fd, drmModeAtomicReqPtr req, UInt32 flags, void *data)
{
    Int32 ret;

    retry:
    ret = drmModeAtomicCommit(fd, req, flags, data);

    // -EBUSY
    if (ret == -16)
    {
        SRMError("[connector] Atomic commit failed. -EBUSY.");
        usleep(100);
        goto retry;
    }

    return ret;
}

Int32 srmRenderModeCommonUpdateMode(SRMConnector *connector, UInt32 fb)
{
    connector->lastFb = fb;

    if (connector->targetMode->info.hdisplay == connector->currentMode->info.hdisplay &&
        connector->targetMode->info.vdisplay == connector->currentMode->info.vdisplay)
    {
        connector->currentMode = connector->targetMode;

        if (connector->device->clientCapAtomic)
        {
            drmModeAtomicReqPtr req;
            req = drmModeAtomicAlloc();

            UInt32 modeID;

            // Unset mode

            drmModeCreatePropertyBlob(connector->device->fd,
                                      &connector->currentMode->info,
                                      0,
                                      &modeID);

            drmModeAtomicAddProperty(req,
                                     connector->currentCrtc->id,
                                     connector->currentCrtc->propIDs.MODE_ID,
                                     modeID);

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

            srmRenderModeCommitCursorChanges(connector, req);

            srmRenderModeAtomicCommit(connector->device->fd,
                                      req,
                                      DRM_MODE_ATOMIC_ALLOW_MODESET,
                                      connector);

            drmModeDestroyPropertyBlob(connector->device->fd, modeID);

            drmModeAtomicFree(req);

            // Set mode

            req = drmModeAtomicAlloc();

            drmModeCreatePropertyBlob(connector->device->fd,
                                      &connector->currentMode->info,
                                      sizeof(drmModeModeInfo),
                                      &modeID);

            drmModeAtomicAddProperty(req,
                                     connector->currentCrtc->id,
                                     connector->currentCrtc->propIDs.MODE_ID,
                                     modeID);

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

            srmRenderModeCommitCursorChanges(connector, req);

            srmRenderModeAtomicCommit(connector->device->fd,
                                req,
                                DRM_MODE_ATOMIC_ALLOW_MODESET,
                                connector);

            drmModeDestroyPropertyBlob(connector->device->fd, modeID);

            drmModeAtomicFree(req);
        }
        else
        {
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
            if (ret < 0)
                goto retry;
        }

        connector->interface->resizeGL(connector,
                                       connector->interfaceData);

        return 0;
    }
    else
    {
        if (connector->device->clientCapAtomic)
        {
            drmModeAtomicReqPtr req;
            req = drmModeAtomicAlloc();

            UInt32 modeID;

            drmModeCreatePropertyBlob(connector->device->fd,
                                      &connector->currentMode->info,
                                      0,
                                      &modeID);

            drmModeAtomicAddProperty(req,
                                     connector->currentCrtc->id,
                                     connector->currentCrtc->propIDs.MODE_ID,
                                     modeID);

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

            srmRenderModeCommitCursorChanges(connector, req);

            srmRenderModeAtomicCommit(connector->device->fd,
                                req,
                                DRM_MODE_ATOMIC_ALLOW_MODESET,
                                connector);

            drmModeDestroyPropertyBlob(connector->device->fd, modeID);

            drmModeAtomicFree(req);
        }
        else
        {
            drmModeSetCrtc(connector->device->fd,
                               connector->currentCrtc->id,
                               0,
                               0,
                               0,
                               NULL,
                               0,
                               NULL);
        }

        return 1;
    }
}

void srmRenderModeCommonUninitialize(SRMConnector *connector)
{
    if (connector->device->clientCapAtomic)
    {
        drmModeAtomicReqPtr req;
        req = drmModeAtomicAlloc();

        UInt32 modeID;

        drmModeCreatePropertyBlob(connector->device->fd,
                                  &connector->currentMode->info,
                                  0,
                                  &modeID);

        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane->id,
                                 connector->currentPrimaryPlane->propIDs.CRTC_ID,
                                 connector->currentCrtc->id);

        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane->id,
                                 connector->currentPrimaryPlane->propIDs.FB_ID,
                                 0);

        drmModeAtomicAddProperty(req,
                                 connector->currentCrtc->id,
                                 connector->currentCrtc->propIDs.MODE_ID,
                                 modeID);

        srmRenderModeCommitCursorChanges(connector, req);

        srmRenderModeAtomicCommit(connector->device->fd,
                            req,
                            DRM_MODE_ATOMIC_ALLOW_MODESET,
                            connector);

        drmModeDestroyPropertyBlob(connector->device->fd, modeID);

        drmModeAtomicFree(req);
    }
    else
    {
        drmModeSetCrtc(connector->device->fd,
                           connector->currentCrtc->id,
                           0,
                           0,
                           0,
                           NULL,
                           0,
                           NULL);
    }
}

void srmRenderModeCommonPauseRendering(SRMConnector *connector)
{
    if (connector->device->clientCapAtomic)
    {
        drmModeAtomicReqPtr req;
        req = drmModeAtomicAlloc();

        UInt32 modeID;

        drmModeCreatePropertyBlob(connector->device->fd,
                                  &connector->currentMode->info,
                                  0,
                                  &modeID);

        drmModeAtomicAddProperty(req,
                                 connector->currentCrtc->id,
                                 connector->currentCrtc->propIDs.MODE_ID,
                                 modeID);

        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane->id,
                                 connector->currentPrimaryPlane->propIDs.FB_ID,
                                 0);

        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane->id,
                                 connector->currentPrimaryPlane->propIDs.CRTC_ID,
                                 connector->currentCrtc->id);

        srmRenderModeCommitCursorChanges(connector, req);

        srmRenderModeAtomicCommit(connector->device->fd,
                            req,
                            DRM_MODE_ATOMIC_ALLOW_MODESET,
                            connector);

        drmModeDestroyPropertyBlob(connector->device->fd, modeID);

        drmModeAtomicFree(req);
    }
    else
    {
        drmModeSetCrtc(connector->device->fd,
                       connector->currentCrtc->id,
                       0,
                       0,
                       0,
                       NULL,
                       0,
                       NULL);
    }
}

void srmRenderModeCommonResumeRendering(SRMConnector *connector, UInt32 fb)
{
    connector->lastFb = fb;

    if (connector->device->clientCapAtomic)
    {
        drmModeAtomicReqPtr req;
        req = drmModeAtomicAlloc();

        UInt32 modeID;

        drmModeCreatePropertyBlob(connector->device->fd,
                                  &connector->currentMode->info,
                                  sizeof(drmModeModeInfo),
                                  &modeID);

        drmModeAtomicAddProperty(req,
                                 connector->currentCrtc->id,
                                 connector->currentCrtc->propIDs.MODE_ID,
                                 modeID);

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

        srmRenderModeCommitCursorChanges(connector, req);

        // Commit
        Int32 ret = srmRenderModeAtomicCommit(connector->device->fd,
                                        req,
                                        DRM_MODE_ATOMIC_ALLOW_MODESET,
                                        connector);

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
        Int32 ret = drmModeSetCrtc(connector->device->fd,
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

        UInt32 modeID;

        drmModeCreatePropertyBlob(connector->device->fd,
                                  &connector->currentMode->info,
                                  sizeof(drmModeModeInfo),
                                  &modeID);

        drmModeAtomicAddProperty(req,
                                 connector->currentCrtc->id,
                                 connector->currentCrtc->propIDs.MODE_ID,
                                 modeID);

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

        srmRenderModeCommitCursorChanges(connector, req);

        // Commit
        Int32 ret = srmRenderModeAtomicCommit(connector->device->fd,
                            req,
                            DRM_MODE_ATOMIC_ALLOW_MODESET,
                            connector);

        drmModeDestroyPropertyBlob(connector->device->fd, modeID);

        drmModeAtomicFree(req);

        if (ret)
        {
            SRMError("Failed to set crtc mode on device %s connector %d.",
                     connector->device->name,
                     connector->id);
            return 0;
        }
    }
    else
    {
        Int32 ret = drmModeSetCrtc(connector->device->fd,
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
    }

    return 1;
}

void srmRenderModeCommonPageFlip(SRMConnector *connector, UInt32 fb)
{
    Int32 ret;

    UInt32 buffersCount = srmConnectorGetBuffersCount(connector);

    if (buffersCount == 1 || buffersCount > 2)
    {
        struct pollfd fds;
        fds.fd = connector->device->fd;
        fds.events = POLLIN;
        fds.revents = 0;

        while(connector->pendingPageFlip)
        {
            if (connector->state != SRM_CONNECTOR_STATE_INITIALIZED)
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

            poll(&fds, 1, 500);

            if(fds.revents & POLLIN)
                drmHandleEvent(fds.fd, &connector->drmEventCtx);

            pthread_mutex_unlock(&connector->device->pageFlipMutex);
        }
    }

    connector->lastFb = fb;
    connector->pendingPageFlip = 1;

    if (connector->device->clientCapAtomic)
    {
        drmModeAtomicReqPtr req;
        req = drmModeAtomicAlloc();

        srmRenderModeCommitCursorChanges(connector, req);
        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane->id,
                                 connector->currentPrimaryPlane->propIDs.FB_ID,
                                 connector->lastFb);

        ret = srmRenderModeAtomicCommit(connector->device->fd,
                                  req,
                                  DRM_MODE_PAGE_FLIP_EVENT | DRM_MODE_ATOMIC_NONBLOCK,
                                  connector);
        drmModeAtomicFree(req);
    }
    else
    {
        ret = drmModePageFlip(connector->device->fd,
                        connector->currentCrtc->id,
                        connector->lastFb,
                        DRM_MODE_PAGE_FLIP_EVENT,
                        connector);
    }

    if (ret)
        connector->pendingPageFlip = 0;

    if (buffersCount == 2 || connector->firstPageFlip)
    {
        connector->firstPageFlip = 0;
        struct pollfd fds;
        fds.fd = connector->device->fd;
        fds.events = POLLIN;
        fds.revents = 0;

        while(connector->pendingPageFlip)
        {
            if (connector->state != SRM_CONNECTOR_STATE_INITIALIZED)
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

            poll(&fds, 1, 500);

            if(fds.revents & POLLIN)
                drmHandleEvent(fds.fd, &connector->drmEventCtx);

            pthread_mutex_unlock(&connector->device->pageFlipMutex);
        }
    }
}
