#ifndef SRMDEVICEPRIVATE_H
#define SRMDEVICEPRIVATE_H

#include <private/SRMBufferPrivate.h>
#include <SRMDevice.h>
#include <SRMEGL.h>
#include <gbm.h>
#include <EGL/egl.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum SRM_DEVICE_DRIVER_ENUM
{
    SRM_DEVICE_DRIVER_unknown = 0,
    SRM_DEVICE_DRIVER_i915 = 1,
    SRM_DEVICE_DRIVER_nouveau = 2,
    SRM_DEVICE_DRIVER_lima = 3,
    SRM_DEVICE_DRIVER_nvidia = 4
} SRM_DEVICE_DRIVER;

typedef struct SRMDeviceThreadContextStruct
{
    pthread_t thread;
    EGLContext context;
} SRMDeviceThreadContext;

struct SRMDeviceStruct
{
    SRMCore *core;
    SRM_DEVICE_DRIVER driver;
    SRM_BUFFER_WRITE_MODE cpuBufferWriteMode;
    SRM_RENDER_MODE renderMode;

    // All GPUs are enabled by default
    UInt8 enabled;
    UInt8 isBootVGA;

    // Renderer device could be itself or another if PIRME IMPORT not supported
    SRMDevice *rendererDevice;

    // Prevents multiple threads calling drmModeHandleEvent
    pthread_mutex_t pageFlipMutex;
    UInt8 pageFlipMutexInitialized;

    Int32 fd;

    SRMListItem *coreLink;

    struct gbm_device *gbm;
    EGLDeviceEXT eglDevice;
    EGLDisplay eglDisplay;
    EGLContext eglSharedContext;

    struct gbm_surface *gbmSurfaceTest;
    struct gbm_bo *gbmSurfaceTestBo;
    EGLConfig eglConfigTest;
    EGLSurface eglSurfaceTest;
    GLuint vertexShaderTest;
    GLuint fragmentShaderTest;
    GLuint programTest;
    GLuint textureUniformTest;

    EGLint eglSharedContextAttribs[7];
    SRMEGLDeviceExtensions eglExtensions;
    SRMEGLDeviceFunctions eglFunctions;
    SRMGLDeviceExtensions glExtensions;

    SRMList *dmaRenderFormats, // GL_TEXTURE_2D
        *dmaExternalFormats, // GL_TEXTURE_EXTERNAL_OES
        *dmaTextureFormats; // Render + External

    UInt8 clientCapStereo3D;
    UInt8 clientCapUniversalPlanes;
    UInt8 clientCapAtomic;
    UInt8 clientCapAspectRatio;
    UInt8 clientCapWritebackConnectors;

    UInt8 capDumbBuffer;
    UInt8 capPrimeImport;
    UInt8 capPrimeExport;
    UInt8 capAddFb2Modifiers;
    UInt8 capTimestampMonotonic;
    UInt8 capAsyncPageFlip;
    UInt8 capAtomicAsyncPageFlip;

    clockid_t clock;

    SRMList *contexts;
    SRMList *crtcs;
    SRMList *encoders;
    SRMList *planes;
    SRMList *connectors;

    UInt8 pendingUdevEvents;

    char shortName[8];
    char name[64];
};

SRMDevice *srmDeviceCreate(SRMCore *core, const char *name, UInt8 isBootVGA);
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
UInt8 srmDeviceCreateSharedContextForThread(SRMDevice *device);
void srmDeviceDestroyThreadSharedContext(SRMDevice *device, pthread_t thread);
void srmDeviceUninitializeEGLSharedContext(SRMDevice *device);

UInt8 srmDeviceUpdateGLExtensions(SRMDevice *device);

UInt8 srmDeviceInitializeTestGBMSurface(SRMDevice *device);
void srmDeviceUninitializeTestGBMSurface(SRMDevice *device);

UInt8 srmDeviceInitializeTestEGLSurface(SRMDevice *device);
void srmDeviceUninitializeTestEGLSurface(SRMDevice *device);

UInt8 srmDeviceInitializeTestShader(SRMDevice *device);
void srmDeviceUninitializeTestShader(SRMDevice *device);

UInt8 srmDeviceUpdateClientCaps(SRMDevice *device);
UInt8 srmDeviceUpdateCaps(SRMDevice *device);

UInt8 srmDeviceUpdateCrtcs(SRMDevice *device);
UInt8 srmDeviceUpdateEncoders(SRMDevice *device);
UInt8 srmDeviceUpdatePlanes(SRMDevice *device);
UInt8 srmDeviceUpdateConnectors(SRMDevice *device);

void srmDeviceTestCPUAllocationMode(SRMDevice *device);

UInt8 srmDeviceHandleHotpluggingEvent(SRMDevice *device);

#ifdef __cplusplus
}
#endif

#endif // SRMDEVICEPRIVATE_H
