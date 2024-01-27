#ifndef SRMDEVICEPRIVATE_H
#define SRMDEVICEPRIVATE_H

#include <SRMDevice.h>
#include <SRMEGL.h>
#include <gbm.h>
#include <EGL/egl.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

enum SRM_DEVICE_DRIVER
{
    SRM_DEVICE_DRIVER_unknown = 0,
    SRM_DEVICE_DRIVER_i915 = 1,
    SRM_DEVICE_DRIVER_nouveau = 2,
    SRM_DEVICE_DRIVER_lima = 3
};

struct SRMDeviceStruct
{
    SRMCore *core;

    enum SRM_DEVICE_DRIVER driver;

    // All GPUs are enabled by default
    UInt8 enabled;

    // Renderer device could be itself or another if PIRME IMPORT not supported
    SRMDevice *rendererDevice;

    // Prevent multiple threads calling drmModeHandleEvent
    pthread_mutex_t pageFlipMutex;
    UInt8 pageFlipMutexInitialized;

    Int32 fd;

    SRMListItem *coreLink;

    char name[64];

    struct gbm_device *gbm;
    EGLDeviceEXT eglDevice;
    EGLDisplay eglDisplay;
    EGLContext eglSharedContext;

    /* EGLSurface eglBaseSurface;
    struct gbm_surface *baseSurface;*/

    EGLContext eglDeallocatorContext;
    EGLint eglSharedContextAttribs[7];
    SRMEGLDeviceExtensions eglExtensions;
    SRMEGLDeviceFunctions eglFunctions;
    SRMGLDeviceExtensions glExtensions;
    SRMList *dmaRenderFormats, *dmaExternalFormats, *dmaTextureFormats;

    UInt8 clientCapStereo3D;
    UInt8 clientCapUniversalPlanes;
    UInt8 clientCapAtomic;
    UInt8 clientCapAspectRatio;
    UInt8 clientCapWritebackConnectors;

    UInt8 capDumbBuffer;
    UInt8 capPrimeImport;
    UInt8 capPrimeExport;
    UInt8 capAddFb2Modifiers;
    UInt8 capAsyncPageFlip;
    UInt8 capTimestampMonotonic;

    clockid_t clock;

    SRMList *crtcs;
    SRMList *encoders;
    SRMList *planes;
    SRMList *connectors;

    UInt8 pendingUdevEvents;
};

SRMDevice *srmDeviceCreate(SRMCore *core, const char *name);
void srmDeviceDestroy(SRMDevice *device);

UInt8 srmDeviceInitializeGBM(SRMDevice *device);
void srmDeviceUninitializeGBM(SRMDevice *device);

UInt8 srmDeviceInitializeEGL(SRMDevice *device);
void srmDeviceUninitializeEGL(SRMDevice *device);

UInt8 srmDeviceUpdateEGLExtensions(SRMDevice *device);
UInt8 srmDeviceUpdateEGLFunctions(SRMDevice *device);

UInt8 srmDeviceUpdateDMAFormats(SRMDevice *device);
void srmDeviceDestroyDMAFormats(SRMDevice *device);

UInt8 srmDeviceInitializeEGLSharedContext(SRMDevice *device);
void srmDeviceUninitializeEGLSharedContext(SRMDevice *device);

UInt8 srmDeviceUpdateGLExtensions(SRMDevice *device);

UInt8 srmDeviceInitEGLDeallocatorContext(SRMDevice *device);
void srmDeviceUninitEGLDeallocatorContext(SRMDevice *device);

UInt8 srmDeviceUpdateClientCaps(SRMDevice *device);
UInt8 srmDeviceUpdateCaps(SRMDevice *device);

UInt8 srmDeviceUpdateCrtcs(SRMDevice *device);
UInt8 srmDeviceUpdateEncoders(SRMDevice *device);
UInt8 srmDeviceUpdatePlanes(SRMDevice *device);
UInt8 srmDeviceUpdateConnectors(SRMDevice *device);

UInt8 srmDeviceHandleHotpluggingEvent(SRMDevice *device);

#ifdef __cplusplus
}
#endif

#endif // SRMDEVICEPRIVATE_H
