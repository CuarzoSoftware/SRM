#include <private/modes/SRMRenderModeItself.h>
#include <private/modes/SRMRenderModeCommon.h>

#include <private/SRMConnectorPrivate.h>
#include <private/SRMConnectorModePrivate.h>
#include <private/SRMDevicePrivate.h>
#include <private/SRMCrtcPrivate.h>
#include <private/SRMBufferPrivate.h>
#include <private/SRMPlanePrivate.h>
#include <private/SRMEGLPrivate.h>

#include <SRMList.h>
#include <SRMLog.h>

#include <stdio.h>
#include <stdlib.h>

#define MODE_NAME "ITSELF"

static UInt8 createData(SRMConnector *connector)
{
    if (connector->renderData)
        return 1; // Already setup

    RenderModeCommonData *data = calloc(1, sizeof(RenderModeCommonData));

    if (!data)
    {
        SRMError("[%s] [%s] [%s MODE] Could not allocate render mode data.",
                 connector->device->shortName, connector->name, MODE_NAME);
        return 0;
    }

    connector->renderData = data;
    data->rendererContext = EGL_NO_CONTEXT;
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

static UInt8 getEGLConfiguration(SRMConnector *connector)
{
    RenderModeCommonData *data = (RenderModeCommonData*)connector->renderData;

    if (!srmRenderModeCommonChooseEGLConfiguration(connector->device->eglDisplay,
        commonEGLConfigAttribs,
        connector->currentFormat.format,
        &data->rendererConfig))
    {
        SRMError("[%s] [%s] [%s MODE] Failed to choose EGL configuration.",
            connector->device->shortName, connector->name, MODE_NAME);
        return 0;
    }

    return 1;
}

static UInt8 createEGLContext(SRMConnector *connector)
{
    RenderModeCommonData *data = (RenderModeCommonData*)connector->renderData;

    if (data->rendererContext != EGL_NO_CONTEXT)
        return 1; // Already created

    if (connector->device->rendererDevice->eglExtensions.IMG_context_priority)
        connector->device->rendererDevice->eglSharedContextAttribs[3] = EGL_CONTEXT_PRIORITY_HIGH_IMG;

    data->rendererContext = eglCreateContext(
        connector->device->eglDisplay,
        data->rendererConfig,
        connector->device->eglSharedContext,
        connector->device->eglSharedContextAttribs);

    if (data->rendererContext == EGL_NO_CONTEXT)
    {
        SRMError("[%s] [%s] [%s MODE] Failed to create EGL context.",
            connector->device->shortName, connector->name, MODE_NAME);
        return 0;
    }

    EGLint priority = EGL_CONTEXT_PRIORITY_MEDIUM_IMG;

    if (connector->device->eglExtensions.IMG_context_priority)
        eglQueryContext(connector->device->eglDisplay, data->rendererContext, EGL_CONTEXT_PRIORITY_LEVEL_IMG, &priority);

    SRMDebug("[%s] [%s] [%s MODE] Using EGL context priority: %s.",
             connector->device->shortName, connector->name, MODE_NAME, srmEGLGetContextPriorityString(priority));

    eglMakeCurrent(connector->device->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, data->rendererContext);
    return 1;
}

static void destroyEGLContext(SRMConnector *connector)
{
    RenderModeCommonData *data = (RenderModeCommonData*)connector->renderData;

    if (data->rendererContext != EGL_NO_CONTEXT)
    {
        eglMakeCurrent(connector->device->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(connector->device->eglDisplay, data->rendererContext);
        data->rendererContext = EGL_NO_CONTEXT;
    }
}

static UInt8 createRenderBuffers(SRMConnector *connector)
{
    RenderModeCommonData *data = (RenderModeCommonData*)connector->renderData;

    if (data->rendererBOs[0])
        return 1; // Already created

    data->buffersCount = srmRenderModeCommonCalculateBuffering(connector, MODE_NAME);

    for (UInt32 i = 0; i < data->buffersCount; i++)
    {
        srmRenderModeCommonCreateConnectorGBMBo(connector, &data->rendererBOs[i]);

        if (!data->rendererBOs[i])
        {
            SRMError("[%s] [%s] [%s MODE] Failed to create gbm_bo for renderbuffer %d.",
                     connector->device->shortName, connector->name, MODE_NAME, i);
            return 0;
        }

        if (!srmBufferCreateRBFromBO(connector->device->core, data->rendererBOs[i], &data->rendererFBs[i], &data->rendererRBs[i], &data->rendererBOWrappers[i]))
        {
            SRMError("[%s] [%s] [%s MODE] Failed to create create renderbuffer %d.",
                     connector->device->shortName, connector->name, MODE_NAME, i);
            return 0;
        }
    }

    return 1;
}

static void destroyRenderBuffers(SRMConnector *connector)
{
    RenderModeCommonData *data = (RenderModeCommonData*)connector->renderData;

    eglMakeCurrent(connector->device->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, data->rendererContext);

    for (UInt32 i = 0; i < data->buffersCount; i++)
    {
        if (data->rendererFBs[i] != 0)
        {
            glDeleteFramebuffers(1, &data->rendererFBs[i]);
            data->rendererFBs[i] = 0;
        }

        if (data->rendererRBs[i] != 0)
        {
            glDeleteRenderbuffers(1, &data->rendererRBs[i]);
            data->rendererRBs[i] = 0;
        }

        if (data->rendererBOWrappers[i])
        {
            srmBufferDestroy(data->rendererBOWrappers[i]);
            data->rendererBOWrappers[i] = NULL;
        }

        if (data->rendererBOs[i])
        {
            gbm_bo_destroy(data->rendererBOs[i]);
            data->rendererBOs[i] = NULL;
        }
    }
}

static UInt8 createDRMFramebuffers(SRMConnector *connector)
{
    RenderModeCommonData *data = (RenderModeCommonData*)connector->renderData;

    if (data->drmFBs[0] != 0)
        return 1; // Already created

    if (!srmRenderModeCommonCreateDRMFBsFromBOs(connector, MODE_NAME, data->buffersCount, data->rendererBOs, data->drmFBs))
        return 0;

    data->currentBufferIndex = 0;
    return 1;
}

static void destroyDRMFramebuffers(SRMConnector *connector)
{
    RenderModeCommonData *data = (RenderModeCommonData*)connector->renderData;

    for (UInt32 i = 0; i < data->buffersCount; i++)
    {
        if (data->drmFBs[i] != 0)
        {
            drmModeRmFB(connector->device->fd, data->drmFBs[i]);
            data->drmFBs[i] = 0;
        }
    }

    data->currentBufferIndex = 0;
}

static UInt32 nextBufferIndex(SRMConnector *connector)
{
    RenderModeCommonData *data = (RenderModeCommonData*)connector->renderData;
    if (data->currentBufferIndex + 1 == data->buffersCount)
        return 0;
    else
        return data->currentBufferIndex + 1;
}

static UInt8 render(SRMConnector *connector)
{
    RenderModeCommonData *data = (RenderModeCommonData*)connector->renderData;
    eglMakeCurrent(connector->device->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, data->rendererContext);
    glBindFramebuffer(GL_FRAMEBUFFER, data->rendererFBs[data->currentBufferIndex]);
    connector->interface->paintGL(connector, connector->interfaceData);
    eglMakeCurrent(connector->device->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, data->rendererContext);
    return 1;
}

static UInt8 flipPage(SRMConnector *connector)
{
    RenderModeCommonData *data = (RenderModeCommonData*)connector->renderData;
    srmRenderModeCommonCreateSync(connector);
    srmRenderModeCommonPageFlip(connector, data->drmFBs[data->currentBufferIndex]);
    data->currentBufferIndex = nextBufferIndex(connector);
    connector->interface->pageFlipped(connector, connector->interfaceData);
    connector->interface->pageFlipped(connector, connector->interfaceData);
    return 1;
}

static UInt8 initCrtc(SRMConnector *connector)
{
    RenderModeCommonData *data = (RenderModeCommonData*)connector->renderData;
    return srmRenderModeCommonInitCrtc(connector, data->drmFBs[nextBufferIndex(connector)]);
}

static void uninitialize(SRMConnector *connector)
{
    RenderModeCommonData *data = (RenderModeCommonData*)connector->renderData;

    if (data)
    {
        eglMakeCurrent(connector->device->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, data->rendererContext);
        srmRenderModeCommonUninitialize(connector);
        destroyDRMFramebuffers(connector);
        destroyRenderBuffers(connector);
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
        goto fail;

    if (!createDRMFramebuffers(connector))
        goto fail;

    if (!initCrtc(connector))
        goto fail;

    return 1;

fail:
    uninitialize(connector);

    if (connector->allowModifiers)
    {
        SRMError("[%s] [%s] [%s MODE] Failed to initialize with explicit modifiers %s - %s, falling back to implicit modifiers.",
            connector->device->shortName, connector->name, MODE_NAME,
            drmGetFormatName(connector->currentFormat.format),
            drmGetFormatModifierName(connector->currentFormat.modifier));
        connector->allowModifiers = 0;
        goto retry;
    }

    SRMError("[%s] [%s] [%s MODE] Failed to initialize.",
        connector->device->shortName, connector->name, MODE_NAME);

    return 0;
}

static UInt8 updateMode(SRMConnector *connector)
{
    RenderModeCommonData *data = (RenderModeCommonData*)connector->renderData;

    // If ret == 1 means the new mode dimensions != prev dimensions
    if (srmRenderModeCommonUpdateMode(connector, data->drmFBs[data->currentBufferIndex]))
    {
        eglMakeCurrent(connector->device->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, connector->device->eglSharedContext);
        destroyDRMFramebuffers(connector);
        destroyRenderBuffers(connector);
        connector->currentMode = connector->targetMode;
        return initialize(connector);
    }

    return 1;
}

static UInt32 getFramebufferID(SRMConnector *connector)
{
    RenderModeCommonData *data = (RenderModeCommonData*)connector->renderData;
    return data->rendererFBs[data->currentBufferIndex];
}

static UInt32 getCurrentBufferIndex(SRMConnector *connector)
{
    RenderModeCommonData *data = (RenderModeCommonData*)connector->renderData;
    return data->currentBufferIndex;
}

static UInt32 getBuffersCount(SRMConnector *connector)
{
    RenderModeCommonData *data = (RenderModeCommonData*)connector->renderData;
    return data->buffersCount;
}

static SRMBuffer *getBuffer(SRMConnector *connector, UInt32 bufferIndex)
{
    RenderModeCommonData *data = (RenderModeCommonData*)connector->renderData;

    if (bufferIndex >= data->buffersCount)
        return NULL;

    return data->rendererBOWrappers[bufferIndex];
}

static void pauseRendering(SRMConnector *connector)
{
    srmRenderModeCommonPauseRendering(connector);
}

static void resumeRendering(SRMConnector *connector)
{
    RenderModeCommonData *data = (RenderModeCommonData*)connector->renderData;
    srmRenderModeCommonResumeRendering(connector, data->drmFBs[data->currentBufferIndex]);
}

static EGLContext getEGLContext(SRMConnector *connector)
{
    RenderModeCommonData *data = (RenderModeCommonData*)connector->renderData;
    return data->rendererContext;
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
