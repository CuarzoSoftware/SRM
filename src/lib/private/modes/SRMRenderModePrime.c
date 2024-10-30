#include <private/modes/SRMRenderModeCPU.h>
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

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <GL/gl.h>
#include <GLES2/gl2.h>

#define MODE_NAME "PRIME"

static GLfloat square[16] =
{  //  VERTEX       FRAGMENT
    -1.0f,  1.0f,   0.f, 1.f, // TL
    -1.0f, -1.0f,   0.f, 0.f, // BL
    1.0f, -1.0f,   1.f, 0.f, // BR
    1.0f,  1.0f,   1.f, 1.f  // TR
};

static GLchar vShaderStr[] =
    "precision lowp float;\
    precision lowp int;\
    uniform vec2 texSize;\
    uniform vec4 srcRect;\
    attribute vec4 vertexPosition;\
    varying vec2 v_texcoord;\
    void main()\
{\
        gl_Position = vec4(vertexPosition.xy, 0.0, 1.0);\
        v_texcoord.x = (srcRect.x + vertexPosition.z*srcRect.z) / texSize.x;\
        v_texcoord.y = (srcRect.y + srcRect.w - vertexPosition.w*srcRect.w) / texSize.y;\
}";

static GLchar fShaderStr[] =
    "precision lowp float;\
    precision lowp int;\
    uniform sampler2D tex;\
    varying vec2 v_texcoord;\
    void main()\
{\
        gl_FragColor = texture2D(tex, v_texcoord);\
}";

typedef struct RenderModeDataStruct
{
    RenderModeCommonData c;
    struct gbm_bo *connectorBOs[SRM_MAX_BUFFERING];
    SRMBuffer *connectorBOWrappers[SRM_MAX_BUFFERING];
    GLuint connectorRBs[SRM_MAX_BUFFERING];
    GLuint connectorFBs[SRM_MAX_BUFFERING];
    SRMBuffer *importedBuffers[3];
    EGLConfig connectorConfig;
    EGLContext connectorContext;

    GLuint
        texSizeUniform,
        srcRectUniform,
        activeTextureUniform;

    GLuint vertexShader,fragmentShader;
    GLuint programObject;
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
    data->connectorContext = EGL_NO_CONTEXT;
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
    RenderModeData *data = (RenderModeData*)connector->renderData;

    if (!srmRenderModeCommonChooseEGLConfiguration(connector->device->eglDisplay,
                                                   commonEGLConfigAttribs,
                                                   GBM_FORMAT_XRGB8888,
                                                   &data->connectorConfig))
    {
        SRMError("[%s] [%s] [%s MODE] Failed to choose EGL configuration.",
                 connector->device->name, connector->name, MODE_NAME);
        return 0;
    }

    if (!srmRenderModeCommonChooseEGLConfiguration(connector->device->rendererDevice->eglDisplay,
                                                   commonEGLConfigAttribs,
                                                   GBM_FORMAT_XRGB8888,
                                                   &data->c.rendererConfig))
    {
        SRMError("[%s] [%s] [%s MODE] Failed to choose EGL configuration.",
                 connector->device->rendererDevice->name, connector->name, MODE_NAME);
        return 0;
    }

    return 1;
}

static UInt8 createEGLContext(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    if (data->connectorContext != EGL_NO_CONTEXT)
        return 1; // Already created

    if (connector->device->eglExtensions.IMG_context_priority)
        connector->device->eglSharedContextAttribs[3] = EGL_CONTEXT_PRIORITY_HIGH_IMG;

    data->connectorContext = eglCreateContext(connector->device->eglDisplay,
                                                 data->connectorConfig,
                                                 connector->device->eglSharedContext,
                                                 connector->device->eglSharedContextAttribs);

    if (data->connectorContext == EGL_NO_CONTEXT)
    {
        SRMError("[%s] [%s] [%s MODE] Failed to create EGL context.",
                 connector->device->name, connector->name, MODE_NAME);
        return 0;
    }

    if (connector->device->rendererDevice->eglExtensions.IMG_context_priority)
        connector->device->rendererDevice->eglSharedContextAttribs[3] = EGL_CONTEXT_PRIORITY_HIGH_IMG;

    data->c.rendererContext = eglCreateContext(connector->device->rendererDevice->eglDisplay,
                                                data->c.rendererConfig,
                                                connector->device->rendererDevice->eglSharedContext,
                                                connector->device->rendererDevice->eglSharedContextAttribs);

    if (data->c.rendererContext == EGL_NO_CONTEXT)
    {
        SRMError("[%s] [%s] [%s MODE] Failed to create EGL context.",
                 connector->device->rendererDevice->name,
                 connector->name, MODE_NAME);
        return 0;
    }

    return 1;
}

static void destroyEGLContext(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    if (data->connectorContext != EGL_NO_CONTEXT)
    {
        eglMakeCurrent(connector->device->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(connector->device->eglDisplay, data->connectorContext);
        data->connectorContext = EGL_NO_CONTEXT;
    }

    if (data->c.rendererContext != EGL_NO_CONTEXT)
    {
        eglMakeCurrent(connector->device->rendererDevice->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(connector->device->rendererDevice->eglDisplay, data->c.rendererContext);
        data->c.rendererContext = EGL_NO_CONTEXT;
    }
}

static UInt8 createRenderBuffers(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    if (data->c.rendererBOs[0])
        return 1; // Already created

    data->c.buffersCount = srmRenderModeCommonCalculateBuffering(connector, MODE_NAME);

    SRMDevice *rendererDevice = connector->device->rendererDevice;

    for (UInt32 i = 0; i < data->c.buffersCount; i++)
    {
        /* Connector buffers */

        eglMakeCurrent(connector->device->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, data->connectorContext);
        srmRenderModeCommonCreateConnectorGBMBo(connector, &data->connectorBOs[i]);

        if (!data->connectorBOs[i])
        {
            SRMError("[%s] [%s] [%s MODE] Failed to create connector gbm_bo %d.",
                     connector->device->shortName, connector->name, MODE_NAME, i);
            return 0;
        }

        if (!srmBufferCreateRBFromBO(connector->device->core, data->connectorBOs[i], &data->connectorFBs[i], &data->connectorRBs[i], &data->connectorBOWrappers[i]))
        {
            SRMError("[%s] [%s] [%s MODE] Failed to create create connector renderbuffer %d.",
                     connector->device->shortName, connector->name, MODE_NAME, i);
            return 0;
        }

        /* Renderer buffers */

        eglMakeCurrent(rendererDevice->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, data->c.rendererContext);

        data->c.rendererBOs[i] = srmBufferCreateGBMBo(
            rendererDevice->gbm,
            connector->currentMode->info.hdisplay,
            connector->currentMode->info.vdisplay,
            connector->currentFormat.format,
            DRM_FORMAT_MOD_LINEAR,
            GBM_BO_USE_RENDERING);

        if (!data->c.rendererBOs[i])
        {
            SRMError("[%s] [%s] [%s MODE] Failed to create renderer gbm_bo %d.",
                     rendererDevice->shortName, connector->name, MODE_NAME, i);
            return 0;
        }

        if (!srmBufferCreateRBFromBO(connector->device->core, data->c.rendererBOs[i], &data->c.rendererFBs[i], &data->c.rendererRBs[i], &data->c.rendererBOWrappers[i]))
        {
            SRMError("[%s] [%s] [%s MODE] Failed to create create renderer renderbuffer %d.",
                     connector->device->shortName, connector->name, MODE_NAME, i);
            return 0;
        }

        /* Import renderer device buffer -> connector device buffer */

        SRMBufferDMAData dma;
        struct gbm_bo *bo = data->c.rendererBOs[i];
        dma.format = gbm_bo_get_format(bo);
        dma.width = gbm_bo_get_width(bo);
        dma.height = gbm_bo_get_height(bo);
        dma.num_fds = 1;
        dma.fds[0] = gbm_bo_get_fd(bo);
        dma.modifiers[0] = gbm_bo_get_modifier(bo);
        dma.strides[0] = gbm_bo_get_stride_for_plane(bo, 0);
        dma.offsets[0] = gbm_bo_get_offset(bo, 0);
        data->importedBuffers[i] = srmBufferCreateFromDMA(connector->device->core, connector->device, &dma);
    }

    return 1;
}

static void destroyRenderBuffers(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    for (UInt32 i = 0; i < data->c.buffersCount; i++)
    {
        if (data->importedBuffers[i])
        {
            srmBufferDestroy(data->importedBuffers[i]);
            data->importedBuffers[i] = NULL;
        }

        eglMakeCurrent(connector->device->rendererDevice->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, data->c.rendererContext);

        if (data->c.rendererFBs[i] != 0)
        {
            glDeleteFramebuffers(1, &data->c.rendererFBs[i]);
            data->c.rendererFBs[i] = 0;
        }

        if (data->c.rendererRBs[i] != 0)
        {
            glDeleteRenderbuffers(1, &data->c.rendererRBs[i]);
            data->c.rendererRBs[i] = 0;
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

        eglMakeCurrent(connector->device->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, data->connectorContext);

        if (data->connectorFBs[i] != 0)
        {
            glDeleteFramebuffers(1, &data->connectorFBs[i]);
            data->connectorFBs[i] = 0;
        }

        if (data->connectorRBs[i] != 0)
        {
            glDeleteRenderbuffers(1, &data->connectorRBs[i]);
            data->connectorRBs[i] = 0;
        }

        if (data->connectorBOWrappers[i])
        {
            srmBufferDestroy(data->connectorBOWrappers[i]);
            data->connectorBOWrappers[i] = NULL;
        }

        if (data->connectorBOs[i])
        {
            gbm_bo_destroy(data->connectorBOs[i]);
            data->connectorBOs[i] = NULL;
        }
    }
}

static GLuint compileShader(GLenum type, const char *shaderString)
{
    GLuint shader;
    GLint compiled;    
    shader = glCreateShader(type);
    glShaderSource(shader, 1, &shaderString, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

    if (!compiled)
    {
        GLint infoLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
        GLchar errorLog[infoLen];
        glGetShaderInfoLog(shader, infoLen, &infoLen, errorLog);
        SRMError("%s", errorLog);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

static UInt8 createGLES2(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    if (data->programObject)
        return 1; // Already created

    eglMakeCurrent(connector->device->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, data->connectorContext);
    data->vertexShader = compileShader(GL_VERTEX_SHADER, vShaderStr);
    data->fragmentShader = compileShader(GL_FRAGMENT_SHADER, fShaderStr);
    data->programObject = glCreateProgram();
    glAttachShader(data->programObject, data->vertexShader);
    glAttachShader(data->programObject, data->fragmentShader);
    glBindAttribLocation(data->programObject, 0, "vertexPosition");
    glLinkProgram(data->programObject);

    GLint linked;
    glGetProgramiv(data->programObject, GL_LINK_STATUS, &linked);

    if (!linked)
    {
        GLint infoLen = 0;
        glGetProgramiv(data->programObject, GL_INFO_LOG_LENGTH, &infoLen);
        glDeleteProgram(data->programObject);
        return 0;
    }

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glDisable(GL_DITHER);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    glDisable(GL_SAMPLE_COVERAGE);
    glDisable(GL_SAMPLE_ALPHA_TO_ONE);
    glUseProgram(data->programObject);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, square);
    glEnableVertexAttribArray(0);

    // Get Uniform Variables
    data->texSizeUniform = glGetUniformLocation(data->programObject, "texSize");
    data->srcRectUniform = glGetUniformLocation(data->programObject, "srcRect");
    data->activeTextureUniform = glGetUniformLocation(data->programObject, "tex");
    return 1;
}

static void destroyGLES2(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    eglMakeCurrent(connector->device->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, data->connectorContext);

    if (data->programObject)
    {
        glDeleteProgram(data->programObject);
        data->programObject = 0;
    }

    if (data->fragmentShader)
    {
        glDeleteShader(data->fragmentShader);
        data->fragmentShader = 0;
    }

    if (data->vertexShader)
    {
        glDeleteShader(data->vertexShader);
        data->vertexShader = 0;
    }
}

static UInt8 createDRMFramebuffers(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    if (data->c.drmFBs[0])
        return 1; // Already created

    if (!srmRenderModeCommonCreateDRMFBsFromBOs(connector, MODE_NAME, data->c.buffersCount, data->connectorBOs, data->c.drmFBs))
        return 0;

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

    data->c.currentBufferIndex = 0;
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
    eglMakeCurrent(connector->device->rendererDevice->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, data->c.rendererContext);
    glBindFramebuffer(GL_FRAMEBUFFER, data->c.rendererFBs[data->c.currentBufferIndex]);
    connector->interface->paintGL(connector, connector->interfaceData);
    eglMakeCurrent(connector->device->rendererDevice->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, data->c.rendererContext);
    srmDeviceSyncWait(connector->device->rendererDevice);
    return 1;
}

static void drawTexture(SRMConnector *connector, Int32 x, Int32 y, Int32 w, Int32 h)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;
    glScissor(x, y, w, h);
    glViewport(x, y, w, h);
    glUniform4f(data->srcRectUniform, x, y + h, w, -h);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

static UInt8 flipPage(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    SRMBox defaultDamage =
    {
        0,
        0,
        connector->currentMode->info.hdisplay,
        connector->currentMode->info.vdisplay
    };

    SRMBox *damage;
    UInt32 damageCount;

    if (/*connector->device->isBootVGA && */ connector->damageBoxesCount > 0)
    {
        damage = connector->damageBoxes;
        damageCount = connector->damageBoxesCount;
    }
    else
    {
        damage = &defaultDamage;
        damageCount = 1;
    }

    eglMakeCurrent(connector->device->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, data->connectorContext);
    glUseProgram(data->programObject);
    glBindFramebuffer(GL_FRAMEBUFFER, data->connectorFBs[data->c.currentBufferIndex]);
    glDisable(GL_BLEND);
    glActiveTexture(GL_TEXTURE0);
    glUniform2f(data->texSizeUniform, connector->currentMode->info.hdisplay, connector->currentMode->info.vdisplay);
    glBindTexture(GL_TEXTURE_2D, srmBufferGetTextureID(connector->device, data->importedBuffers[data->c.currentBufferIndex]));
    glUniform1i(data->activeTextureUniform, 0);

    for (UInt32 i = 0; i < damageCount; i++)
    {
        drawTexture(connector,
                    damage[i].x1,
                    damage[i].y1,
                    damage[i].x2 - damage[i].x1,
                    damage[i].y2 - damage[i].y1);
    }

    srmRenderModeCommonCreateSync(connector);
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
    {
        srmRenderModeCommonUninitialize(connector);
        destroyGLES2(connector);
        destroyRenderBuffers(connector);
        destroyEGLContext(connector);
        destroyDRMFramebuffers(connector);
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

    if (!createGLES2(connector))
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
        SRMError("Failed to initialize device %s connector %d with explicit modifiers, falling back to implicit modifiers (PRIME MODE).",
                 connector->device->name,
                 connector->id);
        connector->allowModifiers = 0;
        goto retry;
    }

    SRMError("Failed to initialize render mode PRIME for device %s connector %d.",
             connector->device->name,
             connector->id);

    return 0;
}

static UInt8 updateMode(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    // If ret = 1 then new mode dimensions != prev mode dimensions
    if (srmRenderModeCommonUpdateMode(connector, data->c.drmFBs[data->c.currentBufferIndex]))
    {
        destroyDRMFramebuffers(connector);
        destroyGLES2(connector);
        destroyRenderBuffers(connector);
        connector->currentMode = connector->targetMode;
        return initialize(connector);
    }

    return 1;
}

static UInt32 getFramebufferID(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;
    return data->c.rendererFBs[data->c.currentBufferIndex];
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

static EGLContext getEGLContext(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;
    return data->c.rendererContext;
}

void srmRenderModePrimeSetInterface(SRMConnector *connector)
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
