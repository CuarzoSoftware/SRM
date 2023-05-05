#ifndef SRMCORE_H
#define SRMCORE_H

#include <SRMTypes.h>

SRMCore *srmCoreCreate(SRMInterface *interface, void *userData);
SRMList *srmCoreGetDevices(SRMCore *core);
SRMDevice *srmCoreGetAllocatorDevice(SRMCore *core);
Int32 srmCoreGetMonitorFD(SRMCore *core);
Int32 srmCoreProccessMonitor(SRMCore *core, Int32 msTimeout);
SRMListener *srmCoreAddDeviceCreatedEventListener(SRMCore *core, void(*callback)(SRMListener*, SRMDevice*), void *userData);
SRMListener *srmCoreAddDeviceRemovedEventListener(SRMCore *core, void(*callback)(SRMListener*, SRMDevice*), void *userData);
SRMListener *srmCoreAddConnectorPluggedEventListener(SRMCore *core, void(*callback)(SRMListener*, SRMConnector*), void *userData);
SRMListener *srmCoreAddConnectorUnpluggedEventListener(SRMCore *core, void(*callback)(SRMListener*, SRMConnector*), void *userData);
void srmCoreDestroy(SRMCore *core);

/*
SRMCore *createSRM(SRMInterface *interface, void *userData);

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
*/

#endif // SRMCORE_H
