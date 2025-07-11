#include <private/modes/SRMRenderModeCommon.h>
#include <private/SRMListenerPrivate.h>
#include <private/SRMCorePrivate.h>
#include <private/SRMDevicePrivate.h>
#include <private/SRMCrtcPrivate.h>
#include <private/SRMEncoderPrivate.h>
#include <private/SRMPlanePrivate.h>
#include <private/SRMConnectorPrivate.h>
#include <private/SRMBufferPrivate.h>
#include <private/SRMEGLPrivate.h>

#include <SRMFormat.h>
#include <SRMList.h>
#include <SRMLog.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

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
        SRMWarning("Device %s is blacklisted. Ignoring it.", name);
        return NULL;
    }

    // REF 1
    SRMDevice *device = calloc(1, sizeof(SRMDevice));

    strncpy(device->name, name, sizeof(device->name) - 1);
    size_t n = strlen(name) - 1;
    while (name[n] != '/') n--;
    strncpy(device->shortName, &name[n + 1], sizeof(device->shortName) - 1);

    device->core = core;
    device->enabled = 1;
    device->isBootVGA = isBootVGA;
    device->eglDevice = EGL_NO_DEVICE_EXT;
    device->fd = -1;

    SRMDebug("[%s] Is Boot VGA: %s.", device->shortName, device->isBootVGA ?  "YES" : "NO");

    // REF 2
    device->fd = core->interface->openRestricted(name,
                                                 O_RDWR | O_CLOEXEC,
                                                 core->interfaceUserData);

    if (device->fd < 0)
    {
        SRMError("[%s] Failed to open DRM device.", device->shortName);
        goto fail;
    }

    SRMDebug("[%s] Is DRM Master: %s.", device->shortName, drmIsMaster(device->fd) ?  "YES" : "NO");

    drmVersion *version = drmGetVersion(device->fd);

    if (version)
    {
        SRMDebug("[%s] DRM Driver: %s.", device->shortName, version->name);

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
        SRMError("[%s] Failed to create page flip mutex.", device->shortName);
        goto fail;
    }
    device->pageFlipMutexInitialized = 1;

    // REF -
    if (!srmDeviceUpdateClientCaps(device))
        goto fail;

    // REF -
    if (!srmDeviceUpdateCaps(device))
        goto fail;

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
    if (!srmDeviceInitializeTestGBM(device))
        goto fail;

    // REF 7-3
    if (!srmDeviceInitializeTestShader(device))
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

    // UNREF 7-3
    srmDeviceUninitializeTestShader(device);

    // UNREF 7-1
    srmDeviceUninitializeTestGBM(device);

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
        SRMError("[%s] Failed to initialize GBM.", device->shortName);
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
    device->eglDisplay = device->core->eglFunctions.eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_KHR, device->gbm, NULL);

    if (device->eglDisplay == EGL_NO_DISPLAY)
    {
        SRMError("[%s] Failed to get EGL display.", device->shortName);
        return 0;
    }

    EGLint minor, major;

    if (!eglInitialize(device->eglDisplay, &minor, &major))
    {
        SRMError("[%s] Failed to initialize EGL display.", device->shortName);
        device->eglDisplay = EGL_NO_DISPLAY;
        return 0;
    }

    SRMDebug("[%s] EGL Version: %d.%d.", device->shortName, minor, major);
    const char *vendor = eglQueryString(device->eglDisplay, EGL_VENDOR);
    SRMDebug("[%s] EGL Vendor: %s.", device->shortName, vendor ? vendor : "Unknown");
    return 1;
}

void srmDeviceUninitializeEGL(SRMDevice *device)
{
    if (device->eglDisplay != EGL_NO_DISPLAY)
    {
        eglMakeCurrent(device->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglTerminate(device->eglDisplay);
        eglReleaseThread();
    }
}

UInt8 srmDeviceUpdateEGLExtensions(SRMDevice *device)
{
    const char *extensions = eglQueryString(device->eglDisplay, EGL_EXTENSIONS);

    if (!extensions)
    {
        SRMError("[%s] Failed to query EGL display extensions.", device->shortName);
        return 0;
    }

    if (SRMLogEGLGetLevel() > 3)
        SRMDebug("[%s] EGL Extensions: %s.", device->shortName, extensions);

    device->eglExtensions.KHR_image_base = srmEGLHasExtension(extensions, "EGL_KHR_image_base");
    device->eglExtensions.KHR_image = srmEGLHasExtension(extensions, "EGL_KHR_image");
    device->eglExtensions.EXT_image_dma_buf_import = srmEGLHasExtension(extensions, "EGL_EXT_image_dma_buf_import");
    device->eglExtensions.EXT_image_dma_buf_import_modifiers = srmEGLHasExtension(extensions, "EGL_EXT_image_dma_buf_import_modifiers");
    device->eglExtensions.EXT_create_context_robustness = srmEGLHasExtension(extensions, "EGL_EXT_create_context_robustness");
    device->eglExtensions.KHR_image_pixmap = srmEGLHasExtension(extensions, "EGL_KHR_image_pixmap");
    device->eglExtensions.KHR_gl_texture_2D_image = srmEGLHasExtension(extensions, "EGL_KHR_gl_texture_2D_image");
    device->eglExtensions.KHR_gl_renderbuffer_image = srmEGLHasExtension(extensions, "EGL_KHR_gl_renderbuffer_image");
    device->eglExtensions.KHR_wait_sync = srmEGLHasExtension(extensions, "EGL_KHR_wait_sync");
    device->eglExtensions.KHR_fence_sync = srmEGLHasExtension(extensions, "EGL_KHR_fence_sync");
    device->eglExtensions.ANDROID_native_fence_sync = srmEGLHasExtension(extensions, "EGL_ANDROID_native_fence_sync");

    const char *deviceExtensions = NULL, *driverName = NULL;

    if (device->core->eglExtensions.EXT_device_query)
    {
        EGLAttrib deviceAttrib;

        if (!device->core->eglFunctions.eglQueryDisplayAttribEXT(device->eglDisplay, EGL_DEVICE_EXT, &deviceAttrib))
        {
            SRMError("[%s] eglQueryDisplayAttribEXT(EGL_DEVICE_EXT) failed.", device->shortName);
            goto skipDeviceQuery;
        }

        device->eglDevice = (EGLDeviceEXT)deviceAttrib;

        deviceExtensions = device->core->eglFunctions.eglQueryDeviceStringEXT(device->eglDevice, EGL_EXTENSIONS);

        if (!deviceExtensions)
        {
            SRMError("[%s] eglQueryDeviceStringEXT(EGL_EXTENSIONS) failed.", device->shortName);
            goto skipDeviceQuery;
        }

        if (SRMLogEGLGetLevel() > 3)
            SRMDebug("[%s] EGL Device Extensions: %s.", device->shortName, deviceExtensions);

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

    SRMDebug("[%s] EGL Driver: %s.", device->shortName, driverName ? driverName : "Unknown");

    if (!device->eglExtensions.KHR_no_config_context && !device->eglExtensions.MESA_configless_context)
    {
        SRMError("[%s] Required EGL extensions EGL_KHR_no_config_context and EGL_MESA_configless_context are not available.", device->shortName);
        return 0;
    }

    if (!device->eglExtensions.KHR_surfaceless_context)
    {
        SRMError("[%s] Required EGL extension KHR_surfaceless_context is not available.", device->shortName);
        return 0;
    }

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
             device->shortName,
             device->eglFunctions.glEGLImageTargetTexture2DOES == NULL ? "NO" : "YES");

    SRMDebug("[%s] Has glEGLImageTargetRenderbufferStorageOES: %s.",
             device->shortName,
             device->eglFunctions.glEGLImageTargetRenderbufferStorageOES == NULL ? "NO" : "YES");

    const UInt8 hasEGLSync = device->glExtensions.OES_EGL_sync &&
                             device->eglExtensions.KHR_fence_sync &&
                             device->eglExtensions.KHR_wait_sync &&
                             device->eglExtensions.ANDROID_native_fence_sync;

    if (hasEGLSync)
    {
        device->eglFunctions.eglCreateSyncKHR = (PFNEGLCREATESYNCKHRPROC)eglGetProcAddress("eglCreateSyncKHR");
        device->eglFunctions.eglDestroySyncKHR = (PFNEGLDESTROYSYNCKHRPROC)eglGetProcAddress("eglDestroySyncKHR");
        device->eglFunctions.eglWaitSyncKHR = (PFNEGLWAITSYNCKHRPROC)eglGetProcAddress("eglWaitSyncKHR");
        device->eglFunctions.eglDupNativeFenceFDANDROID = (PFNEGLDUPNATIVEFENCEFDANDROIDPROC)eglGetProcAddress("eglDupNativeFenceFDANDROID");
    }

    SRMDebug("[%s] Has EGL Android Fence Sync: %s.", device->shortName, hasEGLSync ? "YES" : "NO");

    if (device->eglExtensions.EXT_image_dma_buf_import_modifiers)
    {
        device->eglFunctions.eglQueryDmaBufFormatsEXT = (PFNEGLQUERYDMABUFFORMATSEXTPROC) eglGetProcAddress("eglQueryDmaBufFormatsEXT");
        device->eglFunctions.eglQueryDmaBufModifiersEXT = (PFNEGLQUERYDMABUFMODIFIERSEXTPROC) eglGetProcAddress("eglQueryDmaBufModifiersEXT");
    }

    if (!device->eglFunctions.glEGLImageTargetRenderbufferStorageOES)
    {
        SRMError("[%s] Required EGL extension KHR_gl_renderbuffer_image is not available.", device->shortName);
        return 0;
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
        SRMDebug("[%s] No EGL DMA formats (EXT_image_dma_buf_import not avaliable).", device->shortName);
        return 0;
    }

    EGLint formatsCount = 0;
    EGLint *formats = NULL;

    if (device->eglExtensions.EXT_image_dma_buf_import_modifiers)
    {
        if (!device->eglFunctions.eglQueryDmaBufFormatsEXT(device->eglDisplay, 0, NULL, &formatsCount))
        {
            SRMError("[%s] Failed to query the number of EGL DMA formats.", device->shortName);
            goto implicitFallback;
        }

        if (formatsCount <= 0)
        {
            SRMError("[%s] No EGL DMA formats.", device->shortName);
            goto implicitFallback;
        }

        formats = calloc(formatsCount, sizeof(formats[0]));

        if (!device->eglFunctions.eglQueryDmaBufFormatsEXT(device->eglDisplay, formatsCount, formats, &formatsCount))
        {
            SRMError("[%s] Failed to query EGL DMA formats.", device->shortName);
            free(formats);
            goto implicitFallback;
        }
    }
    else
    {
        implicitFallback:
        SRMError("[%s] Failed to query EGL DMA formats. Adding DRM_FORMAT_ARGB8888 and DRM_FORMAT_XRGB8888 as fallback.", device->shortName);
        formatsCount = 2;
        formats = calloc(formatsCount, sizeof(EGLint));
        formats[0] = DRM_FORMAT_ARGB8888;
        formats[1] = DRM_FORMAT_XRGB8888;
    }

    for (Int32 i = 0; i < formatsCount; i++)
    {
        EGLint modifiersCount = 0;
        UInt64 *modifiers = NULL;
        EGLBoolean *externalOnly = NULL;
        UInt8 allExternalOnly = 1;

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

        // EGL always supports implicit modifiers
        srmFormatsListAddFormat(device->dmaTextureFormats, formats[i], DRM_FORMAT_MOD_INVALID);
        srmFormatsListAddFormat(device->dmaExternalFormats, formats[i], DRM_FORMAT_MOD_INVALID);

        if (modifiersCount == 0)
        {
            srmFormatsListAddFormat(device->dmaTextureFormats, formats[i], DRM_FORMAT_MOD_LINEAR);
            srmFormatsListAddFormat(device->dmaExternalFormats, formats[i], DRM_FORMAT_MOD_LINEAR);
            srmFormatsListAddFormat(device->dmaRenderFormats, formats[i], DRM_FORMAT_MOD_LINEAR);

            if (!allExternalOnly)
                srmFormatsListAddFormat(device->dmaRenderFormats, formats[i], DRM_FORMAT_MOD_INVALID);
        }

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
        SRMError("[%s] Failed to bind GLES API.", device->shortName);
        return 0;
    }

    if (!srmRenderModeCommonChooseEGLConfiguration(
            device->eglDisplay,
            commonEGLConfigAttribs,
            DRM_FORMAT_XRGB8888,
            &device->eglConfigTest))
    {
        SRMError("[%s] Failed to choose EGL configuration.", device->shortName);
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
        SRMError("[%s] Failed to create shared EGL context.", device->shortName);
        return 0;
    }

    eglMakeCurrent(device->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, device->eglSharedContext);

    device->contexts = srmListCreate();
    return 1;
}

UInt8 srmDeviceCreateSharedContextForThread(SRMDevice *device)
{
    assert(device->contexts != NULL);
    pthread_t t = pthread_self();

    if (t == device->core->mainThread) // Already created
        return 1;

    SRMListForeach(ctxIt, device->contexts)
    {
        SRMDeviceThreadContext *ctx = srmListItemGetData(ctxIt);

        if (ctx->thread == t) // Already created
            return 1;
    }

    if (!eglBindAPI(EGL_OPENGL_ES_API))
    {
        SRMError("[%s] srmDeviceCreateSharedContextForThread: Failed to bind GLES API.", device->shortName);
        return 0;
    }

    SRMDeviceThreadContext *ctx = calloc(1, sizeof(SRMDeviceThreadContext));

    ctx->thread = t;
    ctx->context = eglCreateContext(device->eglDisplay, device->eglConfigTest, device->eglSharedContext, device->eglSharedContextAttribs);

    if (ctx->context == EGL_NO_CONTEXT)
    {
        SRMError("[%s] srmDeviceCreateSharedContextForThread: Failed to create thread EGL context.", device->shortName);
        free(ctx);
        return 0;
    }

    srmListAppendData(device->contexts, ctx);
    return 1;
}

void srmDeviceDestroyThreadSharedContext(SRMDevice *device, pthread_t thread)
{
    if (thread == device->core->mainThread)
        return;

    assert(device->contexts != NULL);

    SRMListForeach(ctxIt, device->contexts)
    {
        SRMDeviceThreadContext *ctx = srmListItemGetData(ctxIt);

        if (ctx->thread == thread)
        {
            eglMakeCurrent(device->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroyContext(device->eglDisplay, ctx->context);
            srmListRemoveItem(device->contexts, ctxIt);
            free(ctx);
            return;
        }
    }
}

void srmDeviceUninitializeEGLSharedContext(SRMDevice *device)
{
    if (device->eglSharedContext != EGL_NO_CONTEXT)
    {
        eglReleaseThread();
        eglMakeCurrent(device->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(device->eglDisplay, device->eglSharedContext);
    }

    if (device->contexts)
    {
        while (!srmListIsEmpty(device->contexts))
        {
            SRMDeviceThreadContext *ctx = srmListItemGetData(srmListGetFront(device->contexts));
            srmDeviceDestroyThreadSharedContext(device, ctx->thread);
        }
        srmListDestroy(device->contexts);
        device->contexts = NULL;
    }
}

UInt8 srmDeviceInitializeTestGBM(SRMDevice *device)
{
    SRMListItem *it;
    EGLImage image;
    GLenum status;

    device->gbmTestBo = srmBufferCreateGBMBo(
        device->gbm,
        64, 64,
        DRM_FORMAT_XRGB8888,
        DRM_FORMAT_MOD_LINEAR,
        GBM_BO_USE_RENDERING);

    if (!device->gbmTestBo)
    {
        SRMError("[%s] srmDeviceInitializeTestGBMSurface: Failed to create gbm_bo.", device->shortName);
        goto fallback;
    }

    it = srmListAppendData(device->core->devices, device);
    device->testBuffer = srmBufferCreateFromGBM(device->core, device->gbmTestBo);
    srmListRemoveItem(device->core->devices, it);

    if (!device->testBuffer)
    {
        SRMError("[%s] srmDeviceInitializeTestGBMSurface: Failed to create SRMBuffer.", device->shortName);
        goto fallback;
    }

    image = srmBufferGetEGLImage(device, device->testBuffer);

    if (image == EGL_NO_IMAGE)
    {
        SRMError("[%s] srmDeviceInitializeTestGBMSurface: Failed to get EGLImage from SRMBuffer.", device->shortName);
        goto fallback;
    }

    eglMakeCurrent(device->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, device->eglSharedContext);

    glGenRenderbuffers(1, &device->testRB);

    if (device->testRB == 0)
    {
        SRMError("[%s] srmDeviceInitializeTestGBMSurface: Failed to generate GL renderbuffer.", device->shortName);
        goto fallback;
    }

    glBindRenderbuffer(GL_RENDERBUFFER, device->testRB);
    device->eglFunctions.glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, image);
    glGenFramebuffers(1, &device->testFB);

    if (device->testFB == 0)
    {
        SRMError("[%s] srmDeviceInitializeTestGBMSurface: Failed to generate GL framebuffer.", device->shortName);
        goto fallback;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, device->testFB);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, device->testRB);
    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        SRMError("[%s] srmDeviceInitializeTestGBMSurface: Incomplete GL framebuffer.", device->shortName);
        goto fallback;
    }

    return 1;

fallback:

    SRMWarning("[%s] srmDeviceInitializeTestGBMSurface: Fallback to GL texture.", device->shortName);
    srmDeviceUninitializeTestGBM(device);
    glGenFramebuffers(1, &device->testFB);

    if (!device->testFB)
        goto fail;

    glBindFramebuffer(GL_FRAMEBUFFER, device->testFB);

    if (device->testFB == 0)
    {
        SRMError("[%s] srmDeviceInitializeTestGBMSurface: Failed to generate GL framebuffer.", device->shortName);
        goto fail;
    }

    glGenTextures(1, &device->testTex);

    if (!device->testTex)
        goto fail;

    glBindTexture(GL_TEXTURE_2D, device->testTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, device->testTex, 0);
    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        SRMError("[%s] srmDeviceInitializeTestGBMSurface: Incomplete GL framebuffer.", device->shortName);
        return 0;
    }

    return 1;

fail:
    return 0;
}

void srmDeviceUninitializeTestGBM(SRMDevice *device)
{
    eglMakeCurrent(device->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, device->eglSharedContext);

    if (device->testFB)
    {
        glDeleteFramebuffers(1, &device->testFB);
        device->testFB = 0;
    }

    if (device->testRB)
    {
        glDeleteRenderbuffers(1, &device->testRB);
        device->testRB = 0;
    }

    if (device->testTex)
    {
        glDeleteTextures(1, &device->testTex);
        device->testTex = 0;
    }

    if (device->testBuffer)
    {
        srmBufferDestroy(device->testBuffer);
        device->testBuffer = NULL;
    }

    if (device->gbmTestBo)
    {
        gbm_bo_destroy(device->gbmTestBo);
        device->gbmTestBo = NULL;
    }
}

UInt8 srmDeviceInitializeTestShader(SRMDevice *device)
{
    static GLfloat square[] =
    {  //  VERTEX       FRAGMENT
        -1.0f,  1.0f,   0.f, 1.f, // TL
        -1.0f, -1.0f,   0.f, 0.f, // BL
         1.0f, -1.0f,   1.f, 0.f, // BR
         1.0f,  1.0f,   1.f, 1.f  // TR
    };

   const char *vertexShaderSource = "attribute vec4 position; varying vec2 v_texcoord; void main() { gl_Position = vec4(position.xy, 0.0, 1.0); v_texcoord = position.zw; }";
   const char *fragmentShaderSource = "precision mediump float; uniform sampler2D tex; varying vec2 v_texcoord; void main() { gl_FragColor = texture2D(tex, v_texcoord); }";
   const char *fragmentShaderSourceExternal = "#extension GL_OES_EGL_image_external : require\nprecision mediump float; uniform samplerExternalOES tex; varying vec2 v_texcoord; void main() { gl_FragColor = texture2D(tex, v_texcoord); }";

   eglMakeCurrent(device->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, device->eglSharedContext);

   GLint success = 0;
   GLchar infoLog[256];
   UInt8 hasExternal = 1;

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

   // EXTERNAL

   device->fragmentShaderTestExternal = glCreateShader(GL_FRAGMENT_SHADER);
   glShaderSource(device->fragmentShaderTestExternal, 1, &fragmentShaderSourceExternal, NULL);
   glCompileShader(device->fragmentShaderTestExternal);
   success = 0;
   glGetShaderiv(device->fragmentShaderTestExternal, GL_COMPILE_STATUS, &success);

   if (!success)
   {
       glGetShaderInfoLog(device->fragmentShaderTestExternal, sizeof(infoLog), NULL, infoLog);
       SRMWarning("[SRMDevice] External fragment shader compilation error: %s.", infoLog);
       hasExternal = 0;
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

   // External

   if (hasExternal)
   {
       device->programTestExternal = glCreateProgram();
       glAttachShader(device->programTestExternal, device->vertexShaderTest);
       glAttachShader(device->programTestExternal, device->fragmentShaderTestExternal);
       glLinkProgram(device->programTestExternal);
       glUseProgram(device->programTestExternal);
       glBindAttribLocation(device->programTestExternal, 0, "position");
       device->textureUniformTestExternal = glGetUniformLocation(device->programTestExternal, "tex");
   }

   return 1;
}

void srmDeviceUninitializeTestShader(SRMDevice *device)
{
    eglMakeCurrent(device->eglDisplay,  EGL_NO_SURFACE, EGL_NO_SURFACE, device->eglSharedContext);

    if (device->programTest)
    {
        glDetachShader(device->programTest, device->fragmentShaderTest);
        glDetachShader(device->programTest, device->vertexShaderTest);
        glDeleteProgram(device->programTest);
        device->programTest = 0;
    }

    if (device->programTestExternal)
    {
        glDetachShader(device->programTestExternal, device->fragmentShaderTestExternal);
        glDetachShader(device->programTestExternal, device->vertexShaderTest);
        glDeleteProgram(device->programTestExternal);
        device->programTestExternal = 0;
    }

    if (device->fragmentShaderTest)
    {
        glDeleteShader(device->fragmentShaderTest);
        device->fragmentShaderTest = 0;
    }

    if (device->fragmentShaderTestExternal)
    {
        glDeleteShader(device->fragmentShaderTestExternal);
        device->fragmentShaderTestExternal = 0;
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

    if (SRMLogEGLGetLevel() > 3)
        SRMDebug("[%s] OpenGL Extensions: %s.", device->shortName, exts);
    device->glExtensions.EXT_read_format_bgra = srmEGLHasExtension(exts, "GL_EXT_read_format_bgra");
    device->glExtensions.EXT_texture_format_BGRA8888 = srmEGLHasExtension(exts, "GL_EXT_texture_format_BGRA8888");
    device->glExtensions.OES_EGL_image_external = srmEGLHasExtension(exts, "GL_OES_EGL_image_external");
    device->glExtensions.OES_EGL_image = srmEGLHasExtension(exts, "GL_OES_EGL_image");
    device->glExtensions.OES_EGL_image_base = srmEGLHasExtension(exts, "GL_OES_EGL_image_base");
    device->glExtensions.OES_surfaceless_context = srmEGLHasExtension(exts, "GL_OES_surfaceless_context");
    device->glExtensions.OES_EGL_sync = srmEGLHasExtension(exts, "GL_OES_EGL_sync");
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
        SRMError("[%s] Could not get DRM resources.", device->shortName);
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
        SRMError("[%s] No CRCT found.", device->shortName);
        return 0;
    }

    return 1;
}

UInt8 srmDeviceUpdateEncoders(SRMDevice *device)
{
    drmModeRes *res = drmModeGetResources(device->fd);

    if (!res)
    {
        SRMError("[%s] Could not get DRM resources.", device->shortName);
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
        SRMError("[%s] No encoder found.", device->shortName);
        return 0;
    }

    return 1;
}

UInt8 srmDeviceUpdatePlanes(SRMDevice *device)
{
    drmModePlaneRes *planeRes = drmModeGetPlaneResources(device->fd);

    if (!planeRes)
    {
        SRMError("[%s] Could not get plane resources.", device->shortName);
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
        SRMError("[%s] Could not get DRM resources.", device->shortName);
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
        SRMError("[%s] No connector found.", device->shortName);
        return 0;
    }

    return 1;
}

UInt8 srmDeviceHandleHotpluggingEvent(SRMDevice *device)
{
    if (drmIsMaster(device->fd) != 1)
    {
        device->pendingUdevEvents = 1;
        SRMWarning("[%s] Can not handle connector hotplugging event. Device is not master.", device->shortName);
        return 0;
    }

    // TODO: Check if the kernel registered/unregistered connectors

    device->pendingUdevEvents = 0;

    // Check connector states
    SRMListForeach(connectorIt, device->connectors)
    {
        SRMConnector *connector = srmListItemGetData(connectorIt);
        drmModeConnector *res = drmModeGetConnector(device->fd, connector->id);

        if (!res)
        {
            SRMError("Failed to get device %s connector %d resources in hotplug event.", connector->device->shortName, connector->id);
            continue;
        }

        UInt8 connected = res->connection == DRM_MODE_CONNECTED;

        // Connector changed state
        if (connector->connected != connected)
        {
            // Plugged event
            if (connected)
            {
                srmConnectorUpdateProperties(connector, res);
                srmConnectorUpdateNames(connector, res);
                srmConnectorUpdateEncoders(connector, res);
                srmConnectorUpdateModes(connector, res);

                SRMDebug("[%s] Connector (%d) %s, %s, %s plugged.",
                         connector->device->shortName,
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
                         connector->device->shortName,
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
                srmConnectorUpdateProperties(connector, res);
                srmConnectorUpdateNames(connector, res);
                srmConnectorUpdateEncoders(connector, res);
                srmConnectorUpdateModes(connector, res);
            }
        }

        drmModeFreeConnector(res);
    }

    return 1;
}

static UInt8 srmDeviceTestCPUAllocation(const char *modeName, SRMDevice *device, UInt32 width, UInt32 height)
{
    UInt8 *pixels = NULL;
    UInt8 *readPixels = NULL;
    UInt8 color = 255;
    UInt8 ok = 0;
    UInt32 stride = width * 4;
    SRMBuffer *buffer = NULL;
    SRMTexture texture = { 0, GL_NONE };

    pixels = malloc(height * stride);
    readPixels = malloc(height * stride);
    memset(pixels, color, height * stride);

    buffer = srmBufferCreateFromCPU(device->core, device, width, height, stride, pixels, DRM_FORMAT_ARGB8888);

    if (!buffer)
        goto fail;

    texture = srmBufferGetTexture(device, buffer);

    if (texture.id == 0)
        goto fail;

    eglMakeCurrent(device->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, device->eglSharedContext);
    glUseProgram(texture.target == GL_TEXTURE_2D ? device->programTest : device->programTestExternal);
    glBindFramebuffer(GL_FRAMEBUFFER, device->testFB);
    glDisable(GL_BLEND);
    glEnable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, width, height);
    glScissor(0, 0, width, height);
    glUniform1i(texture.target == GL_TEXTURE_2D ? device->textureUniformTest : device->textureUniformTestExternal, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(texture.target, texture.id);
    glTexParameteri(texture.target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(texture.target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glFinish();
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, readPixels);

    UInt32 matches = 0;

    for (UInt32 i = 0; i < height * stride; i+=4)
        if (pixels[i] == readPixels[i] && pixels[i + 1] == readPixels[i + 1] && pixels[i + 2] == readPixels[i + 2])
            matches++;

    ok = matches >= (width * height) / 2;

    if (!ok)
        SRMError("[%s] %s CPU buffer allocation test failed %dx%d. Sample: SRC(%d, %d, %d) - READ(%d, %d, %d).",
                 device->shortName, modeName, width, height,
                 pixels[0], pixels[1], pixels[2],
                 readPixels[0], readPixels[1], readPixels[2]);

fail:
    if (buffer)
        srmBufferDestroy(buffer);

    if (pixels)
        free(pixels);

    if (readPixels)
        free(readPixels);

    if (ok)
        SRMDebug("[%s] %s CPU buffer allocation test succeded %dx%d.", device->shortName, modeName, width, height);

    return ok;
}

void srmDeviceTestCPUAllocationMode(SRMDevice *device)
{
    char *forceGlesAllocation = getenv("SRM_FORCE_GL_ALLOCATION");
    int width = 64;
    int height = 64;

    if (forceGlesAllocation && atoi(forceGlesAllocation) == 1)
    {
        device->cpuBufferWriteMode = SRM_BUFFER_WRITE_MODE_GLES;
        return;
    }

    device->cpuBufferWriteMode = SRM_BUFFER_WRITE_MODE_PRIME;

    SRMDebug("[%s] Testing PRIME map CPU buffer allocation mode.", device->shortName);

    if (srmDeviceTestCPUAllocation("PRIME mmap", device, width, height))
        return;

    SRMDebug("[%s] Testing GBM bo map CPU buffer allocation mode.", device->shortName);

    device->cpuBufferWriteMode = SRM_BUFFER_WRITE_MODE_GBM;

    if (srmDeviceTestCPUAllocation("GBM mmap", device, width, height))
        return;

    SRMDebug("[%s] Using OpenGL CPU buffer allocation mode.", device->shortName);

    device->cpuBufferWriteMode = SRM_BUFFER_WRITE_MODE_GLES;

    if (!srmDeviceTestCPUAllocation("GL", device, width, height))
        SRMWarning("[%s] All CPU buffer allocation tests failed.", device->shortName);
}
