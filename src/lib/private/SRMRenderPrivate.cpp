#include <private/SRMConnectorPrivate.h>
#include <private/SRMDevicePrivate.h>
#include <private/SRMConnectorModePrivate.h>
#include <private/SRMCrtcPrivate.h>

#include <unistd.h>
#include <stdlib.h>
#include <GLES2/gl2.h>
#include <sys/poll.h>

using namespace SRM;

#define SRM_FUNC SRMConnector::SRMConnectorPrivate

static const EGLint COM_eglContextAttribs[] =
{
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE
};

static const EGLint COM_eglConfigAttribs[] =
{
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_ALPHA_SIZE, 0,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_NONE
};

// Init GBM surfaces

static int ITS_createGBMSurfaces(SRMConnector *connector)
{
    connector->imp()->connectorGBMSurface = gbm_surface_create(
        connector->device()->imp()->gbm,
        connector->currentMode()->width(),
        connector->currentMode()->height(),
        GBM_FORMAT_XRGB8888,
        GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

    if (!connector->imp()->connectorGBMSurface)
    {
        fprintf(stderr, "Failed to create GBM surface for connector %d.\n", connector->id());
        return 0;
    }

    return 1;
}

static int DUM_createGBMSurfaces(SRMConnector *connector)
{
    connector->imp()->rendererGBMSurface = gbm_surface_create(
        connector->device()->rendererDevice()->imp()->gbm,
        connector->currentMode()->width(),
        connector->currentMode()->height(),
        GBM_FORMAT_XRGB8888,
        GBM_BO_USE_RENDERING);

    if (!connector->imp()->rendererGBMSurface)
    {
        fprintf(stderr, "Failed to create GBM surface for connector %d.\n", connector->id());
        return 0;
    }

    return 1;
}

static int CPU_createGBMSurfaces(SRMConnector *connector)
{
    connector->imp()->connectorGBMSurface = gbm_surface_create(
        connector->device()->imp()->gbm,
        connector->currentMode()->width(),
        connector->currentMode()->height(),
        GBM_FORMAT_XRGB8888,
        GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

    if (!connector->imp()->connectorGBMSurface)
    {
        fprintf(stderr, "Failed to create GBM surface for connector %d.\n", connector->id());
        return 0;
    }

    connector->imp()->rendererGBMSurface = gbm_surface_create(
        connector->device()->rendererDevice()->imp()->gbm,
        connector->currentMode()->width(),
        connector->currentMode()->height(),
        GBM_FORMAT_XRGB8888,
        GBM_BO_USE_RENDERING);

    if (!connector->imp()->rendererGBMSurface)
    {
        fprintf(stderr, "Failed to create GBM surface for connector %d.\n", connector->id());
        gbm_surface_destroy(connector->imp()->connectorGBMSurface);
        return 0;
    }

    return 1;
}

// Choose EGL configurations

static int COM_matchConfigToVisual(EGLDisplay egl_display, EGLint visual_id, EGLConfig *configs, int count)
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

static int COM_chooseEGLConfiguration(EGLDisplay egl_display, const EGLint *attribs, EGLint visual_id, EGLConfig *config_out)
{
    EGLint count = 0;
    EGLint matched = 0;
    EGLConfig *configs;
    int config_index = -1;

    if (!eglGetConfigs(egl_display, NULL, 0, &count) || count < 1)
    {
        //LLog::error("No EGL configs to choose from.\n");
        return 0;
    }

    configs = (void**)malloc(count * sizeof *configs);

    if (!configs)
        return 0;

    if (!eglChooseConfig(egl_display, attribs, configs, count, &matched) || !matched)
    {
        //LLog::error("No EGL configs with appropriate attributes.\n");
        goto out;
    }

    if (!visual_id)
    {
        config_index = 0;
    }

    if (config_index == -1)
    {
        config_index = COM_matchConfigToVisual(egl_display, visual_id, configs, matched);
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

static int ITS_getEGLConfigurations(SRMConnector *connector)
{
    if (!COM_chooseEGLConfiguration(connector->device()->imp()->eglDisplay,
                                COM_eglConfigAttribs,
                                GBM_FORMAT_XRGB8888,
                                &connector->imp()->connectorEGLConfig))
    {
        printf("Failed to choose EGL config.\n");
        return 0;
    }

    return 1;
}

static int DUM_getEGLConfigurations(SRMConnector *connector)
{
    if (!COM_chooseEGLConfiguration(connector->device()->rendererDevice()->imp()->eglDisplay,
                                COM_eglConfigAttribs,
                                GBM_FORMAT_XRGB8888,
                                &connector->imp()->connectorEGLConfig))
    {
        printf("Failed to choose EGL config.\n");
        return 0;
    }

    return 1;
}

static int CPU_getEGLConfigurations(SRMConnector *connector)
{
    if (!COM_chooseEGLConfiguration(connector->device()->imp()->eglDisplay,
                                COM_eglConfigAttribs,
                                GBM_FORMAT_XRGB8888,
                                &connector->imp()->connectorEGLConfig))
    {
        printf("Failed to choose EGL config.\n");
        return 0;
    }

    if (!COM_chooseEGLConfiguration(connector->device()->rendererDevice()->imp()->eglDisplay,
                                COM_eglConfigAttribs,
                                GBM_FORMAT_XRGB8888,
                                &connector->imp()->connectorEGLConfig))
    {
        printf("Failed to choose EGL config.\n");
        return 0;
    }

    return 1;
}

// Create EGL contexts

static int ITS_createEGLContexts(SRMConnector *connector)
{
    connector->imp()->connectorEGLContext = eglCreateContext(connector->device()->imp()->eglDisplay,
                                                             connector->imp()->connectorEGLConfig,
                                                             // It is EGL_NO_CONTEXT if no context was previously created in this device
                                                             connector->device()->imp()->eglSharedContext,
                                                             COM_eglContextAttribs);

    if (connector->imp()->connectorEGLContext == EGL_NO_CONTEXT)
        return 0;

    // Store it if is the first context created for this device, so that later on shared context can be created
    if (connector->device()->imp()->eglSharedContext == EGL_NO_CONTEXT)
        connector->device()->imp()->eglSharedContext = connector->imp()->connectorEGLContext;

    return 1;
}

static int DUM_createEGLContexts(SRMConnector *connector)
{
    connector->imp()->rendererEGLContext = eglCreateContext(connector->device()->rendererDevice()->imp()->eglDisplay,
                                                            connector->imp()->rendererEGLConfig,
                                                            connector->device()->rendererDevice()->imp()->eglSharedContext, // It is EGL_NO_CONTEXT if no context was previously created in this device
                                                            COM_eglContextAttribs);

    if (connector->imp()->rendererEGLContext == EGL_NO_CONTEXT)
        return 0;

    // Store it if is the first context created for this device, so that later on shared context can be created
    if (connector->device()->rendererDevice()->imp()->eglSharedContext == EGL_NO_CONTEXT)
        connector->device()->rendererDevice()->imp()->eglSharedContext = connector->imp()->rendererEGLContext;

    return 1;
}

static int CPU_createEGLContexts(SRMConnector *connector)
{
    connector->imp()->connectorEGLContext = eglCreateContext(connector->device()->imp()->eglDisplay,
                                                             connector->imp()->connectorEGLConfig,
                                                             // It is EGL_NO_CONTEXT if no context was previously created in this device
                                                             connector->device()->imp()->eglSharedContext,
                                                             COM_eglContextAttribs);

    if (connector->imp()->connectorEGLContext == EGL_NO_CONTEXT)
        return 0;

    bool wasNoContext = connector->device()->imp()->eglSharedContext == EGL_NO_CONTEXT;

    if (wasNoContext)
        connector->device()->imp()->eglSharedContext = connector->imp()->connectorEGLContext;

    connector->imp()->rendererEGLContext = eglCreateContext(connector->device()->rendererDevice()->imp()->eglDisplay,
                                                            connector->imp()->rendererEGLConfig,
                                                            connector->device()->rendererDevice()->imp()->eglSharedContext, // It is EGL_NO_CONTEXT if no context was previously created in this device
                                                            COM_eglContextAttribs);

    if (connector->imp()->rendererEGLContext == EGL_NO_CONTEXT)
    {
        eglDestroyContext(connector->device()->rendererDevice()->imp()->eglDisplay,
                          connector->imp()->connectorEGLContext);

        if (wasNoContext)
            connector->imp()->connectorEGLContext = EGL_NO_CONTEXT;

        return 0;
    }

    // Store it if is the first context created for this device, so that later on shared context can be created
    if (connector->device()->rendererDevice()->imp()->eglSharedContext == EGL_NO_CONTEXT)
        connector->device()->rendererDevice()->imp()->eglSharedContext = connector->imp()->rendererEGLContext;

    return 1;
}

// Create EGL surfaces

static int ITS_createEGLSurfaces(SRMConnector *connector)
{
    connector->imp()->connectorEGLSurface = eglCreateWindowSurface(connector->device()->imp()->eglDisplay,
                                                                   connector->imp()->connectorEGLConfig,
                                                                   connector->imp()->connectorGBMSurface,
                                                                   NULL);

    if (connector->imp()->connectorEGLSurface == EGL_NO_SURFACE)
    {
        printf("Failed to create EGL surface.\n");
        return 0;
    }

    return 1;
}

static int DUM_createEGLSurfaces(SRMConnector *connector)
{
    connector->imp()->rendererEGLSurface = eglCreateWindowSurface(connector->device()->rendererDevice()->imp()->eglDisplay,
                                                                   connector->imp()->rendererEGLConfig,
                                                                   connector->imp()->rendererGBMSurface,
                                                                   NULL);

    if (connector->imp()->rendererEGLSurface == EGL_NO_SURFACE)
    {
        printf("Failed to create EGL surface.\n");
        return 0;
    }

    return 1;
}

static int CPU_createEGLSurfaces(SRMConnector *connector)
{
    connector->imp()->connectorEGLSurface = eglCreateWindowSurface(connector->device()->imp()->eglDisplay,
                                                                   connector->imp()->connectorEGLConfig,
                                                                   connector->imp()->connectorGBMSurface,
                                                                   NULL);

    if (connector->imp()->connectorEGLSurface == EGL_NO_SURFACE)
    {
        printf("Failed to create EGL surface.\n");
        return 0;
    }

    connector->imp()->rendererEGLSurface = eglCreateWindowSurface(connector->device()->rendererDevice()->imp()->eglDisplay,
                                                                   connector->imp()->rendererEGLConfig,
                                                                   connector->imp()->rendererGBMSurface,
                                                                   NULL);

    if (connector->imp()->rendererEGLSurface == EGL_NO_SURFACE)
    {
        eglDestroySurface(connector->device()->imp()->eglDisplay,
                          connector->imp()->connectorEGLSurface);

        printf("Failed to create EGL surface.\n");
        return 0;
    }

    return 1;
}

// Create DRM framebuffers

static int ITS_createDRMFramebuffers(SRMConnector *connector)
{
    Int32 ret;

    eglMakeCurrent(connector->device()->imp()->eglDisplay,
                   connector->imp()->connectorEGLSurface,
                   connector->imp()->connectorEGLSurface,
                   connector->imp()->connectorEGLContext);

    // 2nd buffer

    eglSwapBuffers(connector->device()->imp()->eglDisplay, connector->imp()->connectorEGLSurface);

    connector->imp()->connectorBOs[1] = gbm_surface_lock_front_buffer(connector->imp()->connectorGBMSurface);

    ret = drmModeAddFB(connector->device()->fd(),
                       connector->currentMode()->width(),
                       connector->currentMode()->height(),
                       24,
                       gbm_bo_get_bpp(connector->imp()->connectorBOs[1]),
                       gbm_bo_get_stride(connector->imp()->connectorBOs[1]),
                       gbm_bo_get_handle(connector->imp()->connectorBOs[1]).u32,
                       &connector->imp()->connectorDRMFramebuffers[1]);

    if (ret)
    {
        printf("Failed o create 2nd DRM framebuffer for connector %d.\n", connector->id());
        return 0;
    }

    gbm_surface_release_buffer(connector->imp()->connectorGBMSurface, connector->imp()->connectorBOs[0]);

    // 1st buffer

    eglSwapBuffers(connector->device()->imp()->eglDisplay, connector->imp()->connectorEGLSurface);

    connector->imp()->connectorBOs[0] = gbm_surface_lock_front_buffer(connector->imp()->connectorGBMSurface);

    ret = drmModeAddFB(connector->device()->fd(),
                       connector->currentMode()->width(),
                       connector->currentMode()->height(),
                       24,
                       gbm_bo_get_bpp(connector->imp()->connectorBOs[0]),
                       gbm_bo_get_stride(connector->imp()->connectorBOs[0]),
                       gbm_bo_get_handle(connector->imp()->connectorBOs[0]).u32,
                       &connector->imp()->connectorDRMFramebuffers[0]);

    if (ret)
    {
        printf("Failed o create 1st DRM framebuffer for connector %d.\n", connector->id());
        return 1;
    }

    gbm_surface_release_buffer(connector->imp()->connectorGBMSurface, connector->imp()->connectorBOs[1]);

    connector->imp()->currentBufferIndex = 1;

    return 1;
}

// Do rendering

static int ITS_render(SRMConnector *connector)
{
    eglMakeCurrent(connector->device()->imp()->eglDisplay,
                   connector->imp()->connectorEGLSurface,
                   connector->imp()->connectorEGLSurface,
                   connector->imp()->connectorEGLContext);

    connector->imp()->interface->paintGL(connector, connector->imp()->interfaceData);

    return 1;
}

// Page flipping

static int ITS_pageFlip(SRMConnector *connector)
{
    eglSwapBuffers(connector->device()->imp()->eglDisplay, connector->imp()->connectorEGLSurface);
    gbm_surface_lock_front_buffer(connector->imp()->connectorGBMSurface);

    connector->imp()->pendingPageFlip = true;

    drmModePageFlip(connector->device()->fd(),
                    connector->currentCrtc()->id(),
                    connector->imp()->connectorDRMFramebuffers[connector->imp()->currentBufferIndex],
                    DRM_MODE_PAGE_FLIP_EVENT,
                    connector);

    // Prevent multiple threads invoking the drmHandleEvent at a time wich causes bugs
    // If more than 1 connector is requesting a page flip, both can be handled here
    // since the struct passed to drmHandleEvent is standard and could be handling events
    // from any connector (E.g. pageFlipHandler(conn1) or pageFlipHandler(conn2))
    connector->device()->imp()->pageFlipMutex.lock();

    pollfd fds;
    fds.fd = connector->device()->fd();
    fds.events = POLLIN;
    fds.revents = 0;

    while(connector->imp()->pendingPageFlip)
    {
        poll(&fds, 1, 100);

        if(fds.revents & POLLIN)
            drmHandleEvent(fds.fd, &connector->imp()->drmEventCtx);

        if(connector->state() != SRM_CONNECTOR_STATE_INITIALIZED)
            break;
    }

    connector->device()->imp()->pageFlipMutex.unlock();

    connector->imp()->currentBufferIndex = !connector->imp()->currentBufferIndex;

    gbm_surface_release_buffer(connector->imp()->connectorGBMSurface, connector->imp()->connectorBOs[connector->imp()->currentBufferIndex]);

    return 1;
}

static void COM_pageFlipHandler(int, unsigned int, unsigned int, unsigned int, void *data)
{
    SRMConnector *connector = (SRMConnector*)data;
    connector->imp()->pendingPageFlip = false;
}

static int COM_initCrtc(SRMConnector *connector)
{
    connector->imp()->drmEventCtx =
    {
        .version = DRM_EVENT_CONTEXT_VERSION,
        .vblank_handler = NULL,
        .page_flip_handler = &COM_pageFlipHandler,
        .page_flip_handler2 = NULL,
        .sequence_handler = NULL
    };

    eglSwapBuffers(connector->device()->imp()->eglDisplay, connector->imp()->connectorEGLSurface);
    gbm_surface_lock_front_buffer(connector->imp()->connectorGBMSurface);

    drmModeSetCrtc(connector->device()->fd(),
                   connector->currentCrtc()->id(),
                   connector->imp()->connectorDRMFramebuffers[connector->imp()->currentBufferIndex],
                   0,
                   0,
                   &connector->imp()->id,
                   1,
                   &connector->currentMode()->imp()->info);

    connector->imp()->currentBufferIndex = !connector->imp()->currentBufferIndex;

    gbm_surface_release_buffer(connector->imp()->connectorGBMSurface, connector->imp()->connectorBOs[connector->imp()->currentBufferIndex]);


    return 1;
}

// Cursor
static int COM_createCursor(SRMConnector *connector)
{
    connector->imp()->cursorBO = gbm_bo_create(connector->device()->imp()->gbm, 64, 64, GBM_FORMAT_ARGB8888, GBM_BO_USE_CURSOR | GBM_BO_USE_WRITE);
    return connector->imp()->cursorBO != NULL;
}

// Render lock
static void COM_waitForRepaintRequest(SRMConnector *connector)
{
    std::unique_lock<std::mutex> lock(connector->imp()->renderMutex);
    connector->imp()->renderConditionVariable.wait(lock,[connector]{return connector->imp()->repaintRequested;});
    connector->imp()->repaintRequested = false;
    lock.unlock();
}

// Setup renderer interface

static int COM_setupRendererInterface(SRMConnector *connector)
{
    switch (connector->device()->renderMode())
    {
        case SRM_RENDER_MODE_ITSELF:
        {
            connector->imp()->rendererInterface.createGBMSurfaces       = &ITS_createGBMSurfaces;
            connector->imp()->rendererInterface.getEGLConfiguration     = &ITS_getEGLConfigurations;
            connector->imp()->rendererInterface.createEGLContexts       = &ITS_createEGLContexts;
            connector->imp()->rendererInterface.createEGLSurfaces       = &ITS_createEGLSurfaces;
            connector->imp()->rendererInterface.createDRMFramebuffers   = &ITS_createDRMFramebuffers;
            connector->imp()->rendererInterface.render                  = &ITS_render;
            connector->imp()->rendererInterface.pageFlip                = &ITS_pageFlip;
            return 1;
        }break;
        case SRM_RENDER_MODE_DUMB:
        {
            connector->imp()->rendererInterface.createGBMSurfaces       = &DUM_createGBMSurfaces;
            connector->imp()->rendererInterface.getEGLConfiguration     = &DUM_getEGLConfigurations;
            connector->imp()->rendererInterface.createEGLContexts       = &DUM_createEGLContexts;
            connector->imp()->rendererInterface.createEGLSurfaces       = &DUM_createEGLSurfaces;
            //connector->imp()->rendererInterface.createDRMFramebuffers   = &DUM_createDRMFramebuffers;
            return 1;
        }break;
        case SRM_RENDER_MODE_CPU:
        {
            connector->imp()->rendererInterface.createGBMSurfaces       = &CPU_createGBMSurfaces;
            connector->imp()->rendererInterface.getEGLConfiguration     = &CPU_getEGLConfigurations;
            connector->imp()->rendererInterface.createEGLContexts       = &CPU_createEGLContexts;
            connector->imp()->rendererInterface.createEGLSurfaces       = &CPU_createEGLSurfaces;
            //connector->imp()->rendererInterface.createDRMFramebuffers   = &CPU_createDRMFramebuffers;
            return 1;
        }break;
    }
}

void SRM_FUNC::initRenderer(SRMConnector *connector, Int32 *initResult)
{

    if (!eglBindAPI(EGL_OPENGL_ES_API))
    {
        printf("Failed to bind GLES API.\n");
        *initResult = -1;
        return;
    }

    if (!COM_setupRendererInterface(connector))
    {
        printf("Failed to setup renderer interface.\n");
        *initResult = -1;
        return;
    }

    SRMRendererInterface *iface = &connector->imp()->rendererInterface;

    if (!iface->createGBMSurfaces(connector))
    {
        printf("Failed GBM surf.\n");
        *initResult = -1;
        return;
    }

    if (!iface->getEGLConfiguration(connector))
    {
        printf("Failed egl CONF.\n");
        *initResult = -1;
        return;
    }

    if (!iface->createEGLContexts(connector))
    {
        printf("Failed egl CONTex.\n");
        *initResult = -1;
        return;
    }

    if (!iface->createEGLSurfaces(connector))
    {
        printf("Failed egl surfs.\n");
        *initResult = -1;
        return;
    }

    if (!iface->createDRMFramebuffers(connector))
    {
        printf("Failed drm fbs.\n");
        *initResult = -1;
        return;
    }

    COM_createCursor(connector);

    *initResult = 1;

    connector->imp()->state = SRM_CONNECTOR_STATE_INITIALIZED;

    // User initializeGL
    connector->imp()->interface->initializeGL(connector, connector->imp()->interfaceData);

    COM_initCrtc(connector);

    while (1)
    {
        COM_waitForRepaintRequest(connector);
        connector->imp()->rendererInterface.render(connector);
        connector->imp()->rendererInterface.pageFlip(connector);
    }
}
