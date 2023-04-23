#ifndef SRMCORE_H
#define SRMCORE_H

#include <SRMNamespaces.h>

class SRM::SRMCore
{
public:
    static SRMCore *createSRM(SRMInterface *interface, void *userdata = nullptr);
    ~SRMCore();

    // List of GPUs
    std::list<SRMDevice*>&devices() const;

    // Register to udev monitor events (GPU and Connectors hotplug events)
    SRMListener *addDeviceCreatedListener(void(*callback)(SRMListener*, SRMDevice*), void *userdata = nullptr);
    SRMListener *addDeviceRemovedListener(void(*callback)(SRMListener*, SRMDevice*), void *userdata = nullptr);
    SRMListener *addConnectorPluggedListener(void(*callback)(SRMListener*, SRMConnector*), void *userdata = nullptr);
    SRMListener *addConnectorUnpluggedListener(void(*callback)(SRMListener*, SRMConnector*), void *userdata = nullptr);
    int monitorFd() const;
    int processMonitor(int msTimeout);

    SRMDevice *allocatorDevice() const;

    class SRMCorePrivate;
    SRMCorePrivate *imp() const;

private:
    SRMCore();
    SRMCorePrivate *m_imp = nullptr;
};

#endif // SRMCORE_H
