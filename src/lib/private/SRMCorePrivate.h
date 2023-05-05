#ifndef SRMCOREPRIVATE_H
#define SRMCOREPRIVATE_H

#include <SRMCore.h>
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

    SRMList *deviceCreatedListeners;
    SRMList *deviceRemovedListeners;
    SRMList *connectorPluggedListeners;
    SRMList *connectorUnpluggedListeners;

    SRMDevice *allocatorDevice;
};

int srmCoreCreateUdev(SRMCore *core);
int srmCoreEnumerateDevices(SRMCore *core);
int srmCoreInitMonitor(SRMCore *core);
SRMDevice *srmCoreFindBestAllocatorDevice(SRMCore *core);
void srmCoreAssignRendererDevices(SRMCore *core);
int srmCoreUpdateBestConfiguration(SRMCore *core);

/*

// Config
void updateBestConfiguration();
SRMDevice *findBestAllocatorDevice();
void assignRendererDevices();
*/


#endif // SRMCOREPRIVATE_H
