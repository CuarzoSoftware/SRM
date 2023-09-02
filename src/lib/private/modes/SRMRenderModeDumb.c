#include <errno.h>
#include <private/modes/SRMRenderModeDumb.h>
#include <private/modes/SRMRenderModeCommon.h>

#include <private/SRMConnectorPrivate.h>
#include <private/SRMConnectorModePrivate.h>
#include <private/SRMDevicePrivate.h>
#include <private/SRMCrtcPrivate.h>
#include <private/SRMBufferPrivate.h>
#include <private/SRMPlanePrivate.h>

#include <SRMList.h>
#include <SRMLog.h>

#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <unistd.h>

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
    struct gbm_bo **connectorBOs;
    UInt32 *connectorDRMFramebuffers;
    SRMBuffer **buffers;

    struct drm_mode_create_dumb *dumbBuffers;
    struct drm_mode_map_dumb *dumbMapRequests;
    UInt8 **dumbMaps;

    UInt32 buffersCount;
    UInt32 currentBufferIndex;
    EGLConfig connectorEGLConfig;
    EGLContext connectorEGLContext;
    EGLSurface connectorEGLSurface;
};

typedef struct RenderModeDataStruct RenderModeData;

static UInt8 createData(SRMConnector *connector)
{
    if (connector->renderData)
    {
        // Already setup
        return 1;
    }

    RenderModeData *data = calloc(1, sizeof(RenderModeData));

    if (!data)
    {
        SRMError("Could not allocate data for device %s connector %d render mode (DUMB MODE).",
                 connector->device->name,
                 connector->id);
        return 0;
    }

    connector->renderData = data;
    data->connectorEGLContext = EGL_NO_CONTEXT;
    data->connectorEGLSurface = EGL_NO_SURFACE;
    return 1;
}

static void destroyData(SRMConnector *connector)
{
    if (connector->renderData)
    {
        free(connector->renderData);
        connector->renderData = NULL;
    }
}

static UInt8 createGBMSurfaces(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    if (data->connectorGBMSurface)
    {
        // Already created
        return 1;
    }

    data->connectorGBMSurface = gbm_surface_create(
        connector->device->rendererDevice->gbm,
        connector->currentMode->info.hdisplay,
        connector->currentMode->info.vdisplay,
        GBM_FORMAT_XRGB8888,
        GBM_BO_USE_RENDERING | GBM_BO_USE_LINEAR);

    if (!data->connectorGBMSurface)
    {
        data->connectorGBMSurface = gbm_surface_create(
            connector->device->rendererDevice->gbm,
            connector->currentMode->info.hdisplay,
            connector->currentMode->info.vdisplay,
            GBM_FORMAT_XRGB8888,
            GBM_BO_USE_RENDERING | GBM_BO_USE_SCANOUT);

        if (!data->connectorGBMSurface)
        {
            SRMError("Failed to create GBM surface for device %s connector %d (DUMB MODE).",
                     connector->device->name, connector->id);

            return 0;
        }
    }

    return 1;
}

static void destroyGBMSurfaces(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    if (data->connectorGBMSurface)
    {
        gbm_surface_destroy(data->connectorGBMSurface);
        data->connectorGBMSurface = NULL;
    }
}

static UInt8 getEGLConfiguration(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    if (!srmRenderModeCommonChooseEGLConfiguration(connector->device->rendererDevice->eglDisplay,
                                                   eglConfigAttribs,
                                                   GBM_FORMAT_XRGB8888,
                                                   &data->connectorEGLConfig))
    {
        SRMError("Failed to choose EGL configuration for device %s connector %d (DUMB MODE).",
                 connector->device->name,
                 connector->id);
        return 0;
    }

    return 1;
}

static UInt8 createEGLContext(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    if (data->connectorEGLContext != EGL_NO_CONTEXT)
    {
        // Already created
        return 1;
    }

    if (connector->device->rendererDevice->eglExtensions.IMG_context_priority)
        connector->device->rendererDevice->eglSharedContextAttribs[3] = EGL_CONTEXT_PRIORITY_HIGH_IMG;
    data->connectorEGLContext = eglCreateContext(connector->device->rendererDevice->eglDisplay,
                                                 data->connectorEGLConfig,
                                                 // It is EGL_NO_CONTEXT if no context was previously created in this device
                                                 connector->device->rendererDevice->eglSharedContext,
                                                 connector->device->rendererDevice->eglSharedContextAttribs);

    if (data->connectorEGLContext == EGL_NO_CONTEXT)
    {
        SRMError("Failed to create EGL context for device %s connector %d (DUMB MODE).",
                 connector->device->name,
                 connector->id);
        return 0;
    }

    // Store it if is the first context created for this device, so that later on shared context can be created
    if (connector->device->rendererDevice->eglSharedContext == EGL_NO_CONTEXT)
        connector->device->rendererDevice->eglSharedContext = data->connectorEGLContext;

    return 1;
}

static void destroyEGLContext(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    if (data->connectorEGLContext != EGL_NO_CONTEXT)
    {
        if (data->connectorEGLContext != connector->device->rendererDevice->eglSharedContext)
            eglDestroyContext(connector->device->rendererDevice->eglDisplay, data->connectorEGLContext);

        data->connectorEGLContext = EGL_NO_CONTEXT;
    }
}

static UInt8 createEGLSurfaces(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    if (data->connectorEGLSurface != EGL_NO_SURFACE)
    {
        // Already created
        return 1;
    }

    data->connectorEGLSurface = eglCreateWindowSurface(connector->device->rendererDevice->eglDisplay,
                                                       data->connectorEGLConfig,
                                                       (EGLNativeWindowType)data->connectorGBMSurface,
                                                       NULL);

    if (data->connectorEGLSurface == EGL_NO_SURFACE)
    {
        SRMError("Failed to create EGL surface for device %s connector %d (DUMB MODE).",
                 connector->device->name,
                 connector->id);
        return 0;
    }

    eglMakeCurrent(connector->device->rendererDevice->eglDisplay,
                   data->connectorEGLSurface,
                   data->connectorEGLSurface,
                   data->connectorEGLContext);

    SRMList *bos = srmListCreate();

    struct gbm_bo *bo = NULL;

    char *env = getenv("SRM_RENDER_MODE_DUMB_FB_COUNT");

    UInt32 fbCount = 1;

    if (env)
    {
        Int32 c = atoi(env);

        if (c > 0 && c < 4)
            fbCount = c;
    }

    while (srmListGetLength(bos) < fbCount && gbm_surface_has_free_buffers(data->connectorGBMSurface) > 0)
    {
        eglSwapBuffers(connector->device->rendererDevice->eglDisplay,
                       data->connectorEGLSurface);
        bo = gbm_surface_lock_front_buffer(data->connectorGBMSurface);
        srmListAppendData(bos, bo);
    }

    data->buffersCount = srmListGetLength(bos);

    if (data->buffersCount == 0)
    {
        srmListDestroy(bos);
        SRMError("Failed to get BOs from gbm_surface for connector %d device %s.", connector->id, connector->device->name);
        return 0;
    }

    data->connectorBOs = calloc(data->buffersCount, sizeof(struct gbm_bo*));

    UInt32 i = 0;
    SRMListForeach(boIt, bos)
    {
        struct gbm_bo *b = srmListItemGetData(boIt);
        gbm_surface_release_buffer(data->connectorGBMSurface, b);
        data->connectorBOs[i] = b;
        i++;
    }

    srmListDestroy(bos);
    return 1;
}

static void destroyEGLSurfaces(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    if (data->connectorBOs)
    {
        for (UInt32 i = 0; i < data->buffersCount; i++)
        {
            if(data->connectorBOs[i])
            {
                gbm_surface_release_buffer(data->connectorGBMSurface, data->connectorBOs[i]);
                data->connectorBOs[i] = NULL;
            }
        }

        free(data->connectorBOs);
        data->connectorBOs = NULL;
    }

    if (data->connectorEGLSurface != EGL_NO_SURFACE)
    {
        eglDestroySurface(connector->device->rendererDevice->eglDisplay, data->connectorEGLSurface);
        data->connectorEGLSurface = EGL_NO_SURFACE;
    }
}

static UInt8 swapBuffers(SRMConnector *connector, EGLDisplay display, EGLSurface surface)
{
    SRM_UNUSED(connector);
    return eglSwapBuffers(display, surface);
}

static UInt8 createDumbBuffers(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    if (data->dumbBuffers)
    {
        // Already created
        return 1;
    }

    Int32 ret;

    data->dumbBuffers = calloc(data->buffersCount, sizeof(struct drm_mode_create_dumb));
    data->dumbMapRequests = calloc(data->buffersCount, sizeof(struct drm_mode_map_dumb));
    data->dumbMaps = calloc(data->buffersCount, sizeof(UInt8 *));

    for (UInt32 i = 0; i < data->buffersCount; i++)
    {
        data->dumbBuffers[i].height = gbm_bo_get_height(data->connectorBOs[i]);
        data->dumbBuffers[i].width  = gbm_bo_get_width(data->connectorBOs[i]);
        data->dumbBuffers[i].bpp    = gbm_bo_get_bpp(data->connectorBOs[i]);

        ret = ioctl(connector->device->fd,
                    DRM_IOCTL_MODE_CREATE_DUMB,
                    &data->dumbBuffers[i]);

        if (ret)
        {
            SRMError("Failed to create dumb buffer %d for connector %d.", i, connector->id);
            return 0;
        }

        data->dumbMapRequests[i].handle = data->dumbBuffers[i].handle;

        ret = ioctl(connector->device->fd, DRM_IOCTL_MODE_MAP_DUMB, &data->dumbMapRequests[i]);

        if (ret)
        {
            SRMError("Failed to get dumb buffer %d map offset for connector %d (DUMB MODE): %s.", i, connector->id, strerror(errno));
            return 0;
        }

        data->dumbMaps[i] = (UInt8*)mmap(0,
                                       data->dumbBuffers[i].size,
                                       PROT_READ | PROT_WRITE,
                                       MAP_SHARED,
                                       connector->device->fd,
                                       data->dumbMapRequests[i].offset);

        if (data->dumbMaps[i] == NULL || data->dumbMaps[i] == MAP_FAILED)
        {
            SRMError("Failed to map the dumb buffer %d FD for connector %d (DUMB MODE).", i, connector->id);
            return 0;
        }
    }

    return 1;
}

static void destroyDumbBuffers(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    if (data->dumbMaps)
    {
        for (UInt32 i = 0; i < data->buffersCount; i++)
        {
            if (data->dumbMaps[i] != NULL && data->dumbMaps[i] != NULL)
                munmap(data->dumbMaps[i], data->dumbBuffers[i].size);
        }
        free(data->dumbMaps);
        data->dumbMaps = NULL;
    }

    if (data->dumbMapRequests)
    {
        free(data->dumbMapRequests);
        data->dumbMapRequests = NULL;
    }

    if (data->dumbBuffers)
    {
        for (UInt32 i = 0; i < data->buffersCount; i++)
        {
            if (data->dumbBuffers[i].handle)
            {
                ioctl(connector->device->fd,
                      DRM_IOCTL_MODE_DESTROY_DUMB,
                      &data->dumbBuffers[i].handle);
            }
        }

        free(data->dumbBuffers);
        data->dumbBuffers = NULL;
    }
}

static UInt8 createDRMFramebuffers(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    if (data->connectorDRMFramebuffers)
    {
        // Already created
        return 1;
    }

    Int32 ret;
    data->connectorDRMFramebuffers = calloc(data->buffersCount, sizeof(UInt32));

    if (!data->buffers)
        data->buffers = calloc(data->buffersCount, sizeof(SRMBuffer*));

    for (UInt32 i = 0; i < data->buffersCount; i++)
    {
        ret = drmModeAddFB(connector->device->fd,
                           data->dumbBuffers[i].width,
                           data->dumbBuffers[i].height,
                           24,
                           data->dumbBuffers[i].bpp,
                           data->dumbBuffers[i].pitch,
                           data->dumbBuffers[i].handle,
                           &data->connectorDRMFramebuffers[i]);

        if (ret)
        {
            SRMError("Failed o create DRM framebuffer %d for device %s connector %d (DUMB MODE).",
                     i,
                     connector->device->name,
                     connector->id);
            return 0;
        }

        data->buffers[i] = srmBufferCreateFromGBM(connector->device->rendererDevice->core, data->connectorBOs[i]);

        if (!data->buffers[i])
        {
            SRMWarning("Failed o create buffer %d from GBM bo for device %s connector %d (DUMB MODE).",
                       i,
                       connector->device->name,
                       connector->id);
        }
        else
        {
            if (srmBufferGetTextureID(connector->device, data->buffers[i]) == 0)
            {
                SRMWarning("Failed o get texture ID from buffer %d on device %s connector %d (DUMB MODE).",
                           i,
                           connector->device->name,
                           connector->id);
            }
        }
    }

    data->currentBufferIndex = 0;

    return 1;
}

static void destroyDRMFramebuffers(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    if (data->connectorDRMFramebuffers)
    {
        for (UInt32 i = 0; i < data->buffersCount; i++)
        {
            if (data->connectorDRMFramebuffers[i] != 0)
            {
                drmModeRmFB(connector->device->fd, data->connectorDRMFramebuffers[i]);
                data->connectorDRMFramebuffers[i] = 0;
            }
        }

        free(data->connectorDRMFramebuffers);
        data->connectorDRMFramebuffers = NULL;
    }

    if (data->buffers)
    {
        for (UInt32 i = 0; i < data->buffersCount; i++)
        {
            if (data->buffers[i])
            {
                srmBufferDestroy(data->buffers[i]);
                data->buffers[i] = NULL;
            }
        }

        free(data->buffers);
        data->buffers = NULL;
    }

    data->currentBufferIndex = 0;
}

static UInt32 nextBufferIndex(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;
    if (data->currentBufferIndex + 1 == data->buffersCount)
        return 0;
    else
        return data->currentBufferIndex + 1;
}

static UInt8 render(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    eglMakeCurrent(connector->device->rendererDevice->eglDisplay,
                   data->connectorEGLSurface,
                   data->connectorEGLSurface,
                   data->connectorEGLContext);

    connector->interface->paintGL(connector, connector->interfaceData);

    return 1;
}

static UInt8 flipPage(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    UInt32 b = data->currentBufferIndex;
    UInt32 h = data->dumbBuffers[b].height;
    UInt32 w = data->dumbBuffers[b].width;
    UInt32 p = data->dumbBuffers[b].pitch;
    UInt32 s = data->dumbBuffers[b].bpp/8;

    UInt8 useBufferRead = data->buffers[b] && data->buffers[b]->map && data->buffers[b]->modifiers[0] == DRM_FORMAT_MOD_LINEAR;

    if (useBufferRead)
        goto skipGLRead;

    if (connector->damageRectsCount > 0)
    {
        SRMRect *rect;

        for (Int32 r = 0; r < connector->damageRectsCount; r++)
        {
            rect = &connector->damageRects[r];

            for (Int32 i = rect->y; i < rect->y + rect->height; i++)
            {
                glReadPixels(rect->x,
                             h-i-1,
                             rect->width,
                             1,
                             GL_BGRA_EXT,
                             GL_UNSIGNED_BYTE,
                             &data->dumbMaps[b][i*p + s*rect->x]);
            }
        }

        connector->damageRectsCount = 0;
        free(connector->damageRects);
        connector->damageRects = NULL;
    }
    else
    {
        for (UInt32 i = 0; i < h; i++)
        {
            glReadPixels(0,
                         h-i-1,
                         w,
                         1,
                         GL_BGRA_EXT,
                         GL_UNSIGNED_BYTE,
                         &data->dumbMaps[b][i*p]);
        }
    }

    skipGLRead:

    swapBuffers(connector, connector->device->rendererDevice->eglDisplay, data->connectorEGLSurface);

    if (useBufferRead)
    {
        if (connector->damageRectsCount > 0)
        {
            SRMRect *rect;

            for (Int32 r = 0; r < connector->damageRectsCount; r++)
            {
                rect = &connector->damageRects[r];

                srmBufferRead(data->buffers[b],rect->x, rect->y, rect->width, rect->height,
                              rect->x, rect->y, data->dumbBuffers[b].pitch, data->dumbMaps[b]);
            }

            connector->damageRectsCount = 0;
            free(connector->damageRects);
            connector->damageRects = NULL;
        }
        else
        {
            srmBufferRead(data->buffers[b], 0, 0, data->dumbBuffers[b].width, data->dumbBuffers[b].height,
                          0, 0, data->dumbBuffers[b].pitch, data->dumbMaps[b]);
        }
    }

    gbm_surface_lock_front_buffer(data->connectorGBMSurface);

    srmRenderModeCommonPageFlip(connector, data->connectorDRMFramebuffers[data->currentBufferIndex]);

    data->currentBufferIndex = nextBufferIndex(connector);
    gbm_surface_release_buffer(data->connectorGBMSurface, data->connectorBOs[data->currentBufferIndex]);
    connector->interface->pageFlipped(connector, connector->interfaceData);
    return 1;
}

static UInt8 initCrtc(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;
    return srmRenderModeCommonInitCrtc(connector, data->connectorDRMFramebuffers[data->currentBufferIndex]);
}

static void uninitialize(SRMConnector *connector)
{
    if (connector->renderData)
    {
        srmRenderModeCommonUninitialize(connector);
        destroyEGLSurfaces(connector);
        destroyEGLContext(connector);
        destroyDRMFramebuffers(connector);
        destroyDumbBuffers(connector);
        destroyGBMSurfaces(connector);
        destroyData(connector);
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

    if (!createData(connector))
        goto fail;

    if (!getEGLConfiguration(connector))
        goto fail;

    if (!createEGLContext(connector))
        goto fail;

    if (!createGBMSurfaces(connector))
        goto fail;

    if (!createEGLSurfaces(connector))
        goto fail;

    if (!createDumbBuffers(connector))
        goto fail;

    if (!createDRMFramebuffers(connector))
        goto fail;

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

static UInt8 updateMode(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    // If ret = 1 then new mode dimensions != prev mode dimensions
    if (srmRenderModeCommonUpdateMode(connector, data->connectorDRMFramebuffers[nextBufferIndex(connector)]))
    {
        eglMakeCurrent(connector->device->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, connector->device->eglSharedContext);
        destroyDRMFramebuffers(connector);
        destroyDumbBuffers(connector);
        destroyEGLSurfaces(connector);
        destroyGBMSurfaces(connector);
        connector->currentMode = connector->targetMode;
        return initialize(connector);
    }

    return 1;
}

static UInt32 getCurrentBufferIndex(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;
    return data->currentBufferIndex;
}

static UInt32 getBuffersCount(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;
    return data->buffersCount;
}

static SRMBuffer *getBuffer(SRMConnector *connector, UInt32 bufferIndex)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    if (bufferIndex >= data->buffersCount)
        return NULL;

    return data->buffers[bufferIndex];
}

static void pauseRendering(SRMConnector *connector)
{
    srmRenderModeCommonPauseRendering(connector);
}

static void resumeRendering(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;
    srmRenderModeCommonResumeRendering(connector, data->connectorDRMFramebuffers[data->currentBufferIndex]);
}

void srmRenderModeDumbSetInterface(SRMConnector *connector)
{
    connector->renderInterface.initialize = &initialize;
    connector->renderInterface.render = &render;
    connector->renderInterface.flipPage = &flipPage;
    connector->renderInterface.updateMode = &updateMode;
    connector->renderInterface.getCurrentBufferIndex = &getCurrentBufferIndex;
    connector->renderInterface.getBuffersCount = &getBuffersCount;
    connector->renderInterface.getBuffer = &getBuffer;
    connector->renderInterface.uninitialize = &uninitialize;
    connector->renderInterface.pause = &pauseRendering;
    connector->renderInterface.resume = &resumeRendering;
}
