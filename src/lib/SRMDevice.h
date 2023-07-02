#ifndef SRMDEVICE_H
#define SRMDEVICE_H

#include "SRMTypes.h"

#include <EGL/egl.h>

#ifdef __cplusplus
extern "C" {
#endif

SRMCore *srmDeviceGetCore(SRMDevice *device);
const char *srmDeviceGetName(SRMDevice *device);
Int32 srmDeviceGetFD(SRMDevice *device);

// Client caps
UInt8 srmDeviceGetClientCapStereo3D(SRMDevice *device);
UInt8 srmDeviceGetClientCapUniversalPlanes(SRMDevice *device);
UInt8 srmDeviceGetClientCapAtomic(SRMDevice *device);
UInt8 srmDeviceGetClientCapAspectRatio(SRMDevice *device);
UInt8 srmDeviceGetClientCapWritebackConnectors(SRMDevice *device);

// Caps
UInt8 srmDeviceGetCapDumbBuffer(SRMDevice *device);
UInt8 srmDeviceGetCapPrimeImport(SRMDevice *device);
UInt8 srmDeviceGetCapPrimeExport(SRMDevice *device);
UInt8 srmDeviceGetCapAddFb2Modifiers(SRMDevice *device);
UInt8 srmDeviceGetCapAsyncPageFlip(SRMDevice *device);

// Enables or disables the GPU
UInt8 srmDeviceSetEnabled(SRMDevice *device, UInt8 enabled);
UInt8 srmDeviceIsEnabled(SRMDevice *device);
UInt8 srmDeviceIsRenderer(SRMDevice *device);
SRMDevice *srmDeviceGetRendererDevice(SRMDevice *device);
SRM_RENDER_MODE srmDeviceGetRenderMode(SRMDevice *device);

// Resources
SRMList *srmDeviceGetCrtcs(SRMDevice *device);
SRMList *srmDeviceGetEncoders(SRMDevice *device);
SRMList *srmDeviceGetPlanes(SRMDevice *device);
SRMList *srmDeviceGetConnectors(SRMDevice *device);

SRMList *srmDeviceGetDMATextureFormats(SRMDevice *device);
SRMList *srmDeviceGetDMARenderFormats(SRMDevice *device);

EGLDisplay *srmDeviceGetEGLDisplay(SRMDevice *device);
EGLContext *srmDeviceGetEGLContext(SRMDevice *device);

#ifdef __cplusplus
}
#endif

#endif // SRMDEVICE_H
