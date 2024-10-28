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

#define MODE_NAME "DUMB"

typedef struct RenderModeDataStruct
{
    RenderModeCommonData c;
    struct drm_mode_create_dumb *dumbBuffers;
    struct drm_mode_map_dumb *dumbMapRequests;
    UInt8 **dumbMaps;
    GLuint fallbackTextures[SRM_MAX_BUFFERING];
} RenderModeData;

static UInt8 createData(SRMConnector *connector)
{
    if (connector->renderData)
        return 1; // Already setup

    RenderModeData *data = calloc(1, sizeof(RenderModeData));

    if (!data)
    {
        SRMError("[%s] [%s] [%s MODE] Could not allocate render mode data.",
                 connector->device->shortName, connector->name, MODE_NAME);
        return 0;
    }

    connector->renderData = data;
    data->c.rendererContext = EGL_NO_CONTEXT;
    connector->currentFormat.format = DRM_FORMAT_XRGB8888;
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

static UInt8 getEGLConfiguration(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    if (!srmRenderModeCommonChooseEGLConfiguration(connector->device->rendererDevice->eglDisplay,
                                                   commonEGLConfigAttribs,
                                                   GBM_FORMAT_XRGB8888,
                                                   &data->c.rendererConfig))
    {
        SRMError("[%s] [%s] [%s MODE] Failed to choose EGL configuration.",
                 connector->device->rendererDevice->shortName, connector->name, MODE_NAME);
        return 0;
    }

    return 1;
}

static UInt8 createEGLContext(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    if (data->c.rendererContext != EGL_NO_CONTEXT)
        return 1; // Already created

    SRMDevice *dev = connector->device->rendererDevice;

    if (dev->eglExtensions.IMG_context_priority)
        dev->eglSharedContextAttribs[3] = EGL_CONTEXT_PRIORITY_HIGH_IMG;

    data->c.rendererContext = eglCreateContext(
        dev->eglDisplay,
        data->c.rendererConfig,
        dev->eglSharedContext,
        dev->eglSharedContextAttribs);

    if (data->c.rendererContext == EGL_NO_CONTEXT)
    {
        SRMError("[%s] [%s] [%s MODE] Failed to create EGL context.",
                 connector->device->shortName, connector->name, MODE_NAME);
        return 0;
    }

    EGLint priority = EGL_CONTEXT_PRIORITY_MEDIUM_IMG;

    if (dev->eglExtensions.IMG_context_priority)
        eglQueryContext(connector->device->eglDisplay, data->c.rendererContext, EGL_CONTEXT_PRIORITY_LEVEL_IMG, &priority);

    SRMDebug("[%s] [%s] [%s MODE] Using EGL context priority: %s.",
             dev->shortName, connector->name, MODE_NAME, srmEGLGetContextPriorityString(priority));

    eglMakeCurrent(dev->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, data->c.rendererContext);

    return 1;
}

static void destroyEGLContext(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;
    SRMDevice *dev = connector->device->rendererDevice;

    if (data->c.rendererContext != EGL_NO_CONTEXT)
    {
        eglMakeCurrent(dev->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(dev->eglDisplay, data->c.rendererContext);
        data->c.rendererContext = EGL_NO_CONTEXT;
    }
}

static void destroyRendererBuffers(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;
    eglMakeCurrent(connector->device->rendererDevice->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, data->c.rendererContext);

    for (UInt32 i = 0; i < data->c.buffersCount; i++)
    {
        if (data->c.rendererFBs[i])
        {
            glDeleteFramebuffers(1, &data->c.rendererFBs[i]);
            data->c.rendererFBs[i] = 0;
        }

        if (data->c.rendererRBs[i])
        {
            glDeleteRenderbuffers(1, &data->c.rendererRBs[i]);
            data->c.rendererRBs[i] = 0;
        }

        if (data->fallbackTextures[i])
        {
            glDeleteTextures(1, &data->fallbackTextures[i]);
            data->fallbackTextures[i] = 0;
        }

        if (data->c.rendererBOWrappers[i])
        {
            srmBufferDestroy(data->c.rendererBOWrappers[i]);
            data->c.rendererBOWrappers[i] = NULL;
        }

        if (data->c.rendererBOs[i])
        {
            gbm_bo_destroy(data->c.rendererBOs[i]);
            data->c.rendererBOs[i] = NULL;
        }
    }
}

static UInt8 createRendererBuffers(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;
    SRMDevice *dev = connector->device->rendererDevice;

    if (data->c.rendererFBs[0])
        return 1; // Already created

    data->c.buffersCount = srmRenderModeCommonCalculateBuffering(connector, MODE_NAME);
    eglMakeCurrent(dev->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, data->c.rendererContext);

    // Try linear first

    connector->currentFormat.modifier = DRM_FORMAT_MOD_LINEAR;

    for (UInt32 i = 0; i < data->c.buffersCount; i++)
    {
        data->c.rendererBOs[i] = srmBufferCreateLinearBO(
            dev->gbm,
            connector->currentMode->info.hdisplay,
            connector->currentMode->info.vdisplay,
            connector->currentFormat.format);

        if (!data->c.rendererBOs[i])
        {
            SRMError("[%s] [%s] [%s MODE] Failed to create linear gbm_bo for renderbuffer %d.",
                     dev->shortName, connector->name, MODE_NAME, i);
            goto nonLinearFallback;
        }

        if (!srmBufferCreateRBFromBO(dev->core, data->c.rendererBOs[i], &data->c.rendererFBs[i], &data->c.rendererRBs[i], &data->c.rendererBOWrappers[i]))
        {
            SRMError("[%s] [%s] [%s MODE] Failed to create linear renderbuffer %d.",
                     dev->shortName, connector->name, MODE_NAME, i);
            goto nonLinearFallback;
        }
    }

    return 1;

nonLinearFallback:
    destroyRendererBuffers(connector);
    SRMListForeach(fmtIt, dev->dmaRenderFormats)
    {
        SRMFormat *fmt = srmListItemGetData(fmtIt);

        if (fmt->format == connector->currentFormat.format && fmt->modifier != DRM_FORMAT_MOD_LINEAR)
        {
            connector->currentFormat.modifier = fmt->modifier;
            break;
        }
    }

    if (connector->currentFormat.modifier == DRM_FORMAT_MOD_LINEAR)
        goto glesFallback;

    for (UInt32 i = 0; i < data->c.buffersCount; i++)
    {
        data->c.rendererBOs[i] = srmBufferCreateGBMBo(
            dev->gbm,
            connector->currentMode->info.hdisplay,
            connector->currentMode->info.vdisplay,
            GBM_FORMAT_XRGB8888,
            connector->currentFormat.modifier,
            GBM_BO_USE_RENDERING);

        if (!data->c.rendererBOs[i])
        {
            SRMError("[%s] [%s] [%s MODE] Failed to create gbm_bo for renderbuffer %d.",
                     dev->shortName, connector->name, MODE_NAME, i);
            goto glesFallback;
        }

        if (!srmBufferCreateRBFromBO(dev->core, data->c.rendererBOs[i], &data->c.rendererFBs[i], &data->c.rendererRBs[i], &data->c.rendererBOWrappers[i]))
        {
            SRMError("[%s] [%s] [%s MODE] Failed to create create renderbuffer %d.",
                     dev->shortName, connector->name, MODE_NAME, i);
            goto glesFallback;
        }
    }

    return 1;

glesFallback:
    connector->currentFormat.modifier = DRM_FORMAT_MOD_INVALID;
    destroyRendererBuffers(connector);

    for (UInt32 i = 0; i < data->c.buffersCount; i++)
    {
        glGenFramebuffers(1, &data->c.rendererFBs[i]);

        if (data->c.rendererFBs[i] == 0)
            goto fail;

        glBindFramebuffer(GL_FRAMEBUFFER, data->c.rendererFBs[i]);
        glGenTextures(1, &data->fallbackTextures[i]);

        if (data->fallbackTextures[i] == 0)
            goto fail;

        glBindTexture(GL_TEXTURE_2D, data->fallbackTextures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                     connector->currentMode->info.hdisplay,
                     connector->currentMode->info.vdisplay,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, data->fallbackTextures[i], 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            goto fail;
    }

    return 1;

fail:
    SRMError("[%s] [%s] [%s MODE] Failed to create renderbuffers.",
             dev->shortName, connector->name, MODE_NAME);

    return 0;
}

static UInt8 createDumbBuffers(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    if (data->dumbBuffers)
        return 1; // Already created

    Int32 ret;
    SRMDevice *dev = connector->device;
    data->dumbBuffers = calloc(data->c.buffersCount, sizeof(struct drm_mode_create_dumb));
    data->dumbMapRequests = calloc(data->c.buffersCount, sizeof(struct drm_mode_map_dumb));
    data->dumbMaps = calloc(data->c.buffersCount, sizeof(UInt8 *));

    for (UInt32 i = 0; i < data->c.buffersCount; i++)
    {
        data->dumbBuffers[i].width  = connector->currentMode->info.hdisplay;
        data->dumbBuffers[i].height = connector->currentMode->info.vdisplay;
        data->dumbBuffers[i].bpp    = 32;

        ret = ioctl(dev->fd, DRM_IOCTL_MODE_CREATE_DUMB, &data->dumbBuffers[i]);

        if (ret)
        {
            SRMError("[%s] [%s] [%s MODE] Failed to create dumb buffer %d.",
                     dev->shortName, connector->name, MODE_NAME, i);
            return 0;
        }

        data->dumbMapRequests[i].handle = data->dumbBuffers[i].handle;

        ret = ioctl(connector->device->fd, DRM_IOCTL_MODE_MAP_DUMB, &data->dumbMapRequests[i]);

        if (ret)
        {
            SRMError("[%s] [%s] [%s MODE] DRM_IOCTL_MODE_MAP_DUMB failed for buffer %d. DRM error: %s.",
                     dev->shortName, connector->name, MODE_NAME, i, strerror(errno));
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
            SRMError("[%s] [%s] [%s MODE] Failed to map dumb buffer %d.",
                     dev->shortName, connector->name, MODE_NAME, i);
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
        for (UInt32 i = 0; i < data->c.buffersCount; i++)
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
        for (UInt32 i = 0; i < data->c.buffersCount; i++)
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

    if (data->c.drmFBs[0])
        return 1; // Already created

    Int32 ret;

    for (UInt32 i = 0; i < data->c.buffersCount; i++)
    {
        ret = drmModeAddFB(connector->device->fd,
                           data->dumbBuffers[i].width,
                           data->dumbBuffers[i].height,
                           24,
                           data->dumbBuffers[i].bpp,
                           data->dumbBuffers[i].pitch,
                           data->dumbBuffers[i].handle,
                           &data->c.drmFBs[i]);

        if (ret)
        {
            SRMError("[%s] [%s] [%s MODE] Failed to create DRM fb %d.",
                     connector->device->shortName, connector->name, MODE_NAME, i);
            return 0;
        }
    }

    data->c.currentBufferIndex = 0;

    return 1;
}

static void destroyDRMFramebuffers(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    for (UInt32 i = 0; i < data->c.buffersCount; i++)
    {
        if (data->c.drmFBs[i] != 0)
        {
            drmModeRmFB(connector->device->fd, data->c.drmFBs[i]);
            data->c.drmFBs[i] = 0;
        }
    }

    data->c.currentBufferIndex = 0;;
}

static UInt32 nextBufferIndex(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;
    if (data->c.currentBufferIndex + 1 == data->c.buffersCount)
        return 0;
    else
        return data->c.currentBufferIndex + 1;
}

static UInt8 render(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    eglMakeCurrent(connector->device->rendererDevice->eglDisplay,
                   EGL_NO_SURFACE, EGL_NO_SURFACE,
                   data->c.rendererContext);

    connector->interface->paintGL(connector, connector->interfaceData);

    eglMakeCurrent(connector->device->rendererDevice->eglDisplay,
                   EGL_NO_SURFACE, EGL_NO_SURFACE,
                   data->c.rendererContext);

    srmDeviceSyncWait(connector->device->rendererDevice);
    return 1;
}

static UInt8 flipPage(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    UInt32 b = data->c.currentBufferIndex;
    UInt32 h = data->dumbBuffers[b].height;
    UInt32 w = data->dumbBuffers[b].width;
    UInt32 p = data->dumbBuffers[b].pitch;
    UInt32 s = data->dumbBuffers[b].bpp/8;

    UInt8 useBufferRead = data->c.rendererBOWrappers[b] &&
                          data->c.rendererBOWrappers[b]->map &&
                          data->c.rendererBOWrappers[b]->dma.modifiers[0] == DRM_FORMAT_MOD_LINEAR;

    if (useBufferRead)
        goto skipGLRead;

    if (connector->damageBoxesCount > 0)
    {
        SRMBox *box;
        UInt32 buffInd;
        Int32 w;

        for (Int32 r = 0; r < connector->damageBoxesCount; r++)
        {
            box = &connector->damageBoxes[r];

            for (Int32 i = box->y1; i < box->y2; i++)
            {
                buffInd = i*p + s*box->x1;
                w = box->x2 - box->x1;

                if (connector->device->glExtensions.EXT_read_format_bgra)
                {
                    glReadPixels(box->x1,
                                 i,
                                 w,
                                 1,
                                 GL_BGRA_EXT,
                                 GL_UNSIGNED_BYTE,
                                 &data->dumbMaps[b][buffInd]);
                }
                else
                {
                    glReadPixels(box->x1,
                                 i,
                                 w,
                                 1,
                                 GL_RGBA,
                                 GL_UNSIGNED_BYTE,
                                 &data->dumbMaps[b][buffInd]);

                    UInt8 tmp;

                    // Swap R <> B
                    for (UInt32 r = buffInd; r < buffInd + w * s; r += s)
                    {
                        tmp = data->dumbMaps[b][r];
                        data->dumbMaps[b][r] = data->dumbMaps[b][r + 2];
                        data->dumbMaps[b][r + 2] = tmp;
                    }
                }
            }
        }

        connector->damageBoxesCount = 0;
        free(connector->damageBoxes);
        connector->damageBoxes = NULL;
    }
    else
    {
        for (UInt32 i = 0; i < h; i++)
        {
            if (connector->device->glExtensions.EXT_read_format_bgra)
            {
                glReadPixels(0,
                             i,
                             w,
                             1,
                             GL_BGRA_EXT,
                             GL_UNSIGNED_BYTE,
                             &data->dumbMaps[b][i*p]);
            }
            else
            {
                glReadPixels(0,
                             i,
                             w,
                             1,
                             GL_RGBA,
                             GL_UNSIGNED_BYTE,
                             &data->dumbMaps[b][i*p]);

                UInt8 tmp;

                // Swap R <> B
                for (UInt32 r = i*p; r < i*p + w * s; r += s)
                {
                    tmp = data->dumbMaps[b][r];
                    data->dumbMaps[b][r] = data->dumbMaps[b][r + 2];
                    data->dumbMaps[b][r + 2] = tmp;
                }
            }
        }
    }

    skipGLRead:

    if (useBufferRead)
    {
        if (connector->damageBoxesCount > 0)
        {
            SRMBox *box;

            for (Int32 r = 0; r < connector->damageBoxesCount; r++)
            {
                box = &connector->damageBoxes[r];

                srmBufferRead(data->c.rendererBOWrappers[b], box->x1, box->y1,
                              box->x2 - box->x1,
                              box->y2 - box->y1,
                              box->x1, box->y1, data->dumbBuffers[b].pitch, data->dumbMaps[b]);
            }

            connector->damageBoxesCount = 0;
            free(connector->damageBoxes);
            connector->damageBoxes = NULL;
        }
        else
        {
            srmBufferRead(data->c.rendererBOWrappers[b], 0, 0, data->dumbBuffers[b].width, data->dumbBuffers[b].height,
                          0, 0, data->dumbBuffers[b].pitch, data->dumbMaps[b]);
        }
    }

    srmRenderModeCommonPageFlip(connector, data->c.drmFBs[data->c.currentBufferIndex]);
    data->c.currentBufferIndex = nextBufferIndex(connector);
    connector->interface->pageFlipped(connector, connector->interfaceData);
    return 1;
}

static UInt8 initCrtc(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;
    eglMakeCurrent(connector->device->rendererDevice->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, data->c.rendererContext);
    return srmRenderModeCommonInitCrtc(connector, data->c.drmFBs[nextBufferIndex(connector)]);
}

static void uninitialize(SRMConnector *connector)
{
    if (connector->renderData)
    {    RenderModeData *data = (RenderModeData*)connector->renderData;
        eglMakeCurrent(connector->device->rendererDevice->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, data->c.rendererContext);
        srmRenderModeCommonUninitialize(connector);
        destroyDRMFramebuffers(connector);
        destroyDumbBuffers(connector);
        destroyRendererBuffers(connector);
        destroyEGLContext(connector);
        destroyData(connector);
    }
}

static UInt8 initialize(SRMConnector *connector)
{
    if (!eglBindAPI(EGL_OPENGL_ES_API))
    {
        SRMError("[%s] [%s] [%s MODE] Failed to bind GLES API.",
                 connector->device->shortName, connector->name, MODE_NAME);
        goto fail;
    }

    if (!createData(connector))
        goto fail;

    if (!getEGLConfiguration(connector))
        goto fail;

    if (!createEGLContext(connector))
        goto fail;

    if (!createRendererBuffers(connector))
        goto fail;

    if (!createDumbBuffers(connector))
        goto fail;

    if (!createDRMFramebuffers(connector))
        goto fail;

    if (!initCrtc(connector))
        goto fail;

    return 1;

fail:
    SRMError("[%s] [%s] [%s MODE] Failed to initialize.",
             connector->device->shortName, connector->name, MODE_NAME);

    uninitialize(connector);
    return 0;
}

static UInt8 updateMode(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    // If ret = 1 then new mode dimensions != prev mode dimensions
    if (srmRenderModeCommonUpdateMode(connector, data->c.drmFBs[nextBufferIndex(connector)]))
    {
        destroyDRMFramebuffers(connector);
        destroyDumbBuffers(connector);
        destroyRendererBuffers(connector);
        connector->currentMode = connector->targetMode;
        return initialize(connector);
    }

    return 1;
}

static UInt32 getCurrentBufferIndex(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;
    return data->c.currentBufferIndex;
}

static UInt32 getBuffersCount(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;
    return data->c.buffersCount;
}

static SRMBuffer *getBuffer(SRMConnector *connector, UInt32 bufferIndex)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    if (bufferIndex >= data->c.buffersCount)
        return NULL;

    return data->c.rendererBOWrappers[bufferIndex];
}

static void pauseRendering(SRMConnector *connector)
{
    srmRenderModeCommonPauseRendering(connector);
}

static void resumeRendering(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;
    srmRenderModeCommonResumeRendering(connector, data->c.drmFBs[data->c.currentBufferIndex]);
}

static UInt32 getFramebufferID(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;
    return data->c.rendererFBs[data->c.currentBufferIndex];
}

static EGLContext getEGLContext(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;
    return data->c.rendererContext;
}

void srmRenderModeDumbSetInterface(SRMConnector *connector)
{
    connector->renderInterface.initialize = &initialize;
    connector->renderInterface.render = &render;
    connector->renderInterface.flipPage = &flipPage;
    connector->renderInterface.updateMode = &updateMode;
    connector->renderInterface.getFramebufferID = &getFramebufferID;
    connector->renderInterface.getCurrentBufferIndex = &getCurrentBufferIndex;
    connector->renderInterface.getBuffersCount = &getBuffersCount;
    connector->renderInterface.getBuffer = &getBuffer;
    connector->renderInterface.getEGLContext = &getEGLContext;
    connector->renderInterface.uninitialize = &uninitialize;
    connector->renderInterface.pause = &pauseRendering;
    connector->renderInterface.resume = &resumeRendering;
}
