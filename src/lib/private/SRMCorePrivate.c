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

    core->monitorFd.fd = epoll_create1(0);

    if (core->monitorFd.fd < 0)
    {
        SRMFatal("[core] Failed to create udev epoll fd.");
        goto fail;
    }

    core->monitorFd.events = POLLIN | POLLHUP;
    core->monitorFd.revents = 0;

    struct epoll_event event;
    event.events = EPOLLIN | EPOLLHUP;
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

    /* This should be invoked upon GPU hotplug events, but it may affect user resources.
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

static void *srmCoreDeallocatorLoop(void *data)
{
    SRMCore *core = data;

    UInt8 finish = 0;

    SRMDebug("[core] Deallocator thread started.");

    while (1)
    {
        pthread_mutex_lock(&core->deallocatorMutex);

        if (srmListIsEmpty(core->deallocatorMessages))
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
                if (message->device->eglExtensions.IMG_context_priority)
                    message->device->eglSharedContextAttribs[3] = EGL_CONTEXT_PRIORITY_LOW_IMG;

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
            else if (message->msg == SRM_DEALLOCATOR_MSG_DESTROY_CONTEXT)
            {
                if (message->device->eglDeallocatorContext != EGL_NO_CONTEXT)
                {
                    eglDestroyContext(message->device->eglDisplay, message->device->eglDeallocatorContext);
                    eglReleaseThread();
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
        {
            core->deallocatorRunning = 0;
            return NULL;
        }
    }
}

UInt8 srmCoreInitDeallocator(SRMCore *core)
{
    pthread_mutex_init(&core->deallocatorMutex, NULL);
    pthread_cond_init(&core->deallocatorCond, NULL);
    core->deallocatorMessages = srmListCreate();
    core->deallocatorRunning = 1;

    if (pthread_create(&core->deallocatorThread, NULL, srmCoreDeallocatorLoop, core))
    {
        SRMFatal("[core] Could not start render thread for device %s connector %d.");
        pthread_mutex_destroy(&core->deallocatorMutex);
        pthread_cond_destroy(&core->deallocatorCond);
        srmListDestroy(core->deallocatorMessages);
        core->deallocatorMessages = NULL;
        core->deallocatorRunning = 0;
        return 0;
    }

    return 1;
}

void srmCoreUnitDeallocator(SRMCore *core)
{
    if (core->deallocatorRunning)
    {
        srmCoreSendDeallocatorMessage(core,
                                      SRM_DEALLOCATOR_MSG_STOP_THREAD,
                                      NULL,
                                      0,
                                      EGL_NO_IMAGE);

        while (core->deallocatorRunning)
            usleep(1000);

        srmListDestroy(core->deallocatorMessages);
        pthread_cond_destroy(&core->deallocatorCond);
        pthread_mutex_destroy(&core->deallocatorMutex);
    }
}

void srmCoreSendDeallocatorMessage(SRMCore *core,
                                    enum SRM_DEALLOCATOR_MSG msg,
                                    SRMDevice *device,
                                    GLuint textureID,
                                    EGLImage image)
{
    if (!core->deallocatorRunning)
    {
        core->deallocatorState = 1;
        return;
    }

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

#if SRM_PAR_CPY == 1
static void *srmCoreCopyThreadLoop(void *data)
{
    struct SRMCopyThread *thread = data;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(thread->cpu, &cpuset);

    if (pthread_setaffinity_np(thread->thread, sizeof(cpu_set_t), &cpuset) != 0)
        SRMDebug("[core] Failed to set copy thread to CPU %d.", thread->cpu);
    else
        SRMDebug("[core] New copy thread assigned to CPU %d.", thread->cpu);

    thread->state = 1;

    while (1)
    {
        pthread_mutex_lock(&thread->mutex);

        if (thread->finished)
            pthread_cond_wait(&thread->cond, &thread->mutex);

        if (thread->state == 1)
        {
            Int32 a,b;
            // If not copied yet
            for (Int32 i = 0; i < thread->rows; i++)
            {
                memmove(&thread->dst[ (thread->offsetY + i) * thread->dstStride],
                       &thread->src[ (thread->offsetY + i) * thread->srcStride],
                       thread->size);
            }

            thread->finished = 1;
        }
        else
        {
            thread->state = 3;
            pthread_mutex_unlock(&thread->mutex);
            return NULL;
        }

        pthread_mutex_unlock(&thread->mutex);
    }
}

void srmCoreInitCopyThreads(SRMCore *core)
{
    core->copyThreadsCount = sysconf(_SC_NPROCESSORS_ONLN);

    if (core->copyThreadsCount <= 1)
    {
        core->copyThreadsCount = 0;
        return;
    }

    if (core->copyThreadsCount > SRM_MAX_COPY_THREADS)
        core->copyThreadsCount = SRM_MAX_COPY_THREADS;

    SRMDebug("[core] CPU cores avaliable: %d.", core->copyThreadsCount);

    pthread_attr_t attr;
    struct sched_param param;

    // Initialize thread attributes
    pthread_attr_init(&attr);

    // Set scheduling policy to SCHED_FIFO
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);

    // Set the priority using sched_param
    param.sched_priority = 99;
    pthread_attr_setschedparam(&attr, &param);

    for (UInt8 i = 0; i < core->copyThreadsCount; i++)
    {
        core->copyThreads[i].finished = 1;
        core->copyThreads[i].core = core;
        core->copyThreads[i].cpu = i;
        pthread_mutex_init(&core->copyThreads[i].mutex, NULL);
        pthread_cond_init(&core->copyThreads[i].cond, NULL);
        pthread_create(&core->copyThreads[i].thread, &attr, srmCoreCopyThreadLoop, &core->copyThreads[i]);
    }

    checkThreadsOK:

    for (UInt8 i = 0; i < core->copyThreadsCount; i++)
    {
        if (!core->copyThreads[i].state)
            goto checkThreadsOK;
    }
}

// Buffers pointers must already contain (x,y) offset
UInt8 srmCoreCopy(SRMCore *core, UInt8 *src, UInt8 *dst, Int32 srcStride, Int32 dstStride, Int32 size, Int32 height)
{
    if (!core->copyThreadsCount)
        return 0;

    UInt8 workingThreads = 0;

    struct SRMCopyThread *thread;

    // If there the number of threads is equal or greater the rows to copy
    // a single row is assigned to each thread
    if (height <= core->copyThreadsCount)
    {
        workingThreads = height;

        for (UInt8 i = 0; i < workingThreads; i++)
        {
            thread = &core->copyThreads[i];
            pthread_mutex_lock(&thread->mutex);
            thread->finished = 0;
            thread->src = src;
            thread->dst = dst;
            thread->srcStride = srcStride;
            thread->dstStride = dstStride;
            thread->size = size;
            thread->offsetY = i;
            thread->rows = 1;
            pthread_cond_signal(&thread->cond);
            pthread_mutex_unlock(&thread->mutex);
        }
    }
    else
    {
        workingThreads = core->copyThreadsCount;

        Int32 rowsPerThread = height / workingThreads;
        Int32 offsetY = 0;

        for (UInt8 i = 0; i < workingThreads - 1; i++)
        {
            thread = &core->copyThreads[i];
            pthread_mutex_lock(&thread->mutex);
            thread->finished = 0;
            thread->src = src;
            thread->dst = dst;
            thread->srcStride = srcStride;
            thread->dstStride = dstStride;
            thread->size = size;
            thread->offsetY = offsetY;
            thread->rows = rowsPerThread;
            offsetY += rowsPerThread;
            pthread_cond_signal(&thread->cond);
            pthread_mutex_unlock(&thread->mutex);
        }

        thread = &core->copyThreads[workingThreads - 1];
        pthread_mutex_lock(&thread->mutex);
        thread->finished = 0;
        thread->src = src;
        thread->dst = dst;
        thread->srcStride = srcStride;
        thread->dstStride = dstStride;
        thread->size = size;
        thread->offsetY = offsetY;
        thread->rows = height - offsetY;
        pthread_cond_signal(&thread->cond);
        pthread_mutex_unlock(&thread->mutex);
    }

    return 1;
}
#endif
