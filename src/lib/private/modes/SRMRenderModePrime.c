#include <private/modes/SRMRenderModeCPU.h>
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
#include <stdlib.h>
#include <unistd.h>
#include <GL/gl.h>
#include <GLES2/gl2.h>

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

struct RenderModeDataStruct
{
    struct gbm_surface *rendererGBMSurface;
    struct gbm_surface *connectorGBMSurface;

    struct gbm_bo *rendererBOs[3];
    struct gbm_bo *connectorBOs[3];

    UInt32 connectorDRMFramebuffers[3];
    SRMBuffer *rendererBuffers[3];

    UInt32 buffersCount;
    UInt32 currentBufferIndex;

    EGLConfig connectorEGLConfig;
    EGLContext connectorEGLContext;
    EGLSurface connectorEGLSurface;

    EGLConfig rendererEGLConfig;
    EGLContext rendererEGLContext;
    EGLSurface rendererEGLSurface;

    SRMBuffer *primeBuffers[3];

    GLuint
        texSizeUniform,
        srcRectUniform,
        activeTextureUniform;

    GLuint vertexShader,fragmentShader;
    GLuint programObject;
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
        SRMError("[%s] Could not allocate data for connector %d render mode (PRIME MODE).",
                 connector->device->name,
                 connector->id);
        return 0;
    }

    connector->renderData = data;
    data->connectorEGLContext = EGL_NO_CONTEXT;
    data->connectorEGLSurface = EGL_NO_SURFACE;
    data->rendererEGLContext = EGL_NO_CONTEXT;
    data->rendererEGLSurface = EGL_NO_SURFACE;

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

static UInt8 createGBMSurfaces(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    if (data->connectorGBMSurface)
    {
        // Already created
        return 1;
    }

    srmRenderModeCommonCreateConnectorGBMSurface(connector, &data->connectorGBMSurface);

    if (!data->connectorGBMSurface)
    {
        SRMError("[%s] Failed to create GBM surface for connector %d (PRIME MODE).",
                 connector->device->name, connector->id);
        return 0;
    }

    data->rendererGBMSurface = gbm_surface_create(
        connector->device->rendererDevice->gbm,
        connector->currentMode->info.hdisplay,
        connector->currentMode->info.vdisplay,
        GBM_FORMAT_XRGB8888,
        GBM_BO_USE_LINEAR | GBM_BO_USE_RENDERING);

    if (!data->rendererGBMSurface)
    {
        SRMError("[%s] Failed to create GBM surface for connector %d (PRIME MODE).",
                 connector->device->rendererDevice->name, connector->id);
        return 0;
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

    if (data->rendererGBMSurface)
    {
        gbm_surface_destroy(data->rendererGBMSurface);
        data->rendererGBMSurface = NULL;
    }
}

static UInt8 getEGLConfiguration(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    if (!srmRenderModeCommonChooseEGLConfiguration(connector->device->eglDisplay,
                                                   eglConfigAttribs,
                                                   GBM_FORMAT_XRGB8888,
                                                   &data->connectorEGLConfig))
    {
        SRMError("[%s] Failed to choose EGL configuration for connector %d (PRIME MODE).",
                 connector->device->name,
                 connector->id);
        return 0;
    }

    if (!srmRenderModeCommonChooseEGLConfiguration(connector->device->rendererDevice->eglDisplay,
                                                   eglConfigAttribs,
                                                   GBM_FORMAT_XRGB8888,
                                                   &data->rendererEGLConfig))
    {
        SRMError("[%s] Failed to choose EGL configuration for connector %d (PRIME MODE).",
                 connector->device->rendererDevice->name,
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

    if (connector->device->eglExtensions.IMG_context_priority)
        connector->device->eglSharedContextAttribs[3] = EGL_CONTEXT_PRIORITY_HIGH_IMG;

    data->connectorEGLContext = eglCreateContext(connector->device->eglDisplay,
                                                 data->connectorEGLConfig,
                                                 connector->device->eglSharedContext,
                                                 connector->device->eglSharedContextAttribs);

    if (data->connectorEGLContext == EGL_NO_CONTEXT)
    {
        SRMError("[%s] Failed to create EGL context for connector %d (PRIME MODE).",
                 connector->device->name,
                 connector->id);
        return 0;
    }

    if (connector->device->rendererDevice->eglExtensions.IMG_context_priority)
        connector->device->rendererDevice->eglSharedContextAttribs[3] = EGL_CONTEXT_PRIORITY_HIGH_IMG;

    data->rendererEGLContext = eglCreateContext(connector->device->rendererDevice->eglDisplay,
                                                data->rendererEGLConfig,
                                                connector->device->rendererDevice->eglSharedContext,
                                                connector->device->rendererDevice->eglSharedContextAttribs);

    if (data->rendererEGLContext == EGL_NO_CONTEXT)
    {
        SRMError("[%s] Failed to create EGL context for connector %d (PRIME MODE).",
                 connector->device->rendererDevice->name,
                 connector->id);
        return 0;
    }

    return 1;
}

static void destroyEGLContext(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    if (data->connectorEGLContext != EGL_NO_CONTEXT)
    {
        eglDestroyContext(connector->device->eglDisplay, data->connectorEGLContext);
        data->connectorEGLContext = EGL_NO_CONTEXT;
    }

    if (data->rendererEGLContext != EGL_NO_CONTEXT)
    {
        eglDestroyContext(connector->device->rendererDevice->eglDisplay, data->rendererEGLContext);
        data->rendererEGLContext = EGL_NO_CONTEXT;
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

    data->connectorEGLSurface = eglCreateWindowSurface(connector->device->eglDisplay,
                                                       data->connectorEGLConfig,
                                                       (EGLNativeWindowType)data->connectorGBMSurface,
                                                       NULL);

    if (data->connectorEGLSurface == EGL_NO_SURFACE)
    {
        SRMError("[%s] Failed to create EGL surface for connector %d (PRIME MODE).",
                 connector->device->name,
                 connector->id);
        return 0;
    }

    eglMakeCurrent(connector->device->eglDisplay,
                   data->connectorEGLSurface,
                   data->connectorEGLSurface,
                   data->connectorEGLContext);

    SRMList *bos = srmListCreate();
    struct gbm_bo *bo = NULL;
    char *env = getenv("SRM_RENDER_MODE_PRIME_FB_COUNT");
    UInt32 fbCount = 2;

    if (env)
    {
        Int32 c = atoi(env);

        if (connector->device->isBootVGA && c > 1 && c < 4)
            fbCount = c;
    }

    do
    {
        eglSwapBuffers(connector->device->eglDisplay,
                       data->connectorEGLSurface);
        bo = gbm_surface_lock_front_buffer(data->connectorGBMSurface);

        if (bo)
            srmListAppendData(bos, bo);
    }
    while (srmListGetLength(bos) < fbCount && gbm_surface_has_free_buffers(data->connectorGBMSurface) > 0 && bo);

    data->buffersCount = srmListGetLength(bos);

    if (data->buffersCount == 0)
    {
        srmListDestroy(bos);
        SRMError("[%s] Failed to get BOs for connector %d (PRIME MODE).",
                 connector->device->name, connector->id);
        return 0;
    }

    UInt32 i = 0;
    SRMListForeach(boIt, bos)
    {
        struct gbm_bo *b = srmListItemGetData(boIt);
        gbm_surface_release_buffer(data->connectorGBMSurface, b);
        data->connectorBOs[i] = b;
        i++;
    }

    srmListDestroy(bos);

    /* Renderer */

    data->rendererEGLSurface = eglCreateWindowSurface(connector->device->rendererDevice->eglDisplay,
                                                      data->rendererEGLConfig,
                                                      (EGLNativeWindowType)data->rendererGBMSurface,
                                                      NULL);

    if (data->rendererEGLSurface == EGL_NO_SURFACE)
    {
        SRMError("[%s] Failed to create EGL surface for connector %d (PRIME MODE).",
                 connector->device->rendererDevice->name,
                 connector->id);
        return 0;
    }

    eglMakeCurrent(connector->device->rendererDevice->eglDisplay,
                   data->rendererEGLSurface,
                   data->rendererEGLSurface,
                   data->rendererEGLContext);

    bos = srmListCreate();
    bo = NULL;

    do
    {
        eglSwapBuffers(connector->device->rendererDevice->eglDisplay,
                       data->rendererEGLSurface);
        bo = gbm_surface_lock_front_buffer(data->rendererGBMSurface);

        if (bo)
            srmListAppendData(bos, bo);
    }
    while (srmListGetLength(bos) < fbCount && gbm_surface_has_free_buffers(data->rendererGBMSurface) > 0 && bo);

    data->buffersCount = srmListGetLength(bos);

    if (data->buffersCount == 0)
    {
        srmListDestroy(bos);
        SRMError("[%s] Failed to get BOs for connector %d (PRIME MODE).", connector->device->name, connector->id);
        return 0;
    }

    i = 0;
    SRMBufferDMAData dma;
    SRMListForeach(boIt, bos)
    {
        struct gbm_bo *b = srmListItemGetData(boIt);
        gbm_surface_release_buffer(data->rendererGBMSurface, b);
        data->rendererBOs[i] = b;
        dma.format = DRM_FORMAT_XRGB8888;
        dma.width = gbm_bo_get_width(b);
        dma.height = gbm_bo_get_height(b);
        dma.num_fds = 1;
        dma.fds[0] = gbm_bo_get_fd(b);
        dma.modifiers[0] = DRM_FORMAT_MOD_LINEAR;
        dma.strides[0] = gbm_bo_get_stride_for_plane(b, 0);
        dma.offsets[0] = gbm_bo_get_offset(b, 0);
        data->primeBuffers[i] = srmBufferCreateFromDMA(connector->device->core,
                                                     connector->device,
                                                     &dma);
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
        if (data->connectorBOs[i])
        {
            gbm_surface_release_buffer(data->connectorGBMSurface, data->connectorBOs[i]);
            data->connectorBOs[i] = NULL;
        }

        if (data->rendererBOs[i])
        {
            gbm_surface_release_buffer(data->rendererGBMSurface, data->rendererBOs[i]);
            data->rendererBOs[i] = NULL;
        }

        if (data->primeBuffers[i])
        {
            srmBufferDestroy(data->primeBuffers[i]);
            data->primeBuffers[i] = NULL;
        }
    }

    if (data->connectorEGLSurface != EGL_NO_SURFACE)
    {
        eglDestroySurface(connector->device->eglDisplay, data->connectorEGLSurface);
        data->connectorEGLSurface = EGL_NO_SURFACE;
    }

    if (data->rendererEGLSurface != EGL_NO_SURFACE)
    {
        eglDestroySurface(connector->device->rendererDevice->eglDisplay, data->rendererEGLSurface);
        data->rendererEGLSurface = EGL_NO_SURFACE;
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

    eglMakeCurrent(connector->device->eglDisplay,
                   data->connectorEGLSurface,
                   data->connectorEGLSurface,
                   data->connectorEGLContext);

    if (data->programObject)
    {
        // Already created
        return 1;
    }

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

    eglMakeCurrent(connector->device->eglDisplay,
                   data->connectorEGLSurface,
                   data->connectorEGLSurface,
                   data->connectorEGLContext);

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

static UInt8 swapBuffers(SRMConnector *connector, EGLDisplay display, EGLSurface surface)
{
    SRM_UNUSED(connector);
    return eglSwapBuffers(display, surface);
}

static UInt8 createDRMFramebuffers(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    eglMakeCurrent(connector->device->rendererDevice->eglDisplay,
                   data->rendererEGLSurface,
                   data->rendererEGLSurface,
                   data->rendererEGLContext);

    if (data->connectorDRMFramebuffers[0])
    {
        // Already created
        return 1;
    }

    Int32 ret;

    for (UInt32 i = 0; i < data->buffersCount; i++)
    {
        if (connector->currentFormat.modifier != DRM_FORMAT_MOD_LINEAR)
        {
            UInt32 boHandles[4] = { 0 };
            boHandles[0] = gbm_bo_get_handle_for_plane(data->connectorBOs[i], 0).u32;

            UInt32 pitches[4] = { 0 };
            pitches[0] = gbm_bo_get_stride_for_plane(data->connectorBOs[i], 0);

            UInt32 offsets[4] = { 0 };
            offsets[0] = gbm_bo_get_offset(data->connectorBOs[i], 0);

            UInt64 mods[4] = { 0 };
            mods[0] = connector->currentFormat.modifier;

            ret = drmModeAddFB2WithModifiers(connector->device->fd,
                                             connector->currentMode->info.hdisplay,
                                             connector->currentMode->info.vdisplay,
                                             connector->currentFormat.format,
                                             boHandles,
                                             pitches,
                                             offsets,
                                             mods,
                                             &data->connectorDRMFramebuffers[i],
                                             DRM_MODE_FB_MODIFIERS);

            if (ret == 0)
                goto skipLegacy;
        }

        ret = drmModeAddFB(connector->device->fd,
                           connector->currentMode->info.hdisplay,
                           connector->currentMode->info.vdisplay,
                           24,
                           gbm_bo_get_bpp(data->connectorBOs[i]),
                           gbm_bo_get_stride(data->connectorBOs[i]),
                           gbm_bo_get_handle(data->connectorBOs[i]).u32,
                           &data->connectorDRMFramebuffers[i]);

        if (ret)
        {
            SRMError("Failed o create DRM framebuffer %d for device %s connector %d (PRIME MODE).",
                     i,
                     connector->device->name,
                     connector->id);
            return 0;
        }

skipLegacy:

        data->rendererBuffers[i] = srmBufferCreateFromGBM(connector->device->rendererDevice->core, data->rendererBOs[i]);

        if (!data->rendererBuffers[i])
        {
            SRMWarning("Failed o create buffer %d from GBM bo for device %s connector %d (PRIME MODE).",
                       i,
                       connector->device->name,
                       connector->id);
        }
        else
        {
            if (srmBufferGetTextureID(connector->device->rendererDevice, data->rendererBuffers[i]) == 0)
            {
                SRMWarning("Failed o get texture ID from buffer %d on device %s connector %d (PRIME MODE).",
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

    for (UInt32 i = 0; i < data->buffersCount; i++)
    {
        if (data->connectorDRMFramebuffers[i] != 0)
        {
            drmModeRmFB(connector->device->fd, data->connectorDRMFramebuffers[i]);
            data->connectorDRMFramebuffers[i] = 0;
        }

        if (data->rendererBuffers[i])
        {
            srmBufferDestroy(data->rendererBuffers[i]);
            data->rendererBuffers[i] = NULL;
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

    eglMakeCurrent(connector->device->rendererDevice->eglDisplay,
                   data->rendererEGLSurface,
                   data->rendererEGLSurface,
                   data->rendererEGLContext);

    connector->interface->paintGL(connector, connector->interfaceData);

    return 1;
}

static void drawTexture(SRMConnector *connector, Int32 x, Int32 y, Int32 w, Int32 h)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;
    Int32 v = connector->currentMode->info.vdisplay - h - y;
    glScissor(x, v, w, h);
    glViewport(x, v, w, h);
    glUniform4f(data->srcRectUniform, x, y, w, h);
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

    if (connector->device->isBootVGA && connector->damageBoxesCount > 0)
    {
        damage = connector->damageBoxes;
        damageCount = connector->damageBoxesCount;
    }
    else
    {
        damage = &defaultDamage;
        damageCount = 1;
    }

    swapBuffers(connector, connector->device->rendererDevice->eglDisplay, data->rendererEGLSurface);
    gbm_surface_lock_front_buffer(data->rendererGBMSurface);

    eglMakeCurrent(connector->device->eglDisplay,
                   data->connectorEGLSurface,
                   data->connectorEGLSurface,
                   data->connectorEGLContext);

    glDisable(GL_BLEND);
    glActiveTexture(GL_TEXTURE0);
    glUniform2f(data->texSizeUniform, connector->currentMode->info.hdisplay, connector->currentMode->info.vdisplay);
    glBindTexture(GL_TEXTURE_2D, srmBufferGetTextureID(connector->device, data->primeBuffers[data->currentBufferIndex]));
    glUseProgram(data->programObject);
    glUniform1i(data->activeTextureUniform, 0);

    for (UInt32 i = 0; i < damageCount; i++)
    {
        drawTexture(connector,
                    damage[i].x1,
                    damage[i].y1,
                    damage[i].x2 - damage[i].x1,
                    damage[i].y2 - damage[i].y1);
    }

    swapBuffers(connector, connector->device->eglDisplay, data->connectorEGLSurface);
    gbm_surface_lock_front_buffer(data->connectorGBMSurface);

    srmRenderModeCommonPageFlip(connector, data->connectorDRMFramebuffers[data->currentBufferIndex]);

    data->currentBufferIndex = nextBufferIndex(connector);
    gbm_surface_release_buffer(data->rendererGBMSurface, data->rendererBOs[data->currentBufferIndex]);
    gbm_surface_release_buffer(data->connectorGBMSurface, data->connectorBOs[data->currentBufferIndex]);
    connector->interface->pageFlipped(connector, connector->interfaceData);
    return 1;
}

static UInt8 initCrtc(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    eglMakeCurrent(connector->device->rendererDevice->eglDisplay,
                   data->rendererEGLSurface,
                   data->rendererEGLSurface,
                   data->rendererEGLContext);

    return srmRenderModeCommonInitCrtc(connector, data->connectorDRMFramebuffers[nextBufferIndex(connector)]);
}

static void uninitialize(SRMConnector *connector)
{
    if (connector->renderData)
    {
        srmRenderModeCommonUninitialize(connector);
        destroyGLES2(connector);
        destroyEGLSurfaces(connector);
        destroyEGLContext(connector);
        destroyDRMFramebuffers(connector);
        destroyGBMSurfaces(connector);
        destroyData(connector);
    }
}

static UInt8 initialize(SRMConnector *connector)
{
    connector->allowModifiers = 1;

retry:
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
    if (srmRenderModeCommonUpdateMode(connector, data->connectorDRMFramebuffers[data->currentBufferIndex]))
    {
        eglMakeCurrent(connector->device->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, connector->device->eglSharedContext);
        destroyDRMFramebuffers(connector);
        destroyGLES2(connector);
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

    return data->rendererBuffers[bufferIndex];
}

static void pauseRendering(SRMConnector *connector)
{
    srmRenderModeCommonPauseRendering(connector);
}

static void resumeRendering(SRMConnector *connector)
{
    RenderModeData *data = (RenderModeData*)connector->renderData;

    for (UInt32 i = 0; i < data->buffersCount; i++)
    {
        gbm_surface_release_buffer(data->connectorGBMSurface, data->connectorBOs[i]);
        gbm_surface_release_buffer(data->rendererGBMSurface, data->rendererBOs[i]);
    }

    srmRenderModeCommonResumeRendering(connector, data->connectorDRMFramebuffers[data->currentBufferIndex]);
}

void srmRenderModePrimeSetInterface(SRMConnector *connector)
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
