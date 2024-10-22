#include <assert.h>
#include <private/modes/SRMRenderModeItself.h>
#include <private/modes/SRMRenderModeCommon.h>

#include <private/SRMConnectorPrivate.h>
#include <private/SRMConnectorModePrivate.h>
#include <private/SRMDevicePrivate.h>
#include <private/SRMCrtcPrivate.h>
#include <private/SRMBufferPrivate.h>
#include <private/SRMPlanePrivate.h>

#include <SRMList.h>
#include <SRMLog.h>

#include <stdio.h>
#include <stdlib.h>

#define MODE_NAME "ITSELF"

static const EGLint eglConfigAttribs[] =
{
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_ALPHA_SIZE, 0,
    EGL_DEPTH_SIZE, 0,
    EGL_NONE
};

struct RenderModeDataStruct
{
    struct gbm_surface *gbmSurface;
    struct gbm_bo *gbmBOs[SRM_MAX_BUFFERING];
    UInt32 drmFBs[SRM_MAX_BUFFERING];
    SRMBuffer *srmBOs[SRM_MAX_BUFFERING];
    GLuint glRBs[SRM_MAX_BUFFERING];
    GLuint glFBs[SRM_MAX_BUFFERING];
    UInt32 buffersCount;
    UInt32 currentBufferIndex;
    EGLConfig eglConfig;
    EGLContext eglContext;
    EGLSurface eglSurface;
};

typedef struct RenderModeDataStruct RenderModeData;

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

    connector->usingRenderBuffers = 0;
    connector->renderData = data;
    data->eglContext = EGL_NO_CONTEXT;
    data->eglSurface = EGL_NO_SURFACE;
    srmRenderModeCommonSearchNonLinearModifier(connector);
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

static void destroyRenderBuffers(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    for (UInt32 i = 0; i < data->buffersCount; i++)
    {
        if (data->glFBs[i] != 0)
        {
            glDeleteFramebuffers(1, &data->glFBs[i]);
            data->glFBs[i] = 0;
        }

        if (data->glRBs[i] != 0)
        {
            glDeleteRenderbuffers(1, &data->glRBs[i]);
            data->glRBs[i] = 0;
        }

        if (data->srmBOs[i])
        {
            srmBufferDestroy(data->srmBOs[i]);
            data->srmBOs[i] = NULL;
        }

        if (data->gbmBOs[i])
        {
            gbm_bo_destroy(data->gbmBOs[i]);
            data->gbmBOs[i] = NULL;
        }
    }
}

static UInt8 createRenderBuffers(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    if (!connector->device->eglFunctions.glEGLImageTargetRenderbufferStorageOES)
        goto fail;

    data->buffersCount = srmRenderModeCommonCalculateBuffering(connector, MODE_NAME);

    for (UInt32 i = 0; i < data->buffersCount; i++)
    {
        srmRenderModeCommonCreateConnectorGBMBo(connector, &data->gbmBOs[i]);

        if (!data->gbmBOs[i])
        {
            SRMError("[%s] [%s] [%s MODE] Failed to create gbm_bo for render buffer %d.",
                connector->device->shortName, connector->name, MODE_NAME, i);
            goto fail;
        }

        data->srmBOs[i] = srmBufferCreateFromGBM(connector->device->core, data->gbmBOs[i]);

        if (!data->srmBOs[i])
        {
            SRMError("[%s] [%s] [%s MODE] Failed to create SRMBuffer for render buffer %d.",
                connector->device->shortName, connector->name, MODE_NAME, i);
            goto fail;
        }

        EGLImage image = srmBufferGetEGLImage(connector->device, data->srmBOs[i]);

        if (image == EGL_NO_IMAGE)
        {
            SRMError("[%s] [%s] [%s MODE] Failed to get EGLImage for render buffer %d.",
                connector->device->shortName, connector->name, MODE_NAME, i);
            goto fail;
        }

        glGenRenderbuffers(1, &data->glRBs[i]);
        glBindRenderbuffer(GL_RENDERBUFFER, data->glRBs[i]);
        connector->device->eglFunctions.glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, image);

        glGenFramebuffers(1, &data->glFBs[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, data->glFBs[i]);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, data->glRBs[i]);
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            SRMError("[%s] [%s] [%s MODE] Failed to create GL fb for render buffer %d.",
                connector->device->shortName, connector->name, MODE_NAME, i);
            goto fail;
        }
    }

    SRMDebug("[%s] [%s] [%s MODE] Using GL render buffers instead of EGLSurface.",
        connector->device->shortName, connector->name, MODE_NAME);

    connector->usingRenderBuffers = 1;
    return 1;

fail:
    destroyRenderBuffers(connector);
    return 0;
}

static UInt8 createGBMSurfaces(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    if (data->gbmSurface)
        return 1; // Already created

    srmRenderModeCommonCreateConnectorGBMSurface(connector, &data->gbmSurface);

    if (!data->gbmSurface)
    {
        SRMError("[%s] [%s] [%s MODE] Failed to create gbm_surface.",
            connector->device->shortName, connector->name, MODE_NAME);
        return 0;
    }

    return 1;
}

static void destroyGBMSurfaces(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    if (data->gbmSurface)
    {
        gbm_surface_destroy(data->gbmSurface);
        data->gbmSurface = NULL;
    }
}

static UInt8 getEGLConfiguration(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    if (!srmRenderModeCommonChooseEGLConfiguration(connector->device->eglDisplay,
        eglConfigAttribs,
        connector->currentFormat.format,
        &data->eglConfig))
    {
        SRMError("[%s] [%s] [%s MODE] Failed to choose EGL configuration.",
            connector->device->shortName, connector->name, MODE_NAME);
        return 0;
    }

    return 1;
}

static UInt8 createEGLContext(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    if (data->eglContext != EGL_NO_CONTEXT)
        return 1; // Already created

    if (connector->device->rendererDevice->eglExtensions.IMG_context_priority)
        connector->device->rendererDevice->eglSharedContextAttribs[3] = EGL_CONTEXT_PRIORITY_HIGH_IMG;

    data->eglContext = eglCreateContext(
        connector->device->eglDisplay,
        data->eglConfig,
        connector->device->eglSharedContext,
        connector->device->eglSharedContextAttribs);

    if (data->eglContext == EGL_NO_CONTEXT)
    {
        SRMError("[%s] [%s] [%s MODE] Failed to create EGL context.",
            connector->device->shortName, connector->name, MODE_NAME);
        return 0;
    }

    EGLint priority = EGL_CONTEXT_PRIORITY_MEDIUM_IMG;

    if (connector->device->eglExtensions.IMG_context_priority)
        eglQueryContext(connector->device->eglDisplay, data->eglContext, EGL_CONTEXT_PRIORITY_LEVEL_IMG, &priority);

    SRMDebug("[%s] [%s] [%s MODE] Using EGL context priority: %s.",
             connector->device->shortName, connector->name, MODE_NAME, srmEGLGetContextPriorityString(priority));

    eglMakeCurrent(
        connector->device->eglDisplay,
        EGL_NO_SURFACE,
        EGL_NO_SURFACE,
        data->eglContext);

    return 1;
}

static void destroyEGLContext(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    if (data->eglContext != EGL_NO_CONTEXT)
    {
        eglMakeCurrent(connector->device->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(connector->device->eglDisplay, data->eglContext);
        data->eglContext = EGL_NO_CONTEXT;
    }
}

static UInt8 createEGLSurfaces(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    if (data->eglSurface != EGL_NO_SURFACE)
        return 1; // Already created

    data->eglSurface = eglCreateWindowSurface(
        connector->device->eglDisplay,
        data->eglConfig,
        (EGLNativeWindowType)data->gbmSurface,
        NULL);

    if (data->eglSurface == EGL_NO_SURFACE)
    {
        SRMError("[%s] [%s] [%s MODE] Failed to create EGL surface.",
            connector->device->shortName, connector->name, MODE_NAME);
        return 0;
    }

    eglMakeCurrent(
        connector->device->eglDisplay,
        data->eglSurface,
        data->eglSurface,
        data->eglContext);

    SRMList *bos = srmListCreate();

    struct gbm_bo *bo = NULL;

    UInt32 fbCount = srmRenderModeCommonCalculateBuffering(connector, MODE_NAME);

    do
    {
        eglSwapBuffers(connector->device->rendererDevice->eglDisplay, data->eglSurface);
        bo = srmRenderModeCommonSurfaceLockFrontBufferSafe(data->gbmSurface);

        if (bo)
            srmListAppendData(bos, bo);
    }
    while (srmListGetLength(bos) < fbCount && gbm_surface_has_free_buffers(data->gbmSurface) > 0 && bo);

    data->buffersCount = srmListGetLength(bos);

    if (data->buffersCount == 0)
    {
        srmListDestroy(bos);
        SRMError("[%s] [%s] [%s MODE] Failed to get gbm_bo from gbm_surface.",
            connector->device->shortName, connector->name, MODE_NAME);
        return 0;
    }

    UInt32 i = 0;
    SRMListForeach(boIt, bos)
    {
        struct gbm_bo *b = srmListItemGetData(boIt);
        srmRenderModeCommonSurfaceReleaseBufferSafe(data->gbmSurface, b);
        data->gbmBOs[i] = b;
        i++;
    }

    srmListDestroy(bos);
    return 1;
}

static void destroyEGLSurfaces(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    for (UInt32 i = 0; i < data->buffersCount; i++)
    {
        if (data->gbmBOs[i])
        {
            srmRenderModeCommonSurfaceReleaseBufferSafe(data->gbmSurface, data->gbmBOs[i]);
            data->gbmBOs[i] = NULL;
        }
    }

    if (data->eglSurface != EGL_NO_SURFACE)
    {
        eglDestroySurface(connector->device->eglDisplay, data->eglSurface);
        data->eglSurface = EGL_NO_SURFACE;
    }
}

static UInt8 swapBuffers(SRMConnector *connector, EGLDisplay display, EGLSurface surface)
{
    if (connector->usingRenderBuffers)
        return 0;

    return eglSwapBuffers(display, surface);
}

static UInt8 createDRMFramebuffers(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    if (data->drmFBs[0] != 0)
        return 1; // Already created

    Int32 ret;

    for (UInt32 i = 0; i < data->buffersCount; i++)
    {
        if (connector->currentFormat.modifier != DRM_FORMAT_MOD_LINEAR)
        {
            UInt32 boHandles[4] = { 0 };
            boHandles[0] = gbm_bo_get_handle_for_plane(data->gbmBOs[i], 0).u32;

            UInt32 pitches[4] = { 0 };
            pitches[0] = gbm_bo_get_stride_for_plane(data->gbmBOs[i], 0);

            UInt32 offsets[4] = { 0 };
            offsets[0] = gbm_bo_get_offset(data->gbmBOs[i], 0);

            UInt64 mods[4] = { 0 };
            mods[0] = connector->currentFormat.modifier;

            ret = drmModeAddFB2WithModifiers(
                connector->device->fd,
                connector->currentMode->info.hdisplay,
                connector->currentMode->info.vdisplay,
                connector->currentFormat.format,
                boHandles,
                pitches,
                offsets,
                mods,
                &data->drmFBs[i],
                DRM_MODE_FB_MODIFIERS);

            if (ret == 0)
                goto skipLegacy;
        }

        ret = drmModeAddFB(
            connector->device->fd,
            connector->currentMode->info.hdisplay,
            connector->currentMode->info.vdisplay,
            24,
            gbm_bo_get_bpp(data->gbmBOs[i]),
            gbm_bo_get_stride(data->gbmBOs[i]),
            gbm_bo_get_handle(data->gbmBOs[i]).u32,
            &data->drmFBs[i]);

        if (ret)
        {
            SRMError("[%s] [%s] [%s MODE] Failed o create DRM framebuffer %d. DRM Error: %d.",
                connector->device->shortName, connector->name, MODE_NAME, i, ret);
            return 0;
        }

        skipLegacy:

        // Already created when using renderbuffers
        if (!connector->usingRenderBuffers)
        {
            data->srmBOs[i] = srmBufferCreateFromGBM(connector->device->core, data->gbmBOs[i]);

            if (!data->srmBOs[i])
            {
                SRMWarning("[%s] [%s] [%s MODE] Failed to create SRMBuffer %d from GBM bo.",
                    connector->device->shortName, connector->name, MODE_NAME, i);
            }
            else
            {
                if (srmBufferGetTextureID(connector->device, data->srmBOs[i]) == 0)
                {
                    SRMWarning("[%s] [%s] [%s MODE] Failed o get texture ID from SRMBuffer %d.",
                        connector->device->shortName, connector->name, MODE_NAME, i);
                }
            }
        }
    }

    data->currentBufferIndex = 0;

    return 1;
}

static void destroyDRMFramebuffers(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    for (UInt32 i = 0; i < data->buffersCount; i++)
    {
        if (data->drmFBs[i] != 0)
        {
            drmModeRmFB(connector->device->fd, data->drmFBs[i]);
            data->drmFBs[i] = 0;
        }
    }

    if (!connector->usingRenderBuffers)
    {
        for (UInt32 i = 0; i < data->buffersCount; i++)
        {
            if (data->srmBOs[i])
            {
                srmBufferDestroy(data->srmBOs[i]);
                data->srmBOs[i] = NULL;
            }
        }
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
    eglMakeCurrent(connector->device->eglDisplay, data->eglSurface, data->eglSurface, data->eglContext);
    glBindFramebuffer(GL_FRAMEBUFFER, connector->usingRenderBuffers ? data->glFBs[data->currentBufferIndex] : 0);
    connector->interface->paintGL(connector, connector->interfaceData);
    eglMakeCurrent(connector->device->eglDisplay, data->eglSurface, data->eglSurface, data->eglContext);
    return 1;
}

static UInt8 flipPage(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    srmRenderModeCommonCreateSync(connector);

    if (connector->usingRenderBuffers)
    {
        srmRenderModeCommonPageFlip(connector, data->drmFBs[data->currentBufferIndex]);
        data->currentBufferIndex = nextBufferIndex(connector);
        connector->interface->pageFlipped(connector, connector->interfaceData);
    }
    else
    {
        swapBuffers(connector, connector->device->eglDisplay, data->eglSurface);
        struct gbm_bo *bo = srmRenderModeCommonSurfaceLockFrontBufferSafe(data->gbmSurface);

        srmRenderModeCommonPageFlip(connector, data->drmFBs[data->currentBufferIndex]);

        // TODO
        assert("This driver doesn't recycle gbm_surface bos." && bo == data->gbmBOs[data->currentBufferIndex]);

        data->currentBufferIndex = nextBufferIndex(connector);
        srmRenderModeCommonSurfaceReleaseBufferSafe(data->gbmSurface, data->gbmBOs[data->currentBufferIndex]);
    }

    connector->interface->pageFlipped(connector, connector->interfaceData);

    return 1;
}

static UInt8 initCrtc(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;
    return srmRenderModeCommonInitCrtc(connector, data->drmFBs[nextBufferIndex(connector)]);
}

static void uninitialize(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    if (data)
    {
        eglMakeCurrent(connector->device->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, data->eglContext);
        srmRenderModeCommonUninitialize(connector);
        destroyDRMFramebuffers(connector);

        if (connector->usingRenderBuffers)
            destroyRenderBuffers(connector);
        else
        {
            destroyEGLSurfaces(connector);
            destroyGBMSurfaces(connector);
        }
        destroyEGLContext(connector);
        destroyData(connector);
    }
}

static UInt8 initialize(SRMConnector *connector)
{
    connector->allowModifiers = 1;

retry:
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

    if (!createRenderBuffers(connector))
    {
        if (!createGBMSurfaces(connector))
            goto fail;

        if (!createEGLSurfaces(connector))
            goto fail;
    }

    if (!createDRMFramebuffers(connector))
        goto fail;

    if (!initCrtc(connector))
        goto fail;

    return 1;

fail:
    uninitialize(connector);

    if (connector->allowModifiers)
    {
        SRMError("[%s] [%s] [%s MODE] Failed to initialize with explicit modifiers, falling back to implicit modifiers.",
            connector->device->shortName, connector->name, MODE_NAME);
        connector->allowModifiers = 0;
        goto retry;
    }

    SRMError("[%s] [%s] [%s MODE] Failed to initialize.",
        connector->device->shortName, connector->name, MODE_NAME);

    return 0;
}

static UInt8 updateMode(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    // If ret = 1 then new mode dimensions != prev mode dimensions
    if (srmRenderModeCommonUpdateMode(connector, data->drmFBs[data->currentBufferIndex]))
    {
        eglMakeCurrent(connector->device->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, connector->device->eglSharedContext);
        destroyDRMFramebuffers(connector);

        if (connector->usingRenderBuffers)
            destroyRenderBuffers(connector);
        else
        {
            destroyEGLSurfaces(connector);
            destroyGBMSurfaces(connector);
        }
        connector->currentMode = connector->targetMode;
        return initialize(connector);
    }

    return 1;
}

static UInt32 getFramebufferID(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    if (connector->usingRenderBuffers)
        return data->glFBs[data->currentBufferIndex];

    return 0;
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

    return data->srmBOs[bufferIndex];
}

static void pauseRendering(SRMConnector *connector)
{
    srmRenderModeCommonPauseRendering(connector);
}

static void resumeRendering(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    for (UInt32 i = 0; i < data->buffersCount; i++)
        srmRenderModeCommonSurfaceReleaseBufferSafe(data->gbmSurface, data->gbmBOs[i]);

    srmRenderModeCommonResumeRendering(connector, data->drmFBs[data->currentBufferIndex]);
}

static EGLContext getEGLContext(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;
    return data->eglContext;
}

void srmRenderModeItselfSetInterface(SRMConnector *connector)
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
