#include <private/SRMCorePrivate.h>
#include <private/SRMDevicePrivate.h>
#include <private/SRMListenerPrivate.h>
#include <SRMLog.h>

#include <xf86drmMode.h>
#include <sys/poll.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>


#include <SRMList.h>

/*

SRMCore::~SRMCore()
{
    while(!devices().empty())
    {
        SRMDevice *device = devices().back();
        delete device;
        imp()->devices.pop_back();
    }

    delete m_imp;
}



SRMListener *SRMCore::addDeviceCreatedListener(void (*callback)(SRMListener *, SRMDevice *), void *userdata)
{
    return new SRMListener(&imp()->deviceCreatedListeners, (void*)callback, userdata);
}

SRMListener *SRMCore::addDeviceRemovedListener(void (*callback)(SRMListener *, SRMDevice *), void *userdata)
{
    return new SRMListener(&imp()->deviceRemovedListeners, (void*)callback, userdata);
}

SRMListener *SRMCore::addConnectorPluggedListener(void (*callback)(SRMListener *, SRMConnector *), void *userdata)
{
    return new SRMListener(&imp()->connectorPluggedListeners, (void*)callback, userdata);
}

SRMListener *SRMCore::addConnectorUnpluggedListener(void (*callback)(SRMListener *, SRMConnector *), void *userdata)
{
    return new SRMListener(&imp()->connectorUnpluggedListeners, (void*)callback, userdata);
}

SRMCore::SRMCorePrivate *SRMCore::imp() const
{
    return m_imp;
}
*/
SRMCore *srmCoreCreate(SRMInterface *interface, void *userData)
{
    SRMLogInit();
    SRMCore *core = calloc(1, sizeof(SRMCore));
    core->interface = interface;
    core->interfaceUserData = userData;

    if (!srmCoreCreateUdev(core))
        goto fail;

    core->connectorPluggedListeners = srmListCreate();
    core->connectorUnpluggedListeners = srmListCreate();
    core->deviceCreatedListeners = srmListCreate();
    core->deviceRemovedListeners = srmListCreate();
    core->devices = srmListCreate();

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
                // Possible connector hotplug event
                if (strcmp(action, "change") == 0)
                {
                    int card_fd = -1;

                    card_fd = open(devnode, O_RDONLY | O_CLOEXEC);

                    if (card_fd < 0)
                    {
                        SRMError("Failed to open device %s.", devnode);
                        goto unref;
                    }

                    drmModeRes *res = drmModeGetResources(card_fd);

                    if (res)
                    {
                        for (int i = 0; i < res->count_connectors; i++)
                        {
                            drmModeConnector *conn = drmModeGetConnector(card_fd, res->connectors[i]);
                            if (conn)
                            {
                                if (conn->connection == DRM_MODE_CONNECTED)
                                {
                                    /*
                                    printf("Connector %d connected\n", conn->connector_id);

                                    for (SRMListener *listener : imp()->connectorPluggedListeners)
                                    {
                                        void (*callback)(SRMListener *, SRMConnector *) = (void(*)(SRMListener *, SRMConnector *))listener->callback();
                                        callback(listener, nullptr);
                                    }
                                    */
                                }
                                else
                                {
                                    // printf("Connector %d disconnected\n", conn->connector_id);
                                }

                                drmModeFreeConnector(conn);
                            }
                        }

                        drmModeFreeResources(res);
                    }

                    close(card_fd);
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
