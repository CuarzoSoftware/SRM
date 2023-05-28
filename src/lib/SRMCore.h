#ifndef SRMCORE_H
#define SRMCORE_H

#include "SRMTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

void *srmCoreGetUserData(SRMCore *core);
SRMCore *srmCoreCreate(SRMInterface *interface, void *userData);
SRMList *srmCoreGetDevices(SRMCore *core);
SRMDevice *srmCoreGetAllocatorDevice(SRMCore *core);
Int32 srmCoreGetMonitorFD(SRMCore *core);
Int32 srmCoreProccessMonitor(SRMCore *core, Int32 msTimeout);
SRMListener *srmCoreAddDeviceCreatedEventListener(SRMCore *core, void(*callback)(SRMListener*, SRMDevice*), void *userData);
SRMListener *srmCoreAddDeviceRemovedEventListener(SRMCore *core, void(*callback)(SRMListener*, SRMDevice*), void *userData);
SRMListener *srmCoreAddConnectorPluggedEventListener(SRMCore *core, void(*callback)(SRMListener*, SRMConnector*), void *userData);
SRMListener *srmCoreAddConnectorUnpluggedEventListener(SRMCore *core, void(*callback)(SRMListener*, SRMConnector*), void *userData);

const SRMEGLCoreExtensions *srmCoreGetEGLExtensions(SRMCore *core);
const SRMEGLCoreFunctions *srmCoreGetEGLFunctions(SRMCore *core);

/* DMA formats supported by all rendering GPUs */
SRMList *srmCoreGetSharedDMATextureFormats(SRMCore *core);

void srmCoreDestroy(SRMCore *core);

#ifdef __cplusplus
}
#endif

#endif // SRMCORE_H
