#include <private/SRMConnectorPrivate.h>
#include <private/SRMDevicePrivate.h>
#include <private/SRMConnectorModePrivate.h>
#include <private/SRMCrtcPrivate.h>
#include <private/SRMPlanePrivate.h>
#include <private/SRMCorePrivate.h>
#include <SRMLog.h>

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <drm_fourcc.h>
#include <drm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

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
        SRMLog::error("Failed to create GBM surface for connector %d (ITSELF MODE).", connector->id());
        return 0;
    }

    return 1;
}

static int DUM_createGBMSurfaces(SRMConnector *connector)
{
    (void)connector;

    // This mode does not need gbm Surfaces.

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
        SRMLog::error("Failed to create connector GBM surface for connector %d (CPU MODE).", connector->id());
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
        SRMLog::error("Failed to create renderer GBM surface for connector %d (CPU MODE).", connector->id());
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
        SRMLog::error("No EGL configs to choose from.");
        return 0;
    }

    configs = (void**)malloc(count * sizeof *configs);

    if (!configs)
        return 0;

    if (!eglChooseConfig(egl_display, attribs, configs, count, &matched) || !matched)
    {
        SRMLog::error("No EGL configs with appropriate attributes.");
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
        SRMLog::error("Failed to choose EGL configuration for connector %d (ITSELF MODE).", connector->id());
        return 0;
    }

    return 1;
}

static int DUM_getEGLConfigurations(SRMConnector *connector)
{
    EGLint n;
    eglChooseConfig(connector->device()->rendererDevice()->imp()->eglDisplay,
                    COM_eglConfigAttribs,
                    &connector->imp()->rendererEGLConfig,
                    1,
                    &n);
    if (n != 1)
    {
        SRMLog::error("Failed to choose renderer EGL configuration for connector %d (DUMB MODE).", connector->id());
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
        SRMLog::error("Failed to choose connector EGL configuration for connector %d (CPU MODE).", connector->id());
        return 0;
    }

    if (!COM_chooseEGLConfiguration(connector->device()->rendererDevice()->imp()->eglDisplay,
                                COM_eglConfigAttribs,
                                GBM_FORMAT_XRGB8888,
                                &connector->imp()->connectorEGLConfig))
    {
        SRMLog::error("Failed to choose renderer EGL configuration for connector %d (CPU MODE).", connector->id());
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
    {
        SRMLog::error("Failed to create EGL context for connector %d (ITSELF MODE).", connector->id());
        return 0;
    }

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
    {
        SRMLog::error("Failed to create EGL context for connector %d (DUMB MODE).", connector->id());
        return 0;
    }

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
    {
        SRMLog::error("Failed to create connector EGL context for connector %d (CPU MODE).", connector->id());
        return 0;
    }

    bool wasNoContext = connector->device()->imp()->eglSharedContext == EGL_NO_CONTEXT;

    if (wasNoContext)
        connector->device()->imp()->eglSharedContext = connector->imp()->connectorEGLContext;

    connector->imp()->rendererEGLContext = eglCreateContext(connector->device()->rendererDevice()->imp()->eglDisplay,
                                                            connector->imp()->rendererEGLConfig,
                                                            connector->device()->rendererDevice()->imp()->eglSharedContext, // It is EGL_NO_CONTEXT if no context was previously created in this device
                                                            COM_eglContextAttribs);

    if (connector->imp()->rendererEGLContext == EGL_NO_CONTEXT)
    {
        SRMLog::error("Failed to create renderer EGL context for connector %d (CPU MODE).", connector->id());

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
        SRMLog::error("Failed to create EGL surface for connector %d (ITSELF MODE).", connector->id());
        return 0;
    }

    return 1;
}

static int DUM_createEGLSurfaces(SRMConnector *connector)
{
    (void)connector;

    // This mode does not need EGL Surfaces.

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
        SRMLog::error("Failed to create connector EGL surface for connector %d (CPU MODE).", connector->id());
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

        SRMLog::error("Failed to create renderer EGL surface for connector %d (CPU MODE).", connector->id());
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
        SRMLog::error("Failed o create 2nd DRM framebuffer for connector %d (ITSELF MODE).", connector->id());
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
        SRMLog::error("Failed o create 1st DRM framebuffer for connector %d (ITSELF MODE).", connector->id());
        return 1;
    }

    gbm_surface_release_buffer(connector->imp()->connectorGBMSurface, connector->imp()->connectorBOs[1]);

    connector->imp()->currentBufferIndex = 1;

    return 1;
}

static int DUM_createDRMFramebuffers(SRMConnector *connector)
{
    Int32 ret;
    drm_mode_map_dumb dumbMapRequest;

    eglMakeCurrent(connector->device()->rendererDevice()->imp()->eglDisplay,
                   EGL_NO_SURFACE,
                   EGL_NO_SURFACE,
                   connector->imp()->rendererEGLContext);

    // The renderer GPU only needs one GLES2 renderbuffer since it is copied
    // to the DUMB Buffer map each frame
    glGenFramebuffers(1, &connector->imp()->dumbRendererFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, connector->imp()->dumbRendererFBO);
    glGenRenderbuffers(1, &connector->imp()->dumbRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, connector->imp()->dumbRenderbuffer);

    glRenderbufferStorage(GL_RENDERBUFFER,
                          GL_RGB8_OES,
                          connector->currentMode()->width(),
                          connector->currentMode()->height());

    glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                              GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER,
                              connector->imp()->dumbRenderbuffer);


    // 2nd buffer

    connector->imp()->dumbBuffers[1] =
    {
        .height = connector->currentMode()->height(),
        .width  = connector->currentMode()->width(),
        .bpp    = 32
    };

    ret = ioctl(connector->device()->fd(),
                DRM_IOCTL_MODE_CREATE_DUMB,
                &connector->imp()->dumbBuffers[1]);

    if (ret)
    {
        SRMLog::error("Failed to create 2nd dumb buffer for connector %d.", connector->id());
        goto FAIL_DUMB_CREATE_2;
    }

    dumbMapRequest.handle = connector->imp()->dumbBuffers[1].handle;

    ret = ioctl(connector->device()->fd(), DRM_IOCTL_MODE_MAP_DUMB, &dumbMapRequest);

    if (ret)
    {
        SRMLog::error("Failed to get the 2nd dumb buffer map offset for connector %d (DUMB MODE): %s.", connector->id(), strerror(errno));
        goto FAIL_DUMB_MAP_2;
    }

    connector->imp()->dumbMaps[1] = (UInt8*)mmap(0,
                                                 connector->imp()->dumbBuffers[1].size,
                                                 PROT_READ | PROT_WRITE,
                                                 MAP_SHARED,
                                                 connector->device()->fd(),
                                                 dumbMapRequest.offset);

    if (connector->imp()->dumbMaps[1] == NULL || connector->imp()->dumbMaps[1] == MAP_FAILED)
    {
        SRMLog::error("Failed to map the 2nd dumb buffer FD for connector %d (DUMB MODE).", connector->id());
        goto FAIL_DUMB_MAP_2;
    }

    ret = drmModeAddFB(connector->device()->fd(),
                       connector->currentMode()->width(),
                       connector->currentMode()->height(),
                       24,
                       connector->imp()->dumbBuffers[1].bpp,
                       connector->imp()->dumbBuffers[1].pitch,
                       connector->imp()->dumbBuffers[1].handle,
                       &connector->imp()->connectorDRMFramebuffers[1]);

    if (ret)
    {
        SRMLog::error("Failed to create 2nd DRM framebuffer for connector %d (DUMB MODE).", connector->id());
        goto FAIL_FB_CREATE_2;
    }

    // 1st buffer

    connector->imp()->dumbBuffers[0] =
    {
        .height = connector->currentMode()->height(),
        .width  = connector->currentMode()->width(),
        .bpp    = 32
    };

    ret = ioctl(connector->device()->fd(),
                DRM_IOCTL_MODE_CREATE_DUMB,
                &connector->imp()->dumbBuffers[0]);

    if (ret)
    {
        SRMLog::error("Failed to create 1st dumb buffer for connector %d.", connector->id());
        goto FAIL_DUMB_CREATE_1;
    }

    dumbMapRequest.handle = connector->imp()->dumbBuffers[0].handle;

    ret = ioctl(connector->device()->fd(), DRM_IOCTL_MODE_MAP_DUMB, &dumbMapRequest);

    if (ret)
    {
        SRMLog::error("Failed to get the 1st dumb buffer map offset for connector %d (DUMB MODE): %s.", connector->id(), strerror(errno));
        goto FAIL_DUMB_MAP_1;
    }

    connector->imp()->dumbMaps[0] = (UInt8*)mmap(0,
                                                 connector->imp()->dumbBuffers[0].size,
                                                 PROT_READ | PROT_WRITE,
                                                 MAP_SHARED,
                                                 connector->device()->fd(),
                                                 dumbMapRequest.offset);

    if (connector->imp()->dumbMaps[0] == NULL || connector->imp()->dumbMaps[0] == MAP_FAILED)
    {
        SRMLog::error("Failed to map the 1st dumb buffer FD for connector %d (DUMB MODE).", connector->id());
        goto FAIL_DUMB_MAP_1;
    }

    ret = drmModeAddFB(connector->device()->fd(),
                       connector->currentMode()->width(),
                       connector->currentMode()->height(),
                       24,
                       connector->imp()->dumbBuffers[0].bpp,
                       connector->imp()->dumbBuffers[0].pitch,
                       connector->imp()->dumbBuffers[0].handle,
                       &connector->imp()->connectorDRMFramebuffers[0]);

    if (ret)
    {
        SRMLog::error("Failed to create 1st DRM framebuffer for connector %d (DUMB MODE).", connector->id());
        goto FAIL_FB_CREATE_1;
    }

    return 1;

    FAIL_FB_CREATE_1:

        munmap(connector->imp()->dumbMaps[0],
               connector->imp()->dumbBuffers[0].size);

    FAIL_DUMB_MAP_1:

        ioctl(connector->device()->fd(),
              DRM_IOCTL_MODE_DESTROY_DUMB,
              &connector->imp()->dumbBuffers[0].handle);

    FAIL_DUMB_CREATE_1:

        drmModeRmFB(connector->device()->fd(),
                    connector->imp()->connectorDRMFramebuffers[1]);

    FAIL_FB_CREATE_2:

        munmap(connector->imp()->dumbMaps[1],
               connector->imp()->dumbBuffers[1].size);

    FAIL_DUMB_MAP_2:

        ioctl(connector->device()->fd(),
              DRM_IOCTL_MODE_DESTROY_DUMB,
              &connector->imp()->dumbBuffers[1].handle);

    FAIL_DUMB_CREATE_2:

        return 0;
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

static int DUM_render(SRMConnector *connector)
{
    eglMakeCurrent(connector->device()->rendererDevice()->imp()->eglDisplay,
                   EGL_NO_SURFACE,
                   EGL_NO_SURFACE,
                   connector->imp()->rendererEGLContext);

    glBindFramebuffer(GL_FRAMEBUFFER, connector->imp()->dumbRendererFBO);

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

static int DUM_pageFlip(SRMConnector *connector)
{
    UInt32 b = connector->imp()->currentBufferIndex;
    UInt32 h = connector->imp()->dumbBuffers[b].height;
    UInt32 w = connector->imp()->dumbBuffers[b].width;
    UInt32 p = connector->imp()->dumbBuffers[b].pitch;

    auto start = std::chrono::high_resolution_clock::now();

    for (UInt32 i = 0; i < h; i++)
    {
        glReadPixels(0,
                     h-i-1,
                     w,
                     1,
                     GL_BGRA_EXT,
                     GL_UNSIGNED_BYTE,
                     &connector->imp()->dumbMaps[b][
                     i*p]);
    }

    // Get the ending time point
    auto end = std::chrono::high_resolution_clock::now();

    // Calculate the time difference in milliseconds
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    printf("glReadLapse %ld ms\n",duration.count());

    if (connector->device()->clientCapAtomic())
    {
        drmModeAtomicReqPtr req;
        req = drmModeAtomicAlloc();

        // Plane

        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane()->id(),
                                 connector->currentPrimaryPlane()->imp()->propIDs.FB_ID,
                                 connector->imp()->connectorDRMFramebuffers[b]);

        auto start = std::chrono::high_resolution_clock::now();

        connector->imp()->pendingPageFlip = true;

        // Commit
        drmModeAtomicCommit(connector->device()->fd(),
                            req,
                            DRM_MODE_PAGE_FLIP_EVENT | DRM_MODE_ATOMIC_NONBLOCK,
                            connector);

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

        // Get the ending time point
        auto end = std::chrono::high_resolution_clock::now();

        // Calculate the time difference in milliseconds
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        printf("ATOMIC COMMIT %ld ms\n",duration.count());

        drmModeAtomicFree(req);
    }
    else
    {
        connector->imp()->pendingPageFlip = true;

        drmModePageFlip(connector->device()->fd(),
                        connector->currentCrtc()->id(),
                        connector->imp()->connectorDRMFramebuffers[b],
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
    }

    connector->imp()->currentBufferIndex = !b;

    return 1;
}

static void COM_pageFlipHandler(int, unsigned int, unsigned int, unsigned int, void *data)
{
    SRMConnector *connector = (SRMConnector*)data;
    connector->imp()->pendingPageFlip = false;
}

static int ITS_initCrtc(SRMConnector *connector)
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


    Int32 ret = drmModeSetCrtc(connector->device()->fd(),
                   connector->currentCrtc()->id(),
                   connector->imp()->connectorDRMFramebuffers[connector->imp()->currentBufferIndex],
                   0,
                   0,
                   &connector->imp()->id,
                   1,
                   &connector->currentMode()->imp()->info);

    if (ret)
    {
        SRMLog::error("Failed to set crtc mode on connector %d.", connector->id());
        return 0;
    }

    connector->imp()->currentBufferIndex = !connector->imp()->currentBufferIndex;

    gbm_surface_release_buffer(connector->imp()->connectorGBMSurface, connector->imp()->connectorBOs[connector->imp()->currentBufferIndex]);

    return 1;
}

static int DUM_initCrtc(SRMConnector *connector)
{
    connector->imp()->drmEventCtx =
    {
        .version = DRM_EVENT_CONTEXT_VERSION,
        .vblank_handler = NULL,
        .page_flip_handler = &COM_pageFlipHandler,
        .page_flip_handler2 = NULL,
        .sequence_handler = NULL
    };

    UInt32 b = connector->imp()->currentBufferIndex;
    UInt32 h = connector->imp()->dumbBuffers[b].height;
    UInt32 w = connector->imp()->dumbBuffers[b].width;
    UInt32 p = connector->imp()->dumbBuffers[b].pitch;

    glBindFramebuffer(GL_FRAMEBUFFER, connector->imp()->dumbRendererFBO);

    for (UInt32 i = 0; i < h; i++)
    {
        glReadPixels(0,
                     h-i-1,
                     w,
                     1,
                     GL_BGRA_EXT,
                     GL_UNSIGNED_BYTE,
                     &connector->imp()->dumbMaps[b][
                     i*p]);
    }

    if (connector->device()->clientCapAtomic())
    {
        drmModeAtomicReqPtr req;
        req = drmModeAtomicAlloc();

        UInt32 modeID;

        drmModeCreatePropertyBlob(connector->device()->fd(),
                                  &connector->currentMode()->imp()->info,
                                  sizeof(drmModeModeInfo),
                                  &modeID);

        drmModeAtomicAddProperty(req,
                                 connector->currentCrtc()->id(),
                                 connector->currentCrtc()->imp()->propIDs.MODE_ID,
                                 modeID);

        drmModeAtomicAddProperty(req,
                                 connector->currentCrtc()->id(),
                                 connector->currentCrtc()->imp()->propIDs.ACTIVE,
                                 1);

        // Connector

        drmModeAtomicAddProperty(req,
                                 connector->id(),
                                 connector->imp()->propIDs.CRTC_ID,
                                 connector->currentCrtc()->id());

        drmModeAtomicAddProperty(req,
                                 connector->id(),
                                 connector->imp()->propIDs.link_status,
                                 DRM_MODE_LINK_STATUS_GOOD);


        // Plane

        /*
        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane()->id(),
                                 connector->currentPrimaryPlane()->imp()->propIDs.rotation,
                                 DRM_MODE_REFLECT_Y); */

        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane()->id(),
                                 connector->currentPrimaryPlane()->imp()->propIDs.CRTC_ID,
                                 connector->currentCrtc()->id());

        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane()->id(),
                                 connector->currentPrimaryPlane()->imp()->propIDs.CRTC_X,
                                 0);


        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane()->id(),
                                 connector->currentPrimaryPlane()->imp()->propIDs.CRTC_Y,
                                 0);

        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane()->id(),
                                 connector->currentPrimaryPlane()->imp()->propIDs.CRTC_W,
                                 connector->currentMode()->width());

        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane()->id(),
                                 connector->currentPrimaryPlane()->imp()->propIDs.CRTC_H,
                                 connector->currentMode()->height());

        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane()->id(),
                                 connector->currentPrimaryPlane()->imp()->propIDs.FB_ID,
                                 connector->imp()->connectorDRMFramebuffers[b]);

        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane()->id(),
                                 connector->currentPrimaryPlane()->imp()->propIDs.SRC_X,
                                 0);

        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane()->id(),
                                 connector->currentPrimaryPlane()->imp()->propIDs.SRC_Y,
                                 0);

        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane()->id(),
                                 connector->currentPrimaryPlane()->imp()->propIDs.SRC_W,
                                 (UInt64)connector->currentMode()->width() << 16);

        drmModeAtomicAddProperty(req,
                                 connector->currentPrimaryPlane()->id(),
                                 connector->currentPrimaryPlane()->imp()->propIDs.SRC_H,
                                 (UInt64)connector->currentMode()->height() << 16);

        // Commit
        drmModeAtomicCommit(connector->device()->fd(),
                            req,
                            DRM_MODE_ATOMIC_ALLOW_MODESET,
                            connector);

        drmModeAtomicFree(req);


    }
    else
    {
        drmModeSetCrtc(connector->device()->fd(),
                       connector->currentCrtc()->id(),
                       connector->imp()->connectorDRMFramebuffers[b],
                       0,
                       0,
                       &connector->imp()->id,
                       1,
                       &connector->currentMode()->imp()->info);
    }

    connector->imp()->currentBufferIndex = !b;

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
            connector->imp()->rendererInterface.initCrtc                = &ITS_initCrtc;
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
            connector->imp()->rendererInterface.createDRMFramebuffers   = &DUM_createDRMFramebuffers;
            connector->imp()->rendererInterface.initCrtc                = &DUM_initCrtc;
            connector->imp()->rendererInterface.render                  = &DUM_render;
            connector->imp()->rendererInterface.pageFlip                = &DUM_pageFlip;
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
        SRMLog::error("Failed to bind GLES API for connector %d.", connector->id());
        *initResult = -1;
        return;
    }

    if (!COM_setupRendererInterface(connector))
    {
        SRMLog::error("Failed to setup renderer interface for connector %d.", connector->id());
        *initResult = -1;
        return;
    }

    SRMRendererInterface *iface = &connector->imp()->rendererInterface;

    if (!iface->createGBMSurfaces(connector))
    {
        SRMLog::error("Failed to create GBM surfaces for connector %d.", connector->id());
        *initResult = -1;
        return;
    }

    if (!iface->getEGLConfiguration(connector))
    {
        SRMLog::error("Failed to get a valid EGL configuration for connector %d.", connector->id());
        *initResult = -1;
        return;
    }

    if (!iface->createEGLContexts(connector))
    {
        SRMLog::error("Failed to create EGL contexts for connector %d.", connector->id());
        *initResult = -1;
        return;
    }

    if (!iface->createEGLSurfaces(connector))
    {
        SRMLog::error("Failed to create EGL surfaces for connector %d.", connector->id());
        *initResult = -1;
        return;
    }

    if (!iface->createDRMFramebuffers(connector))
    {
        SRMLog::error("Failed to create DRM framebuffers for connector %d.", connector->id());
        *initResult = -1;
        return;
    }

    if (!COM_createCursor(connector))
        SRMLog::warning("Failed to create hardware cursor for connector %d.", connector->id());

    *initResult = 1;

    connector->imp()->state = SRM_CONNECTOR_STATE_INITIALIZED;

    // User initializeGL
    connector->imp()->interface->initializeGL(connector, connector->imp()->interfaceData);

    if (!iface->initCrtc(connector))
    {
        printf("BIG F");
        exit(1);
        return;
    }

    while (1)
    {
        COM_waitForRepaintRequest(connector);
        connector->imp()->rendererInterface.render(connector);
        connector->imp()->rendererInterface.pageFlip(connector);
    }
}
