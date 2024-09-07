#ifndef SRMCOREPRIVATE_H
#define SRMCOREPRIVATE_H

#include <SRMCore.h>
#include <SRMEGL.h>

#include <libudev.h>
#include <sys/poll.h>

#ifdef __cplusplus
extern "C" {
#endif

enum SRM_DEALLOCATOR_MSG
{
    SRM_DEALLOCATOR_MSG_CREATE_CONTEXT,
    SRM_DEALLOCATOR_MSG_DESTROY_CONTEXT,
    SRM_DEALLOCATOR_MSG_DESTROY_BUFFER,
    SRM_DEALLOCATOR_MSG_STOP_THREAD
};

struct SRMDeallocatorThreadMessage
{
    enum SRM_DEALLOCATOR_MSG msg;
    SRMDevice *device;
    GLuint textureID;
    EGLImage image;
};

struct SRMCoreStruct
{
    SRMVersion version;
    SRMInterface *interface;
    void *interfaceUserData;

    UInt8 isSuspended;
    struct udev *udev;
    struct udev_monitor *monitor;
    struct pollfd monitorFd;
    Int32 udevMonitorFd;

    SRMList *devices;
    SRMList *sharedDMATextureFormats;

    SRMList *deviceCreatedListeners;
    SRMList *deviceRemovedListeners;
    SRMList *connectorPluggedListeners;
    SRMList *connectorUnpluggedListeners;

    SRMDevice *allocatorDevice;

    SRMEGLCoreExtensions eglExtensions;
    SRMEGLCoreFunctions eglFunctions;

    pthread_t deallocatorThread;
    SRMList *deallocatorMessages;
    Int8 deallocatorState;
    UInt8 deallocatorRunning;
    pthread_cond_t deallocatorCond;
    pthread_mutex_t deallocatorMutex;

    // Env options
    UInt8 customBufferScanoutIsDisabled;
    UInt8 forceLegacyCursor;
    UInt8 disableCursorPlanes;
};

UInt8 srmCoreInitDeallocator(SRMCore *core);
void srmCoreUnitDeallocator(SRMCore *core);
void srmCoreSendDeallocatorMessage(SRMCore *core,
                                   enum SRM_DEALLOCATOR_MSG msg,
                                   SRMDevice *device,
                                   GLuint textureID,
                                   EGLImage image);

UInt8 srmCoreUpdateEGLExtensions(SRMCore *core);
UInt8 srmCoreUpdateEGLFunctions(SRMCore *core);
UInt8 srmCoreCreateUdev(SRMCore *core);
UInt8 srmCoreEnumerateDevices(SRMCore *core);
UInt8 srmCoreInitMonitor(SRMCore *core);
SRMDevice *srmCoreFindBestAllocatorDevice(SRMCore *core);
void srmCoreAssignRendererDevices(SRMCore *core);
void srmCoreAssignRenderingModes(SRMCore *core);
UInt8 srmCoreUpdateBestConfiguration(SRMCore *core);

/* Intersects the compatible DMA formats of the allocator GPU
 * and all other rendering GPUs*/
void srmCoreUpdateSharedDMATextureFormats(SRMCore *core);

#ifdef __cplusplus
}
#endif

#endif // SRMCOREPRIVATE_H
