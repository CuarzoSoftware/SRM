#include <private/SRMCorePrivate.h>
#include <private/SRMDevicePrivate.h>
#include <SRMListener.h>
#include <SRMLog.h>
#include <stdio.h>
#include <sys/poll.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <xf86drmMode.h>
#include <time.h>

using namespace SRM;

SRMCore::SRMCore()
{
    SRMLog::init();
    srand(time(NULL));
    m_imp = new SRMCorePrivate(this);
}

SRMCore *SRMCore::createSRM(SRMInterface *interface, void *userdata)
{
    SRMCore *srm = new SRMCore();
    srm->imp()->interface = interface;
    srm->imp()->userdata = userdata;

    if (!srm->imp()->createUDEV())
        goto fail;

    if (!srm->imp()->enumerateDevices())
        goto fail;

    if (!srm->imp()->initMonitor())
        goto fail;

    srm->imp()->updateBestConfiguration();

    return srm;

    fail:
        delete srm;
    return nullptr;
}

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

int SRMCore::monitorFd() const
{
    return imp()->monitorFd.fd;
}

int SRMCore::processMonitor(int msTimeout)
{
    int ret = poll(&imp()->monitorFd, 1, msTimeout);

    if (ret > 0 && imp()->monitorFd.revents & POLLIN)
    {
        udev_device *dev = udev_monitor_receive_device(imp()->monitor);

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
                        SRMLog::error("Failed to open device %s.", devnode);
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
                                    //printf("Connector %d connected\n", conn->connector_id);

                                    for (SRMListener *listener : imp()->connectorPluggedListeners)
                                    {
                                        void (*callback)(SRMListener *, SRMConnector *) = (void(*)(SRMListener *, SRMConnector *))listener->callback();
                                        callback(listener, nullptr);
                                    }
                                }
                                else
                                {
                                    printf("Connector %d disconnected\n", conn->connector_id);
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
                    for (SRMListener *listener : imp()->deviceCreatedListeners)
                    {
                        void (*callback)(SRMListener *, SRMDevice *) = (void(*)(SRMListener *, SRMDevice *))listener->callback();
                        callback(listener, nullptr);
                    }
                }

                // GPU removed
                else if (strcmp(action, "remove") == 0)
                {
                    printf("DRM device removed: %s\n", devnode);
                }
            }

            unref:
            udev_device_unref(dev);
        }
    }

    return ret;
}

SRMDevice *SRMCore::allocatorDevice() const
{
    return imp()->allocatorDevice;
}

std::list<SRMDevice *> &SRMCore::devices() const
{
    return imp()->devices;
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
