#ifndef SRMCOREPRIVATE_H
#define SRMCOREPRIVATE_H

#include <SRMCore.h>
#include <libudev.h>
#include <sys/poll.h>

class SRM::SRMCore::SRMCorePrivate
{

public:
    SRMCorePrivate(SRMCore *core);
    ~SRMCorePrivate();

    int createUDEV();
    int enumerateDevices();
    int initMonitor();

    SRMCore *core = nullptr;

    SRMInterface *interface;
    void *userdata;

    struct udev *udev = nullptr;
    udev_monitor *monitor = nullptr;
    pollfd monitorFd;

    std::list<SRMDevice*>devices;

    std::list<SRMListener*>deviceCreatedListeners;
    std::list<SRMListener*>deviceRemovedListeners;
    std::list<SRMListener*>connectorPluggedListeners;
    std::list<SRMListener*>connectorUnpluggedListeners;


    // Config
    void updateBestConfiguration();
    SRMDevice *findBestAllocatorDevice();
    SRMDevice *allocatorDevice = nullptr;
    void assignRendererDevices();

};

#endif // SRMCOREPRIVATE_H
