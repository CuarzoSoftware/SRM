#include <private/SRMCorePrivate.h>
#include <private/SRMDevicePrivate.h>
#include <private/SRMListenerPrivate.h>
#include <private/SRMConnectorPrivate.h>
#include <SRMFormat.h>
#include <SRMList.h>
#include <SRMLog.h>
#include <xf86drmMode.h>
#include <sys/poll.h>
#include <sys/epoll.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

SRMCore *srmCoreCreate(SRMInterface *interface, void *userData)
{
    SRMLogInit();

    setenv("SRM_FORCE_LEGACY_API", "1", 0);

    // REF 1
    SRMCore *core = calloc(1, sizeof(SRMCore));

    core->version.major = SRM_VERSION_MAJOR;
    core->version.minor = SRM_VERSION_MINOR;
    core->version.patch = SRM_VERSION_PATCH;
    core->version.build = SRM_VERSION_BUILD;

    SRMDebug("[core] SRM version %d.%d.%d-%d.",
             SRM_VERSION_MAJOR,
             SRM_VERSION_MINOR,
             SRM_VERSION_PATCH,
             SRM_VERSION_BUILD);

    core->interface = interface;
    core->interfaceUserData = userData;
    core->isSuspended = 0;

    // REF -
    if (!srmCoreUpdateEGLExtensions(core))
        goto fail;

    // REF -
    if (!srmCoreUpdateEGLFunctions(core))
        goto fail;

    // REF 2
    if (!srmCoreCreateUdev(core))
        goto fail;

    // REF 3
    core->connectorPluggedListeners = srmListCreate();

    // REF 4
    core->connectorUnpluggedListeners = srmListCreate();

    // REF 5
    core->deviceCreatedListeners = srmListCreate();

    // REF 6
    core->deviceRemovedListeners = srmListCreate();

    // REF 7
    core->devices = srmListCreate();

    // REF 8
    if (!srmCoreInitDeallocator(core))
        goto fail;

    // REF 7
    if (!srmCoreEnumerateDevices(core)) // Fails if no device found
        goto fail;

    // REF 9
    if (!srmCoreInitMonitor(core))
        goto fail;

    // REF 10
    srmCoreUpdateBestConfiguration(core);

    return core;

    fail:
        srmCoreDestroy(core);

    return NULL;
}

void srmCoreDestroy(SRMCore *core)
{
    // UNREF 7
    if (core->devices)
    {
        // First uninitialize all connectors
        SRMListForeach(devIt, core->devices)
        {
            SRMDevice *dev = srmListItemGetData(devIt);

            SRMListForeach(connIt, dev->connectors)
            {
                SRMConnector *conn = srmListItemGetData(connIt);
                srmConnectorUninitialize(conn);
            }
        }

        // Destroy devices
        while (!srmListIsEmpty(core->devices))
            srmDeviceDestroy(srmListItemGetData(srmListGetBack(core->devices)));

        srmListDestroy(core->devices);
    }

    // UNREF 3
    if (core->connectorPluggedListeners)
    {
        while (!srmListIsEmpty(core->connectorPluggedListeners))
            srmListenerDestroy(srmListItemGetData(srmListGetBack(core->connectorPluggedListeners)));

        srmListDestroy(core->connectorPluggedListeners);
    }

    // UNREF 4
    if (core->connectorUnpluggedListeners)
    {
        while (!srmListIsEmpty(core->connectorUnpluggedListeners))
            srmListenerDestroy(srmListItemGetData(srmListGetBack(core->connectorUnpluggedListeners)));

        srmListDestroy(core->connectorUnpluggedListeners);
    }

    // UNREF 5
    if (core->deviceCreatedListeners)
    {
        while (!srmListIsEmpty(core->deviceCreatedListeners))
            srmListenerDestroy(srmListItemGetData(srmListGetBack(core->deviceCreatedListeners)));

        srmListDestroy(core->deviceCreatedListeners);
    }

    // UNREF 6
    if (core->deviceRemovedListeners)
    {
        while (!srmListIsEmpty(core->deviceRemovedListeners))
            srmListenerDestroy(srmListItemGetData(srmListGetBack(core->deviceRemovedListeners)));

        srmListDestroy(core->deviceRemovedListeners);
    }

    // UNREF 9
    if (core->monitor)
        udev_monitor_unref(core->monitor);

    // UNREF 8
    srmCoreUnitDeallocator(core);

    if (core->udevMonitorFd >= 0)
        close(core->udevMonitorFd);

    if (core->monitorFd.fd >= 0)
        close(core->monitorFd.fd);

    // UNREF 2
    if (core->udev)
        udev_unref(core->udev);

    // UNREF 10
    srmFormatsListDestroy(&core->sharedDMATextureFormats);

    // UNREF 1
    free(core);
}

UInt8 srmCoreSuspend(SRMCore *core)
{
    if (core->isSuspended)
        return 0;

    SRMListForeach (deviceIt, srmCoreGetDevices(core))
    {
        SRMDevice *device = srmListItemGetData(deviceIt);

        SRMListForeach (connectorIt, srmDeviceGetConnectors(device))
        {
            SRMConnector *connector = srmListItemGetData(connectorIt);
            srmConnectorSuspend(connector);
        }
    }

    core->isSuspended = 1;

    if (epoll_ctl(core->monitorFd.fd, EPOLL_CTL_DEL, core->udevMonitorFd, NULL) != 0)
    {
        SRMError("[core] Failed to remove udev monitor fd from epoll.");
        return 0;
    }

    return 1;
}

UInt8 srmCoreResume(SRMCore *core)
{
    if (!core->isSuspended)
        return 0;

    SRMListForeach (deviceIt, srmCoreGetDevices(core))
    {
        SRMDevice *device = srmListItemGetData(deviceIt);

        SRMListForeach (connectorIt, srmDeviceGetConnectors(device))
        {
            SRMConnector *connector = srmListItemGetData(connectorIt);
            srmConnectorResume(connector);
        }
    }

    core->isSuspended = 0;

    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = core->udevMonitorFd;

    if (epoll_ctl(core->monitorFd.fd, EPOLL_CTL_ADD, core->udevMonitorFd, &event) != 0)
    {
        SRMError("[core] Failed to add udev monitor fd to epoll.");
        return 0;
    }

    return 1;
}

UInt8 srmCoreIsSuspended(SRMCore *core)
{
    return core->isSuspended;
}

SRMVersion *srmCoreGetVersion(SRMCore *core)
{
    return &core->version;
}

SRMList *srmCoreGetDevices(SRMCore *core)
{
    return core->devices;
}

SRMDevice *srmCoreGetAllocatorDevice(SRMCore *core)
{
    return core->allocatorDevice;
}

Int32 srmCoreGetMonitorFD(SRMCore *core)
{
    return core->monitorFd.fd;
}

Int32 srmCoreProcessMonitor(SRMCore *core, Int32 msTimeout)
{
    int ret;

    if (core->isSuspended)
        goto checkPoll;

    SRMListForeach(deviceIt, core->devices)
    {
        SRMDevice *dev = srmListItemGetData(deviceIt);

        if (dev->pendingUdevEvents)
        {
            // If not master
            if (!srmDeviceHandleHotpluggingEvent(dev))
                msTimeout = 500;
        }
    }

    checkPoll:

    ret = poll(&core->monitorFd, 1, msTimeout);

    if (!core->isSuspended && ret > 0 && core->monitorFd.revents & POLLIN)
    {
        struct udev_device *dev = udev_monitor_receive_device(core->monitor);

        if (dev)
        {
            const char *action = udev_device_get_action(dev);
            const char *devnode = udev_device_get_devnode(dev);

            if (devnode && strncmp(devnode, "/dev/dri/card", 13) == 0)
            {
                // Find the device
                SRMDevice *device = NULL;

                SRMListForeach(deviceIt, core->devices)
                {
                    SRMDevice *dev = srmListItemGetData(deviceIt);

                    if (strcmp(dev->name, devnode) == 0)
                    {
                        device = dev;
                        break;
                    }
                }

                if (!device)
                {
                    /* TODO: Handle GPU hotplugging */
                    goto unref;
                }

                // Possible connector hotplug event
                if (strcmp(action, "change") == 0)
                {
                    srmDeviceHandleHotpluggingEvent(device);
                }

                // GPU added
                else if (strcmp(action, "add") == 0)
                {
                    SRMDebug("[core] DRM device added: %s.", devnode);

                    /* TODO: Should ask user to re-init or simply add connectors in a non optimal way if possible?
                    for (SRMListener *listener : imp()->deviceCreatedListeners)
                    {
                        void (*callback)(SRMListener *, SRMDevice *) = (void(*)(SRMListener *, SRMDevice *))listener->callback();
                        callback(listener, nullptr);
                    } */
                }

                // GPU removed
                else if (strcmp(action, "remove") == 0)
                {
                    /* The kernel does not permit driver removal while it is in use, 
                    so this action may not be meaningful. 
                    It could potentially be more relevant in the context of physically 
                    unplugging hardware tho. */

                    SRMDebug("[core] DRM device removed: %s.", devnode);
                }
            }

            unref:
            udev_device_unref(dev);
        }
    }

    return ret;
}

SRMListener *srmCoreAddDeviceCreatedEventListener(SRMCore *core, void (*callback)(SRMListener *, SRMDevice *), void *userData)
{
    return srmListenerCreate(core->deviceCreatedListeners, callback, userData);
}

SRMListener *srmCoreAddDeviceRemovedEventListener(SRMCore *core, void (*callback)(SRMListener *, SRMDevice *), void *userData)
{
    return srmListenerCreate(core->deviceRemovedListeners, callback, userData);
}

SRMListener *srmCoreAddConnectorPluggedEventListener(SRMCore *core, void (*callback)(SRMListener *, SRMConnector *), void *userData)
{
    return srmListenerCreate(core->connectorPluggedListeners, callback, userData);
}

SRMListener *srmCoreAddConnectorUnpluggedEventListener(SRMCore *core, void (*callback)(SRMListener *, SRMConnector *), void *userData)
{
    return srmListenerCreate(core->connectorUnpluggedListeners, callback, userData);
}

const SRMEGLCoreExtensions *srmCoreGetEGLExtensions(SRMCore *core)
{
    return &core->eglExtensions;
}

const SRMEGLCoreFunctions *srmCoreGetEGLFunctions(SRMCore *core)
{
    return &core->eglFunctions;
}

SRMList *srmCoreGetSharedDMATextureFormats(SRMCore *core)
{
    return core->sharedDMATextureFormats;
}

void *srmCoreGetUserData(SRMCore *core)
{
    return core->interfaceUserData;
}

void srmCoreSetUserData(SRMCore *core, void *userData)
{
    core->interfaceUserData = userData;
}
