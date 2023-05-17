#ifndef SRMDEVICEPRIVATE_H
#define SRMDEVICEPRIVATE_H

#include "../SRMDevice.h"
#include "../SRMEGL.h"
#include <gbm.h>
#include <EGL/egl.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

struct SRMDeviceStruct
{
    SRMCore *core;

    // All GPUs are enabled by default
    UInt8 enabled;

    // Renderer device could be itself or another if PIRME IMPORT not supported
    SRMDevice *rendererDevice;

    // Prevent multiple threads calling drmModeHandleEvent
    pthread_mutex_t pageFlipMutex;

    Int32 fd;

    SRMListItem *coreLink;

    char *name;

    struct gbm_device *gbm;
    EGLDeviceEXT eglDevice;
    EGLDisplay eglDisplay;
    EGLContext eglSharedContext;
    EGLint eglSharedContextAttribs[7];
    SRMEGLDeviceExtensions eglExtensions;
    SRMEGLDeviceFunctions eglFunctions;
    SRMList *dmaRenderFormats, *dmaTextureFormats;

    UInt8 clientCapStereo3D;
    UInt8 clientCapUniversalPlanes;
    UInt8 clientCapAtomic;
    UInt8 clientCapAspectRatio;
    UInt8 clientCapWritebackConnectors;

    UInt8 capDumbBuffer;
    UInt8 capPrimeImport;
    UInt8 capPrimeExport;
    UInt8 capAddFb2Modifiers;

    SRMList *crtcs;
    SRMList *encoders;
    SRMList *planes;
    SRMList *connectors;

};

SRMDevice *srmDeviceCreate(SRMCore *core, const char *name);
void srmDeviceDestroy(SRMDevice *device);
UInt8 srmDeviceInitializeGBM(SRMDevice *device);

UInt8 srmDeviceInitializeEGL(SRMDevice *device);
UInt8 srmDeviceUpdateEGLExtensions(SRMDevice *device);
UInt8 srmDeviceUpdateEGLFunctions(SRMDevice *device);
UInt8 srmDeviceUpdateDMAFormats(SRMDevice *device);
void srmDeviceDestroyDMAFormats(SRMDevice *device);
UInt8 srmDeviceInitializeEGLSharedContext(SRMDevice *device);

UInt8 srmDeviceUpdateClientCaps(SRMDevice *device);
UInt8 srmDeviceUpdateCaps(SRMDevice *device);

UInt8 srmDeviceUpdateCrtcs(SRMDevice *device);
UInt8 srmDeviceUpdateEncoders(SRMDevice *device);
UInt8 srmDeviceUpdatePlanes(SRMDevice *device);
UInt8 srmDeviceUpdateConnectors(SRMDevice *device);

#ifdef __cplusplus
}
#endif

#endif // SRMDEVICEPRIVATE_H
