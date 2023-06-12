#include <private/SRMCorePrivate.h>
#include <private/SRMDevicePrivate.h>
#include <private/SRMConnectorPrivate.h>

#include <SRMFormat.h>
#include <SRMLog.h>
#include <SRMList.h>

#include <stdio.h>
#include <stdlib.h>

UInt8 srmCoreUpdateEGLExtensions(SRMCore *core)
{
    if (eglBindAPI(EGL_OPENGL_ES_API) == EGL_FALSE)
    {
        SRMFatal("Failed to bind to the OpenGL ES API.");
        return 0;
    }

    const char *extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);

    if (!extensions)
    {
        SRMFatal("Failed to query core EGL extensions.");
        return 0;
    }

    core->eglExtensions.EXT_platform_base = srmEGLHasExtension(extensions, "EGL_EXT_platform_base");

    if (!core->eglExtensions.EXT_platform_base)
    {
        SRMFatal("EGL_EXT_platform_base not supported.");
        return 0;
    }

    core->eglExtensions.KHR_platform_gbm = srmEGLHasExtension(extensions, "EGL_KHR_platform_gbm");
    core->eglExtensions.MESA_platform_gbm = srmEGLHasExtension(extensions, "EGL_MESA_platform_gbm");

    if (!core->eglExtensions.KHR_platform_gbm && !core->eglExtensions.MESA_platform_gbm)
    {
        SRMFatal("EGL_KHR_platform_gbm not supported.");
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
        SRMFatal("Failed to create udev context.");
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

        SRMDevice *device = srmDeviceCreate(core, udev_device_get_devnode(dev));

        if (device)
        {
            device->coreLink = srmListAppendData(core->devices, device);

            // Update device connector names
            SRMListForeach (item, device->connectors)
            {
                SRMConnector *connector = srmListItemGetData(item);
                srmConnectorUpdateNames(connector);
            }
        }


        udev_device_unref(dev);
    }

    udev_enumerate_unref(enumerate);

    return 1;
}

UInt8 srmCoreInitMonitor(SRMCore *core)
{
    core->monitor = udev_monitor_new_from_netlink(core->udev, "udev");

    if (!core->monitor)
    {
        SRMFatal("Failed to create udev monitor.");
        return 0;
    }

    if (udev_monitor_filter_add_match_subsystem_devtype(core->monitor, "drm", "drm_minor") < 0)
    {
        SRMFatal("Failed to add udev monitor filter.");
        goto fail;
    }

    if (udev_monitor_enable_receiving(core->monitor) < 0)
    {
        SRMFatal("Failed to enable udev monitor receiving.");
        goto fail;
    }

    core->monitorFd.fd = udev_monitor_get_fd(core->monitor);

    if (core->monitorFd.fd < 0)
    {
        SRMFatal("Failed to get udev monitor fd.");
        goto fail;
    }

    core->monitorFd.events = POLLIN | POLLHUP;
    core->monitorFd.revents = 0;

    return 1;

    fail:
    udev_monitor_unref(core->monitor);
    core->monitor = NULL;
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
    int bestScore = 0;

    SRMListForeach(item1, core->devices)
    {
        SRMDevice *allocDev = srmListItemGetData(item1);

        if (!srmDeviceIsEnabled(allocDev))
            continue;

        int currentScore = srmDeviceGetCapPrimeExport(allocDev) ? 100 : 10;

        SRMListForeach(item2, core->devices)
        {
            SRMDevice *otherDev = srmListItemGetData(item2);

            if (!srmDeviceIsEnabled(otherDev) || allocDev == otherDev)
                continue;

            // GPU can render
            if (srmDeviceGetCapPrimeExport(allocDev) &&
                    srmDeviceGetCapPrimeImport(otherDev) &&
                    dmaFormatsHaveInCommon(allocDev->dmaTextureFormats, otherDev->dmaTextureFormats))
            {

                currentScore += 100;
                continue;
            }
            // GPU can not render but glRead > dumbBuffer
            else if (srmDeviceGetCapDumbBuffer(otherDev))
            {
                currentScore += 20;
                continue;
            }
            // GPU can not render glRead > CPU > glTex2D > render
            else
            {
                currentScore += 10;
                continue;
            }

        }

        if (currentScore > bestScore)
        {
            bestScore = currentScore;
            bestAllocatorDev = allocDev;
        }

    }

    return bestAllocatorDev;
}

void srmCoreAssignRendererDevices(SRMCore *core)
{
    int enabledDevicesN = 0;

    // Count enabled GPUs
    SRMListForeach(item, core->devices)
    {
        SRMDevice *dev = srmListItemGetData(item);
        enabledDevicesN += dev->enabled;
    }

    SRMListForeach(item, core->devices)
    {
        SRMDevice *dev = srmListItemGetData(item);

        if (!dev->enabled)
        {
            dev->rendererDevice = NULL;
            continue;
        }

        // If only one GPU or can import from allocator
        if (enabledDevicesN == 1 || (dev->capPrimeImport && core->allocatorDevice->capPrimeExport))
        {
            dev->rendererDevice = dev;
            continue;
        }

        int bestScore = 0;
        SRMDevice *bestRendererForDev = NULL;

        SRMListForeach(item, core->devices)
        {
            SRMDevice *rendererDevice = srmListItemGetData(item);

            if (!rendererDevice->enabled)
                continue;

            int currentScore = 0;

            if (dev == rendererDevice)
                currentScore += 10;

            if (!rendererDevice->capDumbBuffer)
                currentScore += 20;

            if (rendererDevice->capPrimeImport && core->allocatorDevice->capPrimeExport)
                currentScore += 100;

            if (rendererDevice == core->allocatorDevice)
                currentScore += 50;

            if (currentScore > bestScore)
            {
                bestRendererForDev = rendererDevice;
                bestScore = currentScore;
            }
        }

        dev->rendererDevice = bestRendererForDev;

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

    /*
    if (allocatorDevice && allocatorDevice != bestAllocatorDevice)
    {
        // TODO may require update
    }
    */

    core->allocatorDevice = bestAllocatorDevice;

    srmCoreAssignRendererDevices(core);
    srmCoreUpdateSharedDMATextureFormats(core);

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
            SRMListForeach(devFmtIt, dev->dmaRenderFormats)
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


    if (SRMLogGetLevel() > 3)
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


void *srmCoreDeallocatorLoop(void *data)
{
    SRMCore *core = data;

    UInt8 finish = 0;

    SRMDebug("[core] Deallocator thread started.");

    while (1)
    {
        pthread_mutex_lock(&core->deallocatorMutex);
        pthread_cond_wait(&core->deallocatorCond, &core->deallocatorMutex);

        while (!srmListIsEmpty(core->deallocatorMessages))
        {
            struct SRMDeallocatorThreadMessage *message = srmListPopBack(core->deallocatorMessages);

            if (message->msg == SRM_DEALLOCATOR_MSG_DESTROY_BUFFER)
            {

                eglMakeCurrent(message->device->eglDisplay,
                               EGL_NO_SURFACE,
                               EGL_NO_SURFACE,
                               message->device->eglDeallocatorContext);


                if (message->textureID)
                {
                    SRMDebug("[%s] GL Texture (%d) deleted.", message->device->name, message->textureID);
                    glDeleteTextures(1, &message->textureID);
                }

                if (message->image != EGL_NO_IMAGE)
                    eglDestroyImage(message->device->eglDisplay, message->image);

                core->deallocatorState = 1;


            }
            else if (message->msg == SRM_DEALLOCATOR_MSG_CREATE_CONTEXT)
            {
                message->device->eglDeallocatorContext = eglCreateContext(message->device->eglDisplay,
                                                                          NULL,
                                                                          message->device->eglSharedContext,
                                                                          message->device->eglSharedContextAttribs);

                if (message->device->eglDeallocatorContext == EGL_NO_CONTEXT)
                {
                    SRMError("Failed to create deallocator EGL context for device %s.", message->device->name);
                    core->deallocatorState = -1;
                    free(message);
                    break;
                }

                core->deallocatorState = 1;
            }
            else
            {
                finish = 1;
                core->deallocatorState = 1;
            }

            free(message);
        }

        pthread_mutex_unlock(&core->deallocatorMutex);

        if (finish)
            return NULL;
    }
}


UInt8 srmCoreInitDeallocator(SRMCore *core)
{
    pthread_mutex_init(&core->deallocatorMutex, NULL);
    pthread_cond_init(&core->deallocatorCond, NULL);
    core->deallocatorMessages = srmListCreate();

    if (pthread_create(&core->deallocatorThread, NULL, srmCoreDeallocatorLoop, core))
    {
        SRMFatal("[core] Could not start render thread for device %s connector %d.");
        pthread_mutex_destroy(&core->deallocatorMutex);
        pthread_cond_destroy(&core->deallocatorCond);
        srmListDestoy(core->deallocatorMessages);
        core->deallocatorMessages = NULL;
        return 0;
    }

    return 1;
}

void srmCoreSendDeallocatorMessage(SRMCore *core,
                                    enum SRM_DEALLOCATOR_MSG msg,
                                    SRMDevice *device,
                                    GLuint textureID,
                                    EGLImage image)
{
    pthread_mutex_lock(&core->deallocatorMutex);
    core->deallocatorState = 0;
    struct SRMDeallocatorThreadMessage *message = malloc(sizeof(struct SRMDeallocatorThreadMessage));
    message->msg = msg;
    message->device = device;
    message->textureID = textureID;
    message->image = image;
    srmListAppendData(core->deallocatorMessages, message);
    pthread_cond_signal(&core->deallocatorCond);
    pthread_mutex_unlock(&core->deallocatorMutex);
}
