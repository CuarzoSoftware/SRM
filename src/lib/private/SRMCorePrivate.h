#ifndef SRMCOREPRIVATE_H
#define SRMCOREPRIVATE_H

#include <SRMCore.h>
#include <SRMEGL.h>

#include <libudev.h>
#include <sys/poll.h>

struct SRMCoreStruct
{
    SRMInterface *interface;
    void *interfaceUserData;

    struct udev *udev;
    struct udev_monitor *monitor;
    struct pollfd monitorFd;

    SRMList *devices;
    SRMList *sharedDMATextureFormats;

    SRMList *deviceCreatedListeners;
    SRMList *deviceRemovedListeners;
    SRMList *connectorPluggedListeners;
    SRMList *connectorUnpluggedListeners;

    SRMDevice *allocatorDevice;

    SRMEGLCoreExtensions eglExtensions;
    SRMEGLCoreFunctions eglFunctions;
};

UInt8 srmCoreUpdateEGLExtensions(SRMCore *core);
UInt8 srmCoreUpdateEGLFunctions(SRMCore *core);
UInt8 srmCoreCreateUdev(SRMCore *core);
UInt8 srmCoreEnumerateDevices(SRMCore *core);
UInt8 srmCoreInitMonitor(SRMCore *core);
SRMDevice *srmCoreFindBestAllocatorDevice(SRMCore *core);
void srmCoreAssignRendererDevices(SRMCore *core);
UInt8 srmCoreUpdateBestConfiguration(SRMCore *core);

/* Intersects the compatible DMA formats of the allocator GPU
 * and all other rendering GPUs*/
void srmCoreUpdateSharedDMATextureFormats(SRMCore *core);

#endif // SRMCOREPRIVATE_H
