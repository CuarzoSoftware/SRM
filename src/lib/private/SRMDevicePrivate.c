#include <private/SRMListenerPrivate.h>
#include <private/SRMCorePrivate.h>
#include <private/SRMDevicePrivate.h>
#include <private/SRMCrtcPrivate.h>
#include <private/SRMEncoderPrivate.h>
#include <private/SRMPlanePrivate.h>
#include <private/SRMConnectorPrivate.h>
#include <private/SRMBufferPrivate.h>
#include <private/modes/SRMRenderModeCommon.h>

#include <SRMFormat.h>
#include <SRMList.h>
#include <SRMLog.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

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

static UInt8 srmDeviceInBlacklist(const char *deviceName)
{
    char *blacklist = getenv("SRM_DEVICES_BLACKLIST");

    if (!blacklist)
        return 0;

    size_t extlen = strlen(deviceName);
    const char *end = blacklist + strlen(blacklist);

    while (blacklist < end)
    {
        if (*blacklist == ':')
        {
            blacklist++;
            continue;
        }

        size_t n = strcspn(blacklist, ":");

        if (n == extlen && strncmp(deviceName, blacklist, n) == 0)
            return 1;

        blacklist += n;
    }

    return 0;
}

SRMDevice *srmDeviceCreate(SRMCore *core, const char *name, UInt8 isBootVGA)
{
    if (srmDeviceInBlacklist(name))
    {
        SRMWarning("[device] %s is blacklisted. Ignoring it.", name);
        return NULL;
    }

    // REF 1
    SRMDevice *device = calloc(1, sizeof(SRMDevice));
    strncpy(device->name, name, sizeof(device->name) - 1);
    device->core = core;
    device->enabled = 1;
    device->isBootVGA = isBootVGA;
    device->eglDevice = EGL_NO_DEVICE_EXT;
    device->eglSurfaceTest = EGL_NO_SURFACE;
    device->fd = -1;

    SRMDebug("[%s] Is boot VGA: %s.", device->name, device->isBootVGA ?  "YES" : "NO");

    // REF 2
    device->fd = core->interface->openRestricted(name,
                                                 O_RDWR | O_CLOEXEC,
                                                 core->interfaceUserData);

    if (device->fd < 0)
    {
        SRMError("[device] Failed to open DRM device %s.", device->name);
        goto fail;
    }

    SRMDebug("[%s] Is master: %s.", device->name, drmIsMaster(device->fd) ?  "YES" : "NO");

    drmVersion *version = drmGetVersion(device->fd);

    if (version)
    {
        SRMDebug("[%s] DRM Driver: %s.", device->name, version->name);

        if (strcmp(version->name, "i915") == 0)
            device->driver = SRM_DEVICE_DRIVER_i915;
        else if (strcmp(version->name, "nouveau") == 0)
            device->driver = SRM_DEVICE_DRIVER_nouveau;
        else if (strcmp(version->name, "lima") == 0)
            device->driver = SRM_DEVICE_DRIVER_lima;
        else if (strcmp(version->name, "nvidia-drm") == 0)
            device->driver = SRM_DEVICE_DRIVER_nvidia;
        else if (strcmp(version->name, "nvidia") == 0)
            device->driver = SRM_DEVICE_DRIVER_nvidia;

        drmFreeVersion(version);
    }

    // REF 3
    if (pthread_mutex_init(&device->pageFlipMutex, NULL))
    {
        SRMError("Failed to create page flip mutex for device %s.", device->name);
        goto fail;
    }
    device->pageFlipMutexInitialized = 1;

    // REF 4
    if (!srmDeviceInitializeGBM(device))
        goto fail;

    // REF 5
    if (!srmDeviceInitializeEGL(device))
        goto fail;

    // REF -
    if (!srmDeviceUpdateEGLExtensions(device))
        goto fail;

    // REF 5-1
    if (!srmDeviceInitializeEGLSharedContext(device))
        goto fail;

    // REF -
    if (!srmDeviceUpdateGLExtensions(device))
        goto fail;

    // REF -
    if (!srmDeviceUpdateEGLFunctions(device))
        goto fail;

    // REF 6
    srmDeviceUpdateDMAFormats(device);

    // REF 7-1
    if (!srmDeviceInitializeTestGBMSurface(device))
        goto fail;

    // REF 7-2
    if(!srmDeviceInitializeTestEGLSurface(device))
        goto fail;

    // REF 7-3
    if (!srmDeviceInitializeTestShader(device))
        goto fail;

    // REF 8
    if (!srmDeviceInitEGLDeallocatorContext(device))
        goto fail;

    // REF -
    if (!srmDeviceUpdateClientCaps(device))
        goto fail;

    // REF -
    if (!srmDeviceUpdateCaps(device))
        goto fail;

    // REF 9
    device->crtcs = srmListCreate();
    if (!srmDeviceUpdateCrtcs(device))
        goto fail;

    // REF 10
    device->encoders = srmListCreate();
    if (!srmDeviceUpdateEncoders(device))
        goto fail;

    // REF 11
    device->planes = srmListCreate();
    if (!srmDeviceUpdatePlanes(device))
        goto fail;

    // REF 12
    device->connectors = srmListCreate();
    if (!srmDeviceUpdateConnectors(device))
        goto fail;

    srmDeviceTestCPUAllocationMode(device);

    return device;

    fail:
    srmDeviceDestroy(device);
    return NULL;
}

void srmDeviceDestroy(SRMDevice *device)
{
    // UNREF 12
    if (device->connectors)
    {
        while (!srmListIsEmpty(device->connectors))
            srmConnectorDestroy(srmListItemGetData(srmListGetBack(device->connectors)));

        srmListDestroy(device->connectors);
    }

    // UNREF 11
    if (device->planes)
    {
        while (!srmListIsEmpty(device->planes))
            srmPlaneDestroy(srmListItemGetData(srmListGetBack(device->planes)));

        srmListDestroy(device->planes);
    }

    // UNREF 10
    if (device->encoders)
    {
        while (!srmListIsEmpty(device->encoders))
            srmEncoderDestroy(srmListItemGetData(srmListGetBack(device->encoders)));

        srmListDestroy(device->encoders);
    }

    // UNREF 9
    if (device->crtcs)
    {
        while (!srmListIsEmpty(device->crtcs))
            srmCrtcDestroy(srmListItemGetData(srmListGetBack(device->crtcs)));

        srmListDestroy(device->crtcs);
    }

    // UNREF 8
    srmDeviceUninitEGLDeallocatorContext(device);

    // UNREF 7-3
    srmDeviceUninitializeTestShader(device);

    // UNREF 7-2
    srmDeviceUninitializeTestEGLSurface(device);

    // UNREF 7-1
    srmDeviceUninitializeTestGBMSurface(device);

    // UNREF 6
    srmDeviceDestroyDMAFormats(device);

    // UNREF 5-1
    srmDeviceUninitializeEGLSharedContext(device);

    // UNREF 5
    srmDeviceUninitializeEGL(device);

    // UNREF 4
    srmDeviceUninitializeGBM(device);

    // UNREF 3
    if (device->pageFlipMutexInitialized)
        pthread_mutex_destroy(&device->pageFlipMutex);

    // UNREF 2
    if (device->fd >= 0)
        device->core->interface->closeRestricted(device->fd, device->core->interfaceUserData);

    if (device->coreLink)
        srmListRemoveItem(device->core->devices, device->coreLink);

    // UNREF 1
    free(device);
}

UInt8 srmDeviceInitializeGBM(SRMDevice *device)
{
    device->gbm = gbm_create_device(device->fd);

    if (!device->gbm)
    {
        SRMError("Failed to initialize GBM on device %s.", device->name);
        return 0;
    }

    return 1;
}

void srmDeviceUninitializeGBM(SRMDevice *device)
{
    if (device->gbm)
        gbm_device_destroy(device->gbm);
}

UInt8 srmDeviceInitializeEGL(SRMDevice *device)
{
    #ifdef EGL_PLATFORM_GBM_KHR
        device->eglDisplay = eglGetPlatformDisplay(EGL_PLATFORM_GBM_KHR, device->gbm, NULL);
    #else
        device->eglDisplay = eglGetDisplay(device->gbm);
    #endif

    if (device->eglDisplay == EGL_NO_DISPLAY)
    {
        SRMError("Failed to get EGL display for device %s.", device->name);
        return 0;
    }

    EGLint minor, major;

    if (!eglInitialize(device->eglDisplay, &minor, &major))
    {
        SRMError("Failed to initialize EGL display for device %s.", device->name);
        device->eglDisplay = EGL_NO_DISPLAY;
        return 0;
    }

    SRMDebug("[%s] EGL version: %d.%d.", device->name, minor, major);

    const char *vendor = eglQueryString(device->eglDisplay, EGL_VENDOR);
    SRMDebug("[%s] EGL vendor: %s.", device->name, vendor ? vendor : "Unknown");

    return 1;
}

void srmDeviceUninitializeEGL(SRMDevice *device)
{
    if (device->eglDisplay != EGL_NO_DISPLAY)
    {
        eglReleaseThread();
        eglMakeCurrent(device->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglTerminate(device->eglDisplay);
    }
}

UInt8 srmDeviceUpdateEGLExtensions(SRMDevice *device)
{
    const char *extensions = eglQueryString(device->eglDisplay, EGL_EXTENSIONS);

    if (!extensions)
    {
        SRMError("Failed to query EGL display extensions for device %s.", device->name);
        return 0;
    }

    SRMDebug("[%s] EGL Extensions: %s.", device->name, extensions);

    device->eglExtensions.KHR_image_base = srmEGLHasExtension(extensions, "EGL_KHR_image_base");
    device->eglExtensions.KHR_image = srmEGLHasExtension(extensions, "EGL_KHR_image");
    device->eglExtensions.EXT_image_dma_buf_import = srmEGLHasExtension(extensions, "EGL_EXT_image_dma_buf_import");
    device->eglExtensions.EXT_image_dma_buf_import_modifiers = srmEGLHasExtension(extensions, "EGL_EXT_image_dma_buf_import_modifiers");
    device->eglExtensions.EXT_create_context_robustness = srmEGLHasExtension(extensions, "EGL_EXT_create_context_robustness");
    device->eglExtensions.KHR_image_pixmap = srmEGLHasExtension(extensions, "EGL_KHR_image_pixmap");
    device->eglExtensions.KHR_gl_texture_2D_image = srmEGLHasExtension(extensions, "EGL_KHR_gl_texture_2D_image");
    device->eglExtensions.KHR_gl_renderbuffer_image = srmEGLHasExtension(extensions, "EGL_KHR_gl_renderbuffer_image");

    const char *deviceExtensions = NULL, *driverName = NULL;

    if (device->core->eglExtensions.EXT_device_query)
    {
        EGLAttrib deviceAttrib;

        if (!device->core->eglFunctions.eglQueryDisplayAttribEXT(device->eglDisplay, EGL_DEVICE_EXT, &deviceAttrib))
        {
            SRMError("eglQueryDisplayAttribEXT(EGL_DEVICE_EXT) failed for device %s.", device->name);
            goto skipDeviceQuery;
        }

        device->eglDevice = (EGLDeviceEXT)deviceAttrib;

        deviceExtensions = device->core->eglFunctions.eglQueryDeviceStringEXT(device->eglDevice, EGL_EXTENSIONS);

        if (!deviceExtensions)
        {
            SRMError("eglQueryDeviceStringEXT(EGL_EXTENSIONS) failed for device %s.", device->name);
            goto skipDeviceQuery;
        }

        SRMDebug("[%s] EGL Device Extensions: %s.", device->name, deviceExtensions);

        device->eglExtensions.MESA_device_software = srmEGLHasExtension(deviceExtensions, "EGL_MESA_device_software");

#ifdef EGL_DRIVER_NAME_EXT
        device->eglExtensions.EXT_device_persistent_id = srmEGLHasExtension(deviceExtensions, "EGL_EXT_device_persistent_id");

        if (device->eglExtensions.EXT_device_persistent_id)
            driverName = device->core->eglFunctions.eglQueryDeviceStringEXT(device->eglDevice, EGL_DRIVER_NAME_EXT);
#endif

        device->eglExtensions.EXT_device_drm = srmEGLHasExtension(deviceExtensions, "EGL_EXT_device_drm");
        device->eglExtensions.EXT_device_drm_render_node = srmEGLHasExtension(deviceExtensions, "EGL_EXT_device_drm_render_node");
    }

    skipDeviceQuery:

    device->eglExtensions.KHR_no_config_context = srmEGLHasExtension(extensions, "EGL_KHR_no_config_context");
    device->eglExtensions.MESA_configless_context = srmEGLHasExtension(extensions, "EGL_MESA_configless_context");
    device->eglExtensions.KHR_surfaceless_context = srmEGLHasExtension(extensions, "EGL_KHR_surfaceless_context");
    device->eglExtensions.IMG_context_priority = srmEGLHasExtension(extensions, "EGL_IMG_context_priority");

    SRMDebug("[%s] EGL driver name: %s.", device->name, driverName ? driverName : "Unknown");

    return 1;
}

UInt8 srmDeviceUpdateEGLFunctions(SRMDevice *device)
{
    if (device->eglExtensions.KHR_image_base || device->eglExtensions.KHR_image)
    {
        device->eglFunctions.eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC) eglGetProcAddress("eglCreateImageKHR");
        device->eglFunctions.eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC) eglGetProcAddress("eglDestroyImageKHR");

        if (device->glExtensions.OES_EGL_image || device->glExtensions.OES_EGL_image_base)
        {
            if (device->eglExtensions.KHR_gl_texture_2D_image)
                device->eglFunctions.glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) eglGetProcAddress("glEGLImageTargetTexture2DOES");

            if (device->eglExtensions.KHR_gl_renderbuffer_image)
                device->eglFunctions.glEGLImageTargetRenderbufferStorageOES = (PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC) eglGetProcAddress("glEGLImageTargetRenderbufferStorageOES");
        }
    }

    SRMDebug("[%s] Has glEGLImageTargetTexture2DOES: %s.",
             device->name,
             device->eglFunctions.glEGLImageTargetTexture2DOES == NULL ? "NO" : "YES");

    SRMDebug("[%s] Has glEGLImageTargetRenderbufferStorageOES: %s.",
             device->name,
             device->eglFunctions.glEGLImageTargetRenderbufferStorageOES == NULL ? "NO" : "YES");

    if (device->eglExtensions.EXT_image_dma_buf_import_modifiers)
    {
        device->eglFunctions.eglQueryDmaBufFormatsEXT = (PFNEGLQUERYDMABUFFORMATSEXTPROC) eglGetProcAddress("eglQueryDmaBufFormatsEXT");
        device->eglFunctions.eglQueryDmaBufModifiersEXT = (PFNEGLQUERYDMABUFMODIFIERSEXTPROC) eglGetProcAddress("eglQueryDmaBufModifiersEXT");
    }

    return 1;
}

UInt8 srmDeviceUpdateDMAFormats(SRMDevice *device)
{
    srmDeviceDestroyDMAFormats(device);

    device->dmaRenderFormats = srmListCreate();
    device->dmaExternalFormats = srmListCreate();
    device->dmaTextureFormats = srmListCreate();

    if (!device->eglExtensions.EXT_image_dma_buf_import)
    {
        SRMDebug("[%s] No EGL DMA formats (EXT_image_dma_buf_import not avaliable).", device->name);
        return 0;
    }

    EGLint formatsCount = 0;
    EGLint *formats = NULL;
    UInt8 allExternalOnly = 1;

    if (device->eglExtensions.EXT_image_dma_buf_import_modifiers)
    {
        if (!device->eglFunctions.eglQueryDmaBufFormatsEXT(device->eglDisplay, 0, NULL, &formatsCount))
        {
            SRMError("[%s] Failed to query the number of EGL DMA formats.", device->name);
            return 0;
        }

        if (formatsCount <= 0)
        {
            SRMError("[%s] No EGL DMA formats.", device->name);
            return 0;
        }

        formats = calloc(formatsCount, sizeof(formats[0]));

        if (!device->eglFunctions.eglQueryDmaBufFormatsEXT(device->eglDisplay, formatsCount, formats, &formatsCount))
        {
            SRMError("[%s] Failed to query EGL DMA formats.", device->name);
            free(formats);
            return 0;
        }
    }
    else
    {
        SRMError("[%s] Failed to query EGL DMA formats.", device->name);
        return 0;
    }

    for (Int32 i = 0; i < formatsCount; i++)
    {
        EGLint modifiersCount = 0;
        UInt64 *modifiers = NULL;
        EGLBoolean *externalOnly = NULL;

        // Get the format modifiers
        if (device->eglExtensions.EXT_image_dma_buf_import_modifiers)
        {
            if (!device->eglFunctions.eglQueryDmaBufModifiersEXT(device->eglDisplay, formats[i], 0, NULL, NULL, &modifiersCount))
                modifiersCount = 0;

            if (modifiersCount <= 0)
            {
                modifiersCount = 0;
                goto skipModifiers;
            }

            modifiers = calloc(modifiersCount, sizeof(UInt64));
            externalOnly = calloc(modifiersCount, sizeof(EGLBoolean));

            if (!device->eglFunctions.eglQueryDmaBufModifiersEXT(device->eglDisplay,
                                                                 formats[i],
                                                                 modifiersCount,
                                                                 modifiers,
                                                                 externalOnly,
                                                                 &modifiersCount))
            {
                modifiersCount = 0;
                free(modifiers);
                free(externalOnly);
                modifiers = NULL;
                externalOnly = NULL;
                free(formats);
                return -1;
            }

        }

        skipModifiers:

        for (Int32 j = 0; j < modifiersCount; j++)
        {
            srmFormatsListAddFormat(device->dmaTextureFormats, formats[i], modifiers[j]);

            if (externalOnly[j])
                srmFormatsListAddFormat(device->dmaExternalFormats, formats[i], modifiers[j]);
            else
            {
                srmFormatsListAddFormat(device->dmaRenderFormats, formats[i], modifiers[j]);
                allExternalOnly = 0;
            }
        }

        // EGL always supports implicit modifiers. If at least one modifier supports rendering,
        // assume the implicit modifier supports rendering too.

        srmFormatsListAddFormat(device->dmaTextureFormats, formats[i], DRM_FORMAT_MOD_INVALID);
        srmFormatsListAddFormat(device->dmaExternalFormats, formats[i], DRM_FORMAT_MOD_INVALID);
        if (modifiersCount == 0 || !allExternalOnly)
            srmFormatsListAddFormat(device->dmaRenderFormats, formats[i], DRM_FORMAT_MOD_INVALID);

        if (modifiers)
            free(modifiers);

        if (externalOnly)
            free(externalOnly);
    }

    free(formats);

    return 1;
}

void srmDeviceDestroyDMAFormats(SRMDevice *device)
{
    srmFormatsListDestroy(&device->dmaRenderFormats);
    srmFormatsListDestroy(&device->dmaExternalFormats);
    srmFormatsListDestroy(&device->dmaTextureFormats);
}

UInt8 srmDeviceInitializeEGLSharedContext(SRMDevice *device)
{
    if (!eglBindAPI(EGL_OPENGL_ES_API))
    {
        SRMError("Failed to bind GLES API for device %s.", device->name);
        return 0;
    }

    if (!srmRenderModeCommonChooseEGLConfiguration(
            device->eglDisplay,
            eglConfigAttribs,
            DRM_FORMAT_XRGB8888,
            &device->eglConfigTest))
    {
        SRMError("Failed to choose EGL configuration for device %s.", device->name);
        return 0;
    }

    size_t atti = 0;
    device->eglSharedContextAttribs[atti++] = EGL_CONTEXT_CLIENT_VERSION;
    device->eglSharedContextAttribs[atti++] = 2;

    if (device->eglExtensions.IMG_context_priority)
    {
        device->eglSharedContextAttribs[atti++] = EGL_CONTEXT_PRIORITY_LEVEL_IMG;
        device->eglSharedContextAttribs[atti++] = EGL_CONTEXT_PRIORITY_MEDIUM_IMG;
    }

    if (device->eglExtensions.EXT_create_context_robustness)
    {
        device->eglSharedContextAttribs[atti++] = EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT;
        device->eglSharedContextAttribs[atti++] = EGL_LOSE_CONTEXT_ON_RESET_EXT;
    }

    device->eglSharedContextAttribs[atti++] = EGL_NONE;
    device->eglSharedContext = eglCreateContext(device->eglDisplay, device->eglConfigTest, EGL_NO_CONTEXT, device->eglSharedContextAttribs);

    if (device->eglSharedContext == EGL_NO_CONTEXT)
    {
        SRMError("Failed to create shared EGL context for device %s.", device->name);
        return 0;
    }

    if (device->eglExtensions.IMG_context_priority)
    {
        EGLint priority = EGL_CONTEXT_PRIORITY_MEDIUM_IMG;
        eglQueryContext(device->eglDisplay, device->eglSharedContext, EGL_CONTEXT_PRIORITY_LEVEL_IMG, &priority);
        SRMDebug("[%s] Using %s priority EGL context.", device->name, priority == EGL_CONTEXT_PRIORITY_HIGH_IMG ? "high" : "medium");
    }

    eglMakeCurrent(device->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, device->eglSharedContext);

    return 1;
}

void srmDeviceUninitializeEGLSharedContext(SRMDevice *device)
{
    if (device->eglSharedContext != EGL_NO_CONTEXT)
    {
        eglReleaseThread();
        eglMakeCurrent(device->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(device->eglDisplay, device->eglSharedContext);
    }
}

UInt8 srmDeviceInitializeTestGBMSurface(SRMDevice *device)
{
    device->gbmSurfaceTest = srmBufferCreateGBMSurface(
        device->gbm,
        64, 64,
        DRM_FORMAT_XRGB8888,
        DRM_FORMAT_MOD_LINEAR,
        GBM_BO_USE_RENDERING);

    if (device->gbmSurfaceTest)
        return 1;

    device->gbmSurfaceTest = srmBufferCreateGBMSurface(
        device->gbm,
        64, 64,
        DRM_FORMAT_XRGB8888,
        DRM_FORMAT_MOD_INVALID,
        GBM_BO_USE_RENDERING);

    if (!device->gbmSurfaceTest)
    {
        SRMError("Failed to create GBM surface for device %s.", device->name);
        return 0;
    }

    return 1;
}

void srmDeviceUninitializeTestGBMSurface(SRMDevice *device)
{
    if (device->gbmSurfaceTest)
        gbm_surface_destroy(device->gbmSurfaceTest);
}


UInt8 srmDeviceInitializeTestEGLSurface(SRMDevice *device)
{
    device->eglSurfaceTest = eglCreateWindowSurface(
        device->eglDisplay,
        device->eglConfigTest,
        (EGLNativeWindowType)device->gbmSurfaceTest,
        NULL);

    if (device->eglSurfaceTest == EGL_NO_SURFACE)
    {
        SRMError("Failed to create EGL surface for device %s.", device->name);
        return 0;
    }

    eglMakeCurrent(device->eglDisplay,
                   device->eglSurfaceTest,
                   device->eglSurfaceTest,
                   device->eglSharedContext);

    eglSwapBuffers(device->eglDisplay,
                   device->eglSurfaceTest);

    device->gbmSurfaceTestBo = gbm_surface_lock_front_buffer(device->gbmSurfaceTest);
    return 1;
}

void srmDeviceUninitializeTestEGLSurface(SRMDevice *device)
{
    if (device->gbmSurfaceTestBo)
    {
        gbm_surface_release_buffer(device->gbmSurfaceTest, device->gbmSurfaceTestBo);
        device->gbmSurfaceTestBo = NULL;
    }

    if (device->gbmSurfaceTest)
    {
        gbm_surface_destroy(device->gbmSurfaceTest);
        device->gbmSurfaceTest = NULL;
    }

    if (device->eglSurfaceTest != EGL_NO_SURFACE)
    {
        eglDestroySurface(device->eglDisplay, device->eglSurfaceTest);
        device->eglSurfaceTest = EGL_NO_SURFACE;
    }
}

UInt8 srmDeviceInitializeTestShader(SRMDevice *device)
{
    static GLfloat square[] =
    {  //  VERTEX       FRAGMENT
        -1.0f, -1.0f,   0.f, 1.f, // TL
        -1.0f,  1.0f,   0.f, 0.f, // BL
        1.0f,  1.0f,   1.f, 0.f, // BR
        1.0f, -1.0f,   1.f, 1.f  // TR
    };

   const char *vertexShaderSource = "attribute vec4 position; varying vec2 v_texcoord; void main() { gl_Position = vec4(position.xy, 0.0, 1.0); v_texcoord = position.zw; }";
   const char *fragmentShaderSource = "precision mediump float; uniform sampler2D tex; varying vec2 v_texcoord; void main() { gl_FragColor = texture2D(tex, v_texcoord); }";

   eglMakeCurrent(device->eglDisplay,
                  device->eglSurfaceTest,
                  device->eglSurfaceTest,
                  device->eglSharedContext);

   GLint success = 0;
   GLchar infoLog[256];

   device->vertexShaderTest = glCreateShader(GL_VERTEX_SHADER);
   glShaderSource(device->vertexShaderTest, 1, &vertexShaderSource, NULL);
   glCompileShader(device->vertexShaderTest);
   glGetShaderiv(device->vertexShaderTest, GL_COMPILE_STATUS, &success);

   if (!success)
   {
       glGetShaderInfoLog(device->vertexShaderTest, sizeof(infoLog), NULL, infoLog);
       SRMFatal("[SRMDevice] Vertex shader compilation error: %s.", infoLog);
       return 0;
   }

   device->fragmentShaderTest = glCreateShader(GL_FRAGMENT_SHADER);
   glShaderSource(device->fragmentShaderTest, 1, &fragmentShaderSource, NULL);
   glCompileShader(device->fragmentShaderTest);
   success = 0;
   glGetShaderiv(device->fragmentShaderTest, GL_COMPILE_STATUS, &success);

   if (!success)
   {
       glGetShaderInfoLog(device->fragmentShaderTest, sizeof(infoLog), NULL, infoLog);
       SRMFatal("[SRMDevice] Fragment shader compilation error: %s.", infoLog);
       return 0;
   }

   device->programTest = glCreateProgram();
   glAttachShader(device->programTest, device->vertexShaderTest);
   glAttachShader(device->programTest, device->fragmentShaderTest);
   glLinkProgram(device->programTest);
   glUseProgram(device->programTest);
   glBindAttribLocation(device->programTest, 0, "position");
   glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, square);
   glEnableVertexAttribArray(0);
   device->textureUniformTest = glGetUniformLocation(device->programTest, "tex");
   return 1;
}

void srmDeviceUninitializeTestShader(SRMDevice *device)
{
    eglMakeCurrent(device->eglDisplay,
                   device->eglSurfaceTest,
                   device->eglSurfaceTest,
                   device->eglSharedContext);

    if (device->programTest)
    {
        glDetachShader(device->programTest, device->fragmentShaderTest);
        glDetachShader(device->programTest, device->vertexShaderTest);
        glDeleteProgram(device->programTest);
        device->programTest = 0;
    }

    if (device->fragmentShaderTest)
    {
        glDeleteShader(device->fragmentShaderTest);
        device->fragmentShaderTest = 0;
    }

    if (device->vertexShaderTest)
    {
        glDeleteShader(device->vertexShaderTest);
        device->vertexShaderTest = 0;
    }
}


UInt8 srmDeviceUpdateGLExtensions(SRMDevice *device)
{
    const char *exts = (const char*)glGetString(GL_EXTENSIONS);
    SRMDebug("[%s] OpenGL Extensions: %s.", device->name, exts);
    device->glExtensions.EXT_read_format_bgra = srmEGLHasExtension(exts, "GL_EXT_read_format_bgra");
    device->glExtensions.EXT_texture_format_BGRA8888 = srmEGLHasExtension(exts, "GL_EXT_texture_format_BGRA8888");
    device->glExtensions.OES_EGL_image_external = srmEGLHasExtension(exts, "GL_OES_EGL_image_external");
    device->glExtensions.OES_EGL_image = srmEGLHasExtension(exts, "GL_OES_EGL_image");
    device->glExtensions.OES_EGL_image_base = srmEGLHasExtension(exts, "GL_OES_EGL_image_base");
    return 1;
}

UInt8 srmDeviceUpdateClientCaps(SRMDevice *device)
{
    device->clientCapStereo3D = drmSetClientCap(device->fd, DRM_CLIENT_CAP_STEREO_3D, 1) == 0;

    char *forceLegacyENV = getenv("SRM_FORCE_LEGACY_API");

    if (!forceLegacyENV || atoi(forceLegacyENV) != 1)
        device->clientCapAtomic = drmSetClientCap(device->fd, DRM_CLIENT_CAP_ATOMIC, 1) == 0;

    if (device->clientCapAtomic)
    {
        // Enabled implicitly by atomic
        device->clientCapAspectRatio = 1;
        device->clientCapUniversalPlanes = 1;

        char *writebackENV = getenv("SRM_ENABLE_WRITEBACK_CONNECTORS");

        if (writebackENV && atoi(writebackENV) == 1)
            device->clientCapWritebackConnectors = drmSetClientCap(device->fd, DRM_CLIENT_CAP_WRITEBACK_CONNECTORS, 1)  == 0;
    }
    else
    {
        device->clientCapAspectRatio = drmSetClientCap(device->fd, DRM_CLIENT_CAP_ASPECT_RATIO, 1) == 0;
        device->clientCapUniversalPlanes = drmSetClientCap(device->fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1) == 0;
    }

    return 1;
}

UInt8 srmDeviceUpdateCaps(SRMDevice *device)
{
    UInt64 value = 0;
    drmGetCap(device->fd, DRM_CAP_DUMB_BUFFER, &value);
    device->capDumbBuffer = value == 1;

    value = 0;
    drmGetCap(device->fd, DRM_CAP_PRIME, &value);
    device->capPrimeImport = value & DRM_PRIME_CAP_IMPORT;
    device->capPrimeExport = value & DRM_PRIME_CAP_EXPORT;

    value = 0;
    drmGetCap(device->fd, DRM_CAP_ADDFB2_MODIFIERS, &value);
    device->capAddFb2Modifiers = value == 1;

    value = 0;
    drmGetCap(device->fd, DRM_CAP_TIMESTAMP_MONOTONIC, &value);
    device->capTimestampMonotonic = value == 1;
    device->clock = device->capTimestampMonotonic ? CLOCK_MONOTONIC : CLOCK_REALTIME;

    value = 0;
    drmGetCap(device->fd, DRM_CAP_ASYNC_PAGE_FLIP, &value);
    device->capAsyncPageFlip = value == 1;

#ifdef DRM_CAP_ATOMIC_ASYNC_PAGE_FLIP
    value = 0;
    drmGetCap(device->fd, DRM_CAP_ATOMIC_ASYNC_PAGE_FLIP, &value);
    device->capAtomicAsyncPageFlip = value == 1;
#endif

    return 1;
}

UInt8 srmDeviceUpdateCrtcs(SRMDevice *device)
{
    drmModeRes *res = drmModeGetResources(device->fd);

    if (!res)
    {
        SRMError("Could not get device %s resources.", device->name);
        return 0;
    }

    SRMCrtc *crtc;

    for (int i = 0; i < res->count_crtcs; i++)
    {
        crtc = srmCrtcCreate(device, res->crtcs[i]);

        if (crtc)
            crtc->deviceLink = srmListAppendData(device->crtcs, crtc);
    }

    drmModeFreeResources(res);

    if (srmListIsEmpty(device->crtcs))
    {
        SRMError("SRM Error: No crtc found for device %s.", device->name);
        return 0;
    }

    return 1;
}

UInt8 srmDeviceUpdateEncoders(SRMDevice *device)
{
    drmModeRes *res = drmModeGetResources(device->fd);

    if (!res)
    {
        SRMError("Could not get device %s resources.", device->name);
        return 0;
    }

    SRMEncoder *encoder;

    for (int i = 0; i < res->count_encoders; i++)
    {
        encoder = srmEncoderCreate(device, res->encoders[i]);

        if (encoder)
            encoder->deviceLink = srmListAppendData(device->encoders, encoder);
    }

    drmModeFreeResources(res);

    if (srmListIsEmpty(device->encoders))
    {
        SRMError("No encoder found for device %s.", device->name);
        return 0;
    }

    return 1;
}

UInt8 srmDeviceUpdatePlanes(SRMDevice *device)
{
    drmModePlaneRes *planeRes = drmModeGetPlaneResources(device->fd);

    if (!planeRes)
    {
        SRMError("Could not get device %s plane resources.", device->name);
        return 0;
    }

    SRMPlane *plane;

    for (UInt32 i = 0; i < planeRes->count_planes; i++)
    {
        plane = srmPlaneCreate(device, planeRes->planes[i]);

        if (plane)
            plane->deviceLink = srmListAppendData(device->planes, plane);
    }

    drmModeFreePlaneResources(planeRes);

    return 1;
}

UInt8 srmDeviceUpdateConnectors(SRMDevice *device)
{
    drmModeRes *res = drmModeGetResources(device->fd);

    if (!res)
    {
        SRMError("Could not get device %s resources.", device->name);
        return 0;
    }

    SRMConnector *connector;

    for (int i = 0; i < res->count_connectors; i++)
    {
        connector = srmConnectorCreate(device, res->connectors[i]);

        if (connector)
            connector->deviceLink = srmListAppendData(device->connectors, connector);
    }

    drmModeFreeResources(res);

    if (srmListIsEmpty(device->connectors))
    {
        SRMError("No connector found for device %s.", device->name);
        return 0;
    }

    return 1;
}

UInt8 srmDeviceInitEGLDeallocatorContext(SRMDevice *device)
{
    srmCoreSendDeallocatorMessage(device->core, SRM_DEALLOCATOR_MSG_CREATE_CONTEXT, device, 0, EGL_NO_IMAGE);

    while (device->core->deallocatorState == 0)
        usleep(10);

    if (device->core->deallocatorState == -1)
    {
        SRMError("[%s] Failed to create deallocator EGL context.", device->name);
        return 0;
    }
    return 1;
}

void srmDeviceUninitEGLDeallocatorContext(SRMDevice *device)
{
    if (device->eglDeallocatorContext != EGL_NO_CONTEXT)
    {
        srmCoreSendDeallocatorMessage(device->core, SRM_DEALLOCATOR_MSG_DESTROY_CONTEXT, device, 0, EGL_NO_IMAGE);

        while (device->core->deallocatorState == 0)
            usleep(10);
    }
}

UInt8 srmDeviceHandleHotpluggingEvent(SRMDevice *device)
{
    if (drmIsMaster(device->fd) != 1)
    {
        device->pendingUdevEvents = 1;
        SRMWarning("[core] Can't handle connector hotplugging event. Device %s is not master.", device->name);
        return 0;
    }

    device->pendingUdevEvents = 0;

    // Check connector states
    SRMListForeach(connectorIt, device->connectors)
    {
        SRMConnector *connector = srmListItemGetData(connectorIt);
        drmModeConnector *connectorRes = drmModeGetConnector(device->fd, connector->id);

        if (!connectorRes)
        {
            SRMError("Failed to get device %s connector %d resources in hotplug event.", connector->device->name, connector->id);
            continue;
        }

        UInt8 connected = connectorRes->connection == DRM_MODE_CONNECTED;

        // Connector changed state
        if (connector->connected != connected)
        {
            // Plugged event
            if (connected)
            {
                srmConnectorUpdateProperties(connector);
                srmConnectorUpdateNames(connector);
                srmConnectorUpdateEncoders(connector);
                srmConnectorUpdateModes(connector);

                SRMDebug("[%s] Connector (%d) %s, %s, %s plugged.",
                         connector->device->name,
                         connector->id,
                         connector->name,
                         connector->model,
                         connector->manufacturer);

                // Notify listeners
                SRMListForeach(listenerIt, device->core->connectorPluggedListeners)
                {
                    SRMListener *listener = srmListItemGetData(listenerIt);
                    void (*callback)(SRMListener *, SRMConnector *) = (void(*)(SRMListener *, SRMConnector *))listener->callback;
                    callback(listener, connector);
                }
            }

            // Unplugged event
            else
            {
                SRMDebug("[%s] Connector (%d) %s, %s, %s unplugged.",
                         connector->device->name,
                         connector->id,
                         connector->name,
                         connector->model,
                         connector->manufacturer);

                // Notify listeners
                SRMListForeach(listenerIt, device->core->connectorUnpluggedListeners)
                {
                    SRMListener *listener = srmListItemGetData(listenerIt);
                    void (*callback)(SRMListener *, SRMConnector *) = (void(*)(SRMListener *, SRMConnector *))listener->callback;
                    callback(listener, connector);
                }

                // Uninitialize after notify so users can for example backup some data
                srmConnectorUninitialize(connector);
                srmConnectorUpdateProperties(connector);
                srmConnectorUpdateNames(connector);
                srmConnectorUpdateEncoders(connector);
                srmConnectorUpdateModes(connector);
            }
        }

        drmModeFreeConnector(connectorRes);
    }

    return 1;
}

static UInt8 srmDeviceTestCPUAllocation(SRMDevice *device, UInt32 width, UInt32 height)
{
    UInt8 *pixels = NULL;
    UInt8 *readPixels = NULL;
    UInt8 *pixel, *readPixel;
    UInt8 color = 0;
    UInt8 ok = 0;
    UInt32 stride = width * 4;
    SRMBuffer *buffer = NULL;
    GLuint textureId = 0;

    pixels = malloc(height * stride);
    readPixels = malloc(height * stride);

    for (UInt32 y = 0; y < height; y++)
    {
        for (UInt32 x = 0; x < width; x++)
        {
            color = 1 - color;
            pixel = &pixels[y * stride + x * 4];
            pixel[0] = pixel[1] = pixel[2] = rand() % 256;
        }
    }

    eglMakeCurrent(device->eglDisplay,
                   device->eglSurfaceTest,
                   device->eglSurfaceTest,
                   device->eglSharedContext);

    buffer = srmBufferCreateFromCPU(device->core, device, width, height, stride, pixels, DRM_FORMAT_ARGB8888);

    if (!buffer)
        goto fail;

    textureId = srmBufferGetTextureID(device, buffer);

    if (textureId == 0)
        goto fail;

    glUseProgram(device->programTest);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_BLEND);
    glEnable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, width, height);
    glScissor(0, 0, width, height);
    glUniform1i(device->textureUniformTest, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glFinish();
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, readPixels);

    for (UInt32 y = 0; y < height; y++)
    {
        for (UInt32 x = 0; x < width; x++)
        {
            pixel = &pixels[y * stride + x * 4];
            readPixel = &readPixels[(height - 1 - y) * stride + x * 4];

            if (pixel[0] != readPixel[2] || pixel[1] != readPixel[1] || pixel[2] != readPixel[0])
                goto fail;
        }
    }
    ok = 1;
fail:
    if (buffer)
        srmBufferDestroy(buffer);

    if (pixels)
        free(pixels);

    if (readPixels)
        free(readPixels);

    if (ok)
        SRMDebug("[%s] CPU buffer allocation test succeded %dx%d.", device->name, width, height);
    else
        SRMError("[%s] CPU buffer allocation test failed %dx%d.", device->name, width, height);

    return ok;
}

void srmDeviceTestCPUAllocationMode(SRMDevice *device)
{
    char *forceGlesAllocation = getenv("SRM_FORCE_GL_ALLOCATION");

    if (forceGlesAllocation && atoi(forceGlesAllocation) == 1)
    {
        device->cpuBufferWriteMode = SRM_BUFFER_WRITE_MODE_GLES;
        return;
    }

    device->cpuBufferWriteMode = SRM_BUFFER_WRITE_MODE_PRIME;

    SRMDebug("[%s] Testing PRIME map CPU buffer allocation mode.", device->name);

    if (srmDeviceTestCPUAllocation(device, 13, 17))
        return;

    SRMDebug("[%s] Testing GBM bo map CPU buffer allocation mode.", device->name);

    device->cpuBufferWriteMode = SRM_BUFFER_WRITE_MODE_GBM;

    if (srmDeviceTestCPUAllocation(device, 13, 17))
        return;

    SRMDebug("[%s] Using OpenGL CPU buffer allocation mode.", device->name);

    device->cpuBufferWriteMode = SRM_BUFFER_WRITE_MODE_GLES;
}
