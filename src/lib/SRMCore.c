#include <private/SRMCorePrivate.h>
#include <private/SRMDevicePrivate.h>
#include <private/SRMListenerPrivate.h>
#include <private/SRMConnectorPrivate.h>
#include <SRMList.h>
#include <SRMLog.h>
#include <xf86drmMode.h>
#include <sys/poll.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

SRMCore *srmCoreCreate(SRMInterface *interface, void *userData)
{
    SRMLogInit();
    SRMCore *core = calloc(1, sizeof(SRMCore));
    core->interface = interface;
    core->interfaceUserData = userData;

    if (!srmCoreUpdateEGLExtensions(core))
        goto fail;

    if (!srmCoreUpdateEGLFunctions(core))
        goto fail;

    if (!srmCoreCreateUdev(core))
        goto fail;

    core->connectorPluggedListeners = srmListCreate();
    core->connectorUnpluggedListeners = srmListCreate();
    core->deviceCreatedListeners = srmListCreate();
    core->deviceRemovedListeners = srmListCreate();
    core->devices = srmListCreate();

    if (!srmCoreInitDeallocator(core))
        goto fail;

    if (!srmCoreEnumerateDevices(core)) // Fails if no device found
        goto fail;

    if (!srmCoreInitMonitor(core))
        goto fail;

    srmCoreUpdateBestConfiguration(core);

    return core;

    fail:
        srmCoreDestroy(core);

    return NULL;
}

void srmCoreDestroy(SRMCore *core)
{
    // Destroy lists

    if (core->connectorPluggedListeners)
    {
        while (!srmListIsEmpty(core->connectorPluggedListeners))
            srmListenerDestroy(srmListItemGetData(srmListGetBack(core->connectorPluggedListeners)));
    }

    if (core->connectorUnpluggedListeners)
    {
        while (!srmListIsEmpty(core->connectorUnpluggedListeners))
            srmListenerDestroy(srmListItemGetData(srmListGetBack(core->connectorUnpluggedListeners)));
    }

    if (core->deviceCreatedListeners)
    {
        while (!srmListIsEmpty(core->deviceCreatedListeners))
            srmListenerDestroy(srmListItemGetData(srmListGetBack(core->deviceCreatedListeners)));
    }

    if (core->deviceRemovedListeners)
    {
        while (!srmListIsEmpty(core->deviceRemovedListeners))
            srmListenerDestroy(srmListItemGetData(srmListGetBack(core->deviceRemovedListeners)));
    }

    if (core->devices)
    {
        // TODO handle devices removal
        srmListDestoy(core->devices);
    }

    if (core->monitor)
        udev_monitor_unref(core->monitor);

    srmCoreUnitDeallocator(core);

    srmListDestoy(core->connectorPluggedListeners);
    srmListDestoy(core->connectorUnpluggedListeners);
    srmListDestoy(core->deviceCreatedListeners);
    srmListDestoy(core->deviceRemovedListeners);
    srmListDestoy(core->devices);

    if (core->udev)
        udev_unref(core->udev);

    free(core);
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

Int32 srmCoreProccessMonitor(SRMCore *core, Int32 msTimeout)
{
    int ret = poll(&core->monitorFd, 1, msTimeout);

    if (ret > 0 && core->monitorFd.revents & POLLIN)
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
                                SRMListForeach(listenerIt, core->connectorPluggedListeners)
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
                                SRMListForeach(listenerIt, core->connectorUnpluggedListeners)
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
                }

                // GPU added
                else if (strcmp(action, "add") == 0)
                {
                    /*
                    for (SRMListener *listener : imp()->deviceCreatedListeners)
                    {
                        void (*callback)(SRMListener *, SRMDevice *) = (void(*)(SRMListener *, SRMDevice *))listener->callback();
                        callback(listener, nullptr);
                    }
                    */
                }

                // GPU removed
                else if (strcmp(action, "remove") == 0)
                {
                    // printf("DRM device removed: %s\n", devnode);
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
