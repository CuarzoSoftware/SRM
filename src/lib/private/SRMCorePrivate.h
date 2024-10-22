#ifndef SRMCOREPRIVATE_H
#define SRMCOREPRIVATE_H

#include <SRMCore.h>
#include <SRMEGL.h>

#include <libudev.h>
#include <sys/poll.h>

#ifdef __cplusplus
extern "C" {
#endif

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

    pthread_t mainThread;

    // Env options
    UInt8 customBufferScanoutIsDisabled;
    UInt8 forceLegacyCursor;
    UInt8 disableCursorPlanes;
};

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
