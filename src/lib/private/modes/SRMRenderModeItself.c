#include <private/modes/SRMRenderModeItself.h>
#include <private/modes/SRMRenderModeCommon.h>

#include <private/SRMConnectorPrivate.h>
#include <private/SRMConnectorModePrivate.h>
#include <private/SRMDevicePrivate.h>
#include <private/SRMCrtcPrivate.h>

#include <SRMLog.h>

#include <sys/poll.h>
#include <stdlib.h>

static const EGLint eglContextAttribs[] =
{
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE
};

static const EGLint eglConfigAttribs[] =
{
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_ALPHA_SIZE, 0,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_NONE
};

struct RenderModeDataStruct
{
    struct gbm_surface *connectorGBMSurface;
    struct gbm_bo *connectorBOs[2];
    UInt32 connectorDRMFramebuffers[2];
    UInt8 currentBufferIndex;
    EGLConfig connectorEGLConfig;
    EGLContext connectorEGLContext;
    EGLSurface connectorEGLSurface;
};

typedef struct RenderModeDataStruct RenderModeData;

static UInt8 setupData(SRMConnector *connector)
{
    RenderModeData *data = calloc(1, sizeof(RenderModeData));

    if (!data)
    {
        SRMError("Could not allocate data for device %s connector %d render mode (ITSELF MODE).",
                 connector->device->name,
                 connector->id);
        return 0;
    }

    connector->renderData = data;
    data->connectorEGLContext = EGL_NO_CONTEXT;
    data->connectorEGLSurface = EGL_NO_SURFACE;
    return 1;
}

static UInt8 createGBMSurfaces(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    data->connectorGBMSurface = gbm_surface_create(
        connector->device->gbm,
        connector->currentMode->info.hdisplay,
        connector->currentMode->info.vdisplay,
        GBM_FORMAT_XRGB8888,
        GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

    if (!data->connectorGBMSurface)
    {
        SRMError("Failed to create GBM surface for device %s connector %d (ITSELF MODE).",
                 connector->device->name, connector->id);
        return 0;
    }

    return 1;
}

static UInt8 getEGLConfiguration(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    if (!srmRenderModeCommonChooseEGLConfiguration(connector->device->eglDisplay,
                                eglConfigAttribs,
                                GBM_FORMAT_XRGB8888,
                                &data->connectorEGLConfig))
    {
        SRMError("Failed to choose EGL configuration for device %s connector %d (ITSELF MODE).",
                 connector->device->name,
                 connector->id);
        return 0;
    }

    return 1;
}

static UInt8 createEGLContext(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    data->connectorEGLContext = eglCreateContext(connector->device->eglDisplay,
                                                 data->connectorEGLConfig,
                                                 // It is EGL_NO_CONTEXT if no context was previously created in this device
                                                 connector->device->eglSharedContext,
                                                 eglContextAttribs);

    if (data->connectorEGLContext == EGL_NO_CONTEXT)
    {
        SRMError("Failed to create EGL context for device %s connector %d (ITSELF MODE).",
                 connector->device->name,
                 connector->id);
        return 0;
    }

    // Store it if is the first context created for this device, so that later on shared context can be created
    if (connector->device->eglSharedContext == EGL_NO_CONTEXT)
        connector->device->eglSharedContext = data->connectorEGLContext;

    return 1;
}

static UInt8 createEGLSurfaces(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    data->connectorEGLSurface = eglCreateWindowSurface(connector->device->eglDisplay,
                                                       data->connectorEGLConfig,
                                                       (EGLNativeWindowType)data->connectorGBMSurface,
                                                       NULL);

    if (data->connectorEGLSurface == EGL_NO_SURFACE)
    {
        SRMError("Failed to create EGL surface for device %s connector %d (ITSELF MODE).",
                 connector->device->name,
                 connector->id);
        return 0;
    }

    return 1;
}

static UInt8 createDRMFramebuffers(SRMConnector *connector)
{
    Int32 ret;

    RenderModeData *data = (RenderModeData*)connector->renderData;

    eglMakeCurrent(connector->device->eglDisplay,
                   data->connectorEGLSurface,
                   data->connectorEGLSurface,
                   data->connectorEGLContext);

    // 2nd buffer

    eglSwapBuffers(connector->device->eglDisplay, data->connectorEGLSurface);

    data->connectorBOs[1] = gbm_surface_lock_front_buffer(data->connectorGBMSurface);

    ret = drmModeAddFB(connector->device->fd,
                       connector->currentMode->info.hdisplay,
                       connector->currentMode->info.vdisplay,
                       24,
                       gbm_bo_get_bpp(data->connectorBOs[1]),
                       gbm_bo_get_stride(data->connectorBOs[1]),
                       gbm_bo_get_handle(data->connectorBOs[1]).u32,
                       &data->connectorDRMFramebuffers[1]);

    if (ret)
    {
        SRMError("Failed o create 2nd DRM framebuffer for device %s connector %d (ITSELF MODE).",
                 connector->device->name,
                 connector->id);
        return 0;
    }

    gbm_surface_release_buffer(data->connectorGBMSurface, data->connectorBOs[0]);

    // 1st buffer

    eglSwapBuffers(connector->device->eglDisplay, data->connectorEGLSurface);

    data->connectorBOs[0] = gbm_surface_lock_front_buffer(data->connectorGBMSurface);

    ret = drmModeAddFB(connector->device->fd,
                       connector->currentMode->info.hdisplay,
                       connector->currentMode->info.vdisplay,
                       24,
                       gbm_bo_get_bpp(data->connectorBOs[0]),
                       gbm_bo_get_stride(data->connectorBOs[0]),
                       gbm_bo_get_handle(data->connectorBOs[0]).u32,
                       &data->connectorDRMFramebuffers[0]);

    if (ret)
    {
        SRMError("Failed o create 1st DRM framebuffer for device %s connector %d (ITSELF MODE).",
                 connector->device->name,
                 connector->id);
        return 0;
    }

    gbm_surface_release_buffer(data->connectorGBMSurface, data->connectorBOs[1]);

    data->currentBufferIndex = 1;

    return 1;
}

static UInt8 render(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    eglMakeCurrent(connector->device->eglDisplay,
                   data->connectorEGLSurface,
                   data->connectorEGLSurface,
                   data->connectorEGLContext);

    connector->interface->paintGL(connector, connector->interfaceData);

    return 1;
}

static UInt8 flipPage(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    eglSwapBuffers(connector->device->eglDisplay, data->connectorEGLSurface);
    gbm_surface_lock_front_buffer(data->connectorGBMSurface);

    connector->pendingPageFlip = 1;

    drmModePageFlip(connector->device->fd,
                    connector->currentCrtc->id,
                    data->connectorDRMFramebuffers[data->currentBufferIndex],
                    DRM_MODE_PAGE_FLIP_EVENT,
                    connector);

    // Prevent multiple threads invoking the drmHandleEvent at a time wich causes bugs
    // If more than 1 connector is requesting a page flip, both can be handled here
    // since the struct passed to drmHandleEvent is standard and could be handling events
    // from any connector (E.g. pageFlipHandler(conn1) or pageFlipHandler(conn2))
    pthread_mutex_lock(&connector->device->pageFlipMutex);

    struct pollfd fds;
    fds.fd = connector->device->fd;
    fds.events = POLLIN;
    fds.revents = 0;

    while(connector->pendingPageFlip)
    {
        poll(&fds, 1, 100);

        if(fds.revents & POLLIN)
            drmHandleEvent(fds.fd, &connector->drmEventCtx);

        if(connector->state != SRM_CONNECTOR_STATE_INITIALIZED)
            break;
    }

    pthread_mutex_unlock(&connector->device->pageFlipMutex);

    data->currentBufferIndex = !data->currentBufferIndex;

    gbm_surface_release_buffer(data->connectorGBMSurface, data->connectorBOs[data->currentBufferIndex]);

    return 1;
}

static UInt8 initCrtc(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    connector->interface->initializeGL(connector,
                                       connector->interfaceData);

    eglSwapBuffers(connector->device->eglDisplay, data->connectorEGLSurface);
    gbm_surface_lock_front_buffer(data->connectorGBMSurface);

    Int32 ret = drmModeSetCrtc(connector->device->fd,
                   connector->currentCrtc->id,
                   data->connectorDRMFramebuffers[data->currentBufferIndex],
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

    data->currentBufferIndex = !data->currentBufferIndex;

    gbm_surface_release_buffer(data->connectorGBMSurface, data->connectorBOs[data->currentBufferIndex]);

    return 1;
}

static void uninitialize(SRMConnector *connector)
{
    if (connector->renderData)
    {
        RenderModeData *data = (RenderModeData*)connector->renderData;

        if (data->connectorEGLSurface != EGL_NO_SURFACE)
            eglDestroySurface(connector->device->eglDisplay, data->connectorEGLSurface);

        if (data->connectorEGLContext != EGL_NO_CONTEXT)
        {
            /* TODO: check if used as shared context on other connector */
        }

        if (data->connectorDRMFramebuffers[0])
            drmModeRmFB(connector->device->fd, data->connectorDRMFramebuffers[0]);

        if (data->connectorDRMFramebuffers[1])
            drmModeRmFB(connector->device->fd, data->connectorDRMFramebuffers[1]);

        if (data->connectorGBMSurface)
            gbm_surface_destroy(data->connectorGBMSurface);


        free(data);
        connector->renderData = NULL;
    }
}

static UInt8 initialize(SRMConnector *connector)
{
    if (!eglBindAPI(EGL_OPENGL_ES_API))
    {
        SRMError("Failed to bind GLES API for device %s connector %d (ITSELF MODE).",
                 connector->device->name,
                 connector->id);
        goto fail;
    }

    if (!setupData(connector))
        goto fail;

    if (!createGBMSurfaces(connector))
        goto fail;

    if (!getEGLConfiguration(connector))
        goto fail;

    if (!createEGLContext(connector))
        goto fail;

    if (!createEGLSurfaces(connector))
        goto fail;

    if (!createDRMFramebuffers(connector))
        goto fail;

    connector->state = SRM_CONNECTOR_STATE_INITIALIZED;

    if (!initCrtc(connector))
        goto fail;

    return 1;

fail:
    SRMError("Failed to initialize render mode ITSELF for device %s connector %d.",
             connector->device->name,
             connector->id);

    uninitialize(connector);
    return 0;
}

void srmRenderModeItselfSetInterface(SRMConnector *connector)
{
    connector->renderInterface.initialize = &initialize;
    connector->renderInterface.render = &render;
    connector->renderInterface.flipPage = &flipPage;
    connector->renderInterface.uninitialize = &uninitialize;
}
