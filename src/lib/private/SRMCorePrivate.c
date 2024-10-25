#include <private/SRMCorePrivate.h>
#include <private/SRMDevicePrivate.h>
#include <private/SRMConnectorPrivate.h>
#include <SRMFormat.h>
#include <SRMLog.h>
#include <SRMList.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/epoll.h>
#include <fcntl.h>

UInt8 srmCoreUpdateEGLExtensions(SRMCore *core)
{
    if (eglBindAPI(EGL_OPENGL_ES_API) == EGL_FALSE)
    {
        SRMFatal("[core] Failed to bind to the OpenGL ES API.");
        return 0;
    }

    const char *extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);

    if (!extensions)
    {
        SRMFatal("[core] Failed to query core EGL extensions.");
        return 0;
    }

    core->eglExtensions.EXT_platform_base = srmEGLHasExtension(extensions, "EGL_EXT_platform_base");

    if (!core->eglExtensions.EXT_platform_base)
    {
        SRMFatal("[core] EGL_EXT_platform_base not supported.");
        return 0;
    }

    core->eglExtensions.KHR_platform_gbm = srmEGLHasExtension(extensions, "EGL_KHR_platform_gbm");
    core->eglExtensions.MESA_platform_gbm = srmEGLHasExtension(extensions, "EGL_MESA_platform_gbm");

    if (!core->eglExtensions.KHR_platform_gbm && !core->eglExtensions.MESA_platform_gbm)
    {
        SRMFatal("[core] EGL_KHR_platform_gbm not supported.");
        return 0;
    }

    core->eglExtensions.EXT_platform_device = srmEGLHasExtension(extensions, "EGL_EXT_platform_device");
    core->eglExtensions.KHR_display_reference = srmEGLHasExtension(extensions, "EGL_KHR_display_reference");
    core->eglExtensions.EXT_device_base = srmEGLHasExtension(extensions, "EGL_EXT_device_base");
    core->eglExtensions.EXT_device_enumeration = srmEGLHasExtension(extensions, "EGL_EXT_device_enumeration");
    core->eglExtensions.EXT_device_query = srmEGLHasExtension(extensions, "EGL_EXT_device_query");
    core->eglExtensions.KHR_debug = srmEGLHasExtension(extensions, "EGL_KHR_debug");
    return 1;
}

UInt8 srmCoreCreateUdev(SRMCore *core)
{
    core->udev = udev_new();

    if(!core->udev)
    {
        SRMFatal("[core] Failed to create udev context.");
        return 0;
    }

    return 1;
}

UInt8 srmCoreEnumerateDevices(SRMCore *core)
{
    struct udev_enumerate *enumerate;
    struct udev_list_entry *devices, *dev_list_entry;
    struct udev_device *dev;

    enumerate = udev_enumerate_new(core->udev);

    if (!enumerate)
    {
        SRMFatal("Failed to create udev enumerate.");
        return 0;
    }

    udev_enumerate_add_match_is_initialized(enumerate);
    udev_enumerate_add_match_sysname(enumerate, "card[0-9]*");
    udev_enumerate_add_match_property(enumerate, "DEVTYPE", "drm_minor");
    udev_enumerate_scan_devices(enumerate);
    devices = udev_enumerate_get_list_entry(enumerate);

    udev_list_entry_foreach(dev_list_entry, devices)
    {
        const char *path;

        path = udev_list_entry_get_name(dev_list_entry);
        dev = udev_device_new_from_syspath(core->udev, path);

        UInt8 isBootVGA = 0;

        struct udev_device *pci = udev_device_get_parent_with_subsystem_devtype(dev, "pci", NULL);

        if (pci)
        {
            const char *boot_vga = udev_device_get_sysattr_value(pci, "boot_vga");
            if (boot_vga && strcmp(boot_vga, "1") == 0)
                isBootVGA = 1;
        }

        SRMDevice *device = srmDeviceCreate(core, udev_device_get_devnode(dev), isBootVGA);

        if (device)
            device->coreLink = srmListAppendData(core->devices, device);

        udev_device_unref(dev);
    }

    udev_enumerate_unref(enumerate);

    return !srmListIsEmpty(core->devices);
}

UInt8 srmCoreInitMonitor(SRMCore *core)
{
    core->monitorFd.fd = -1;
    core->udevMonitorFd = -1;

    core->monitor = udev_monitor_new_from_netlink(core->udev, "udev");

    if (!core->monitor)
    {
        SRMFatal("[core] Failed to create udev monitor.");
        return 0;
    }

    if (udev_monitor_filter_add_match_subsystem_devtype(core->monitor, "drm", "drm_minor") < 0)
    {
        SRMFatal("[core] Failed to add udev monitor filter.");
        goto fail;
    }

    if (udev_monitor_enable_receiving(core->monitor) < 0)
    {
        SRMFatal("[core] Failed to enable udev monitor receiving.");
        goto fail;
    }

    core->udevMonitorFd = udev_monitor_get_fd(core->monitor);

    if (core->udevMonitorFd < 0)
    {
        SRMFatal("[core] Failed to get udev monitor fd.");
        goto fail;
    }

    core->monitorFd.fd = epoll_create1(EPOLL_CLOEXEC);

    if (core->monitorFd.fd < 0)
    {
        SRMFatal("[core] Failed to create udev epoll fd.");
        goto fail;
    }

    core->monitorFd.events = POLLIN;
    core->monitorFd.revents = 0;

    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = core->udevMonitorFd;

    if (epoll_ctl(core->monitorFd.fd, EPOLL_CTL_ADD, core->udevMonitorFd, &event) != 0)
    {
        SRMFatal("[core] Failed to add udev monitor fd to epoll fd.");
        goto fail;
    }

    return 1;

    fail:
    udev_monitor_unref(core->monitor);
    core->monitor = NULL;

    if (core->udevMonitorFd >= 0)
        close(core->udevMonitorFd);

    if (core->monitorFd.fd >= 0)
        close(core->monitorFd.fd);

    return 0;
}

UInt32 dmaFormatsHaveInCommon(SRMList *formatsA, SRMList *formatsB)
{
    SRMListForeach(fmtI, formatsA)
    {
        SRMFormat *fmtA = srmListItemGetData(fmtI);

        if (fmtA->modifier == DRM_FORMAT_MOD_INVALID)
            continue;

        SRMListForeach(fmtJ, formatsB)
        {
            SRMFormat *fmtB = srmListItemGetData(fmtJ);

            if (fmtA->format == fmtB->format && fmtA->modifier == fmtB->modifier)
                return 1;
        }
    }

    return 0;
}

SRMDevice *srmCoreFindBestAllocatorDevice(SRMCore *core)
{
    SRMDevice *bestAllocatorDev = NULL;
    SRMDevice *primaryGPU = NULL;
    const char *preferredDevice = getenv("SRM_ALLOCATOR_DEVICE");

    SRMListForeach(item1, core->devices)
    {
        SRMDevice *allocDev = srmListItemGetData(item1);

        if (!srmDeviceIsEnabled(allocDev))
            continue;

        if (preferredDevice)
            if (strcmp(preferredDevice, allocDev->name) == 0)
                return allocDev;

        if (allocDev->isBootVGA)
            primaryGPU = allocDev;

        bestAllocatorDev = allocDev;
    }

    if (primaryGPU)
        return primaryGPU;

    return bestAllocatorDev;
}

void srmCoreAssignRendererDevices(SRMCore *core)
{
    SRMListForeach(item, core->devices)
    {
        SRMDevice *dev = srmListItemGetData(item);
        dev->rendererDevice = core->allocatorDevice;
    }
}

UInt8 srmCoreCheckPRIME(SRMDevice *target, SRMDevice *renderer)
{
    if (!renderer->gbmTestBo || gbm_bo_get_modifier(renderer->gbmTestBo) != DRM_FORMAT_MOD_LINEAR)
        return 0;

    UInt32 size = 8;

    SRMBufferDMAData dma;
    dma.format = gbm_bo_get_format(renderer->gbmTestBo);
    dma.width = size;
    dma.height = size;
    dma.num_fds = 1;
    dma.modifiers[0] = gbm_bo_get_modifier(renderer->gbmTestBo);
    dma.fds[0] = gbm_bo_get_fd(renderer->gbmTestBo);
    dma.strides[0] = gbm_bo_get_stride_for_plane(renderer->gbmTestBo, 0);
    dma.offsets[0] = gbm_bo_get_offset(renderer->gbmTestBo, 0);

    SRMBuffer *primeBuffer = srmBufferCreateFromDMA(
        target->core,
        target,
        &dma);

    if (!primeBuffer)
        return 0;

    UInt32 stride = size * 4;
    UInt8 *targetPixels = malloc(size * stride);
    UInt8 *rendererPixels = malloc(size * stride);

    eglMakeCurrent(renderer->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, renderer->eglSharedContext);

    glBindFramebuffer(GL_FRAMEBUFFER, renderer->testFB);

    UInt32 half = size/2;
    glScissor(0, 0, half, half);
    glViewport(0, 0, half, half);
    glClearColor(1.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    glScissor(half, 0, half, half);
    glViewport(half, 0, half, half);
    glClearColor(0.f, 1.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    glScissor(0, half, half, half);
    glViewport(0, half, half, half);
    glClearColor(0.f, 0.f, 1.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    glScissor(half, half, half, half);
    glViewport(half, half, half, half);
    glClearColor(1.f, 1.f, 1.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    glFinish();
    glReadPixels(0, 0, size, size, GL_RGBA, GL_UNSIGNED_BYTE, rendererPixels);

    eglMakeCurrent(target->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, target->eglSharedContext);

    GLuint textureId = srmBufferGetTextureID(target, primeBuffer);

    if (textureId == 0)
    {
        free(rendererPixels);
        free(targetPixels);
        srmBufferDestroy(primeBuffer);
        return 0;
    }

    glUseProgram(target->programTest);
    glBindFramebuffer(GL_FRAMEBUFFER, target->testFB);
    glDisable(GL_BLEND);
    glEnable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_BUFFER_BIT);
    glScissor(0, 0, size, size);
    glViewport(0, 0, size, size);
    glUniform1i(target->textureUniformTest, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glFinish();
    glReadPixels(0, 0, size, size, GL_RGBA, GL_UNSIGNED_BYTE, targetPixels);

    UInt8 ok = 1;

    for (UInt32 i = 0; i < size * size * 4; i++)
    {
        if (rendererPixels[i] != targetPixels[i])
        {
            ok = 0;
            break;
        }
    }

    free(rendererPixels);
    free(targetPixels);
    srmBufferDestroy(primeBuffer);
    SRMDebug("[core] PRIME import support from %s to %s: %s.", renderer->name, target->name, ok ? "YES" : "NO");
    return ok;
}

void srmCoreAssignRenderingModes(SRMCore *core)
{
    SRMListForeach(item, core->devices)
    {
        SRMDevice *dev = srmListItemGetData(item);

        if (dev == dev->rendererDevice)
        {
            dev->renderMode = SRM_RENDER_MODE_ITSELF;
            //continue;
        }

        if (dev->capPrimeImport && dev->rendererDevice->capPrimeExport && srmCoreCheckPRIME(dev, dev->rendererDevice))
        {
            dev->renderMode = SRM_RENDER_MODE_PRIME;
            continue;
        }

        if (dev->capDumbBuffer)
        {
            dev->renderMode = SRM_RENDER_MODE_DUMB;
            continue;
        }

        dev->renderMode = SRM_RENDER_MODE_CPU;
    }
}

UInt8 srmCoreUpdateBestConfiguration(SRMCore *core)
{
    SRMDevice *bestAllocatorDevice = srmCoreFindBestAllocatorDevice(core);

    if (!bestAllocatorDevice)
    {
        SRMFatal("No allocator device found.");
        return 0;
    }

    eglMakeCurrent(bestAllocatorDevice->eglDisplay,
                   EGL_NO_SURFACE,
                   EGL_NO_SURFACE,
                   bestAllocatorDevice->eglSharedContext);

    core->allocatorDevice = bestAllocatorDevice;
    srmCoreAssignRendererDevices(core);
    srmCoreUpdateSharedDMATextureFormats(core);
    srmCoreAssignRenderingModes(core);
    return 1;
}

void srmCoreUpdateSharedDMATextureFormats(SRMCore *core)
{
    srmFormatsListDestroy(&core->sharedDMATextureFormats);

    core->sharedDMATextureFormats = srmFormatsListCopy(core->allocatorDevice->dmaTextureFormats);

    /* If only 1 GPU, we allow implicit modifiers */
    if (srmListGetLength(core->devices) == 1)
        goto printFormats;

    SRMListForeach(devIt, core->devices)
    {
        SRMDevice *dev = srmListItemGetData(devIt);

        if (dev == core->allocatorDevice || !srmDeviceIsRenderer(dev))
            continue;

        SRMListItem *fmtIt = srmListGetFront(core->sharedDMATextureFormats);

        if (!fmtIt)
        {
            /* TODO: No formats in common could only happen in setups with 3 or more GPUs */
            return;
        }

        while (fmtIt)
        {
            SRMFormat *fmt = srmListItemGetData(fmtIt);

            SRMListItem *next = srmListItemGetNext(fmtIt);

            /* Do not use implicit modifiers since their meaning change in each GPU */
            if (fmt->modifier == DRM_FORMAT_MOD_INVALID)
            {
                free(fmt);
                srmListRemoveItem(core->sharedDMATextureFormats, fmtIt);
                goto setNext;
            }

            UInt8 isSupported = 0;

            /* Check if if the device support it as well */
            SRMListForeach(devFmtIt, dev->dmaTextureFormats)
            {
                SRMFormat *devFmt = srmListItemGetData(devFmtIt);

                if (fmt->format == devFmt->format && fmt->modifier == devFmt->modifier)
                {
                    isSupported = 1;
                    break;
                }
            }

            if (!isSupported)
            {
                free(fmt);
                srmListRemoveItem(core->sharedDMATextureFormats, fmtIt);
            }

            setNext:
            fmtIt = next;
        }
    }

    printFormats:

    if (SRMLogEGLGetLevel() > 3)
    {
        SRMDebug("[core] Supported shared DMA formats:");

        UInt32 prevFmt = 0;
        SRMListForeach(fmtIt, core->sharedDMATextureFormats)
        {
            SRMFormat *fmt = srmListItemGetData(fmtIt);

            if (fmtIt == srmListGetFront(core->sharedDMATextureFormats))
            {
                printf("  Format %s\t[%s", drmGetFormatName(fmt->format), drmGetFormatModifierName(fmt->modifier));
                goto next;
            }

            if (prevFmt == fmt->format)
                printf(", %s", drmGetFormatModifierName(fmt->modifier));
            else
            {
                printf("]\n  Format %s\t[%s", drmGetFormatName(fmt->format), drmGetFormatModifierName(fmt->modifier));
            }

            next:
            prevFmt = fmt->format;

        }
        printf("]\n");
    }
}

static void srmEGLLog(EGLenum error, const char *command, EGLint type, EGLLabelKHR thread, EGLLabelKHR obj, const char *msg)
{
    SRM_UNUSED(thread);
    SRM_UNUSED(obj);

    static const char *format = "[EGL] command: %s, error: %s (0x%x), message: \"%s\".";

    switch (type)
    {
        case EGL_DEBUG_MSG_CRITICAL_KHR:
            SRMFatal(format, command, srmEGLGetErrorString(error), error, msg);
            break;
        case EGL_DEBUG_MSG_ERROR_KHR:
            SRMError(format, command, srmEGLGetErrorString(error), error, msg);
            break;
        case EGL_DEBUG_MSG_WARN_KHR:
            SRMWarning(format, command, srmEGLGetErrorString(error), error, msg);
            break;
        case EGL_DEBUG_MSG_INFO_KHR:
            SRMDebug(format, command, srmEGLGetErrorString(error), error, msg);
            break;
        default:
            SRMDebug(format, command, srmEGLGetErrorString(error), error, msg);
            break;
    }
}

UInt8 srmCoreUpdateEGLFunctions(SRMCore *core)
{
    core->eglFunctions.eglGetPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC) eglGetProcAddress("eglGetPlatformDisplayEXT");

    if (core->eglExtensions.EXT_device_base || core->eglExtensions.EXT_device_enumeration)
        core->eglFunctions.eglQueryDevicesEXT = (PFNEGLQUERYDEVICESEXTPROC) eglGetProcAddress("eglQueryDevicesEXT");

    if (core->eglExtensions.EXT_device_base || core->eglExtensions.EXT_device_query)
    {
        core->eglExtensions.EXT_device_query = 1;
        core->eglFunctions.eglQueryDeviceStringEXT = (PFNEGLQUERYDEVICESTRINGEXTPROC) eglGetProcAddress("eglQueryDeviceStringEXT");
        core->eglFunctions.eglQueryDisplayAttribEXT = (PFNEGLQUERYDISPLAYATTRIBEXTPROC) eglGetProcAddress("eglQueryDisplayAttribEXT");
    }

    if (core->eglExtensions.KHR_debug)
    {
        core->eglFunctions.eglDebugMessageControlKHR = (PFNEGLDEBUGMESSAGECONTROLKHRPROC) eglGetProcAddress("eglDebugMessageControlKHR");

        UInt32 level = 0;
        const char *env = getenv("SRM_EGL_DEBUG");

        if (env)
            level = atoi(env);

        EGLAttrib debugAttribs[] =
        {
            EGL_DEBUG_MSG_CRITICAL_KHR, level > 0 ? EGL_TRUE : EGL_FALSE,
            EGL_DEBUG_MSG_ERROR_KHR,    level > 1 ? EGL_TRUE : EGL_FALSE,
            EGL_DEBUG_MSG_WARN_KHR,     level > 2 ? EGL_TRUE : EGL_FALSE,
            EGL_DEBUG_MSG_INFO_KHR,     level > 3 ? EGL_TRUE : EGL_FALSE,
            EGL_NONE,
        };

        core->eglFunctions.eglDebugMessageControlKHR(srmEGLLog, debugAttribs);
    }

    return 1;
}
