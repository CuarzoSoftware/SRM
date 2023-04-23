#include <private/SRMCorePrivate.h>
#include <private/SRMDevicePrivate.h>
#include <stdio.h>

using namespace SRM;

SRMCore::SRMCorePrivate::SRMCorePrivate(SRMCore *core)
{
    this->core = core;
}

SRMCore::SRMCorePrivate::~SRMCorePrivate()
{
    if (monitor)
        udev_monitor_unref(monitor);

    if (udev)
        udev_unref(udev);
}

int SRMCore::SRMCorePrivate::createUDEV()
{
    udev = udev_new();

    if(!udev)
    {
        fprintf(stderr, "SDR Error: Failed to create udev context.\n");
        return 0;
    }

    return 1;
}

// Find DRM device nodes (/dev/dri/cardN) on init using udev
int SRMCore::SRMCorePrivate::enumerateDevices()
{
    #ifdef SRM_CONFIG_TESTS
        for (int i = 0; i < 2; i++)
        {
            char node[32];
            sprintf(node, "/dev/dri/card%d", i);
            SRMDevice *device = SRMDevice::createDevice(core, node);
            this->devices.push_back(device);
            device->imp()->coreLink = std::prev(this->devices.end());
        }

        return 1;
    #endif

    udev_enumerate *enumerate;
    udev_list_entry *devices, *dev_list_entry;
    udev_device *dev;

    enumerate = udev_enumerate_new(udev);

    if (!enumerate)
    {
        fprintf(stderr, "SDR Error: Failed to create udev enumerate\n");
        udev_unref(udev);
        return 0;
    }

    udev_enumerate_add_match_is_initialized(enumerate);
    udev_enumerate_add_match_sysname(enumerate, "card[0-9]*");
    udev_enumerate_add_match_property(enumerate, "DEVTYPE", "drm_minor");
    udev_enumerate_scan_devices(enumerate);
    devices = udev_enumerate_get_list_entry(enumerate);

    udev_list_entry_foreach(dev_list_entry, devices)
    {
        const char *path;

        path = udev_list_entry_get_name(dev_list_entry);
        dev = udev_device_new_from_syspath(udev, path);

        SRMDevice *device = SRMDevice::createDevice(core, udev_device_get_devnode(dev));

        if (device)
        {
            this->devices.push_back(device);
            device->imp()->coreLink = std::prev(this->devices.end());
        }

        udev_device_unref(dev);
    }

    udev_enumerate_unref(enumerate);

    return 1;
}

// Inits monitor to listen to GPUs or Connectors hotplugs (creates a pollable fd)
int SRMCore::SRMCorePrivate::initMonitor()
{
    monitor = udev_monitor_new_from_netlink(udev, "udev");

    if (!monitor)
    {
        fprintf(stderr, "SRM Error: Failed to create udev monitor.\n");
        return 0;
    }

    if (udev_monitor_filter_add_match_subsystem_devtype(monitor, "drm", "drm_minor") < 0)
    {
        fprintf(stderr, "SRM Error: Failed to add udev monitor filter.\n");
        udev_monitor_unref(monitor);
        return 0;
    }

    if (udev_monitor_enable_receiving(monitor) < 0)
    {
        fprintf(stderr, "SRM Error: Failed to enable udev monitor receiving.\n");
        udev_monitor_unref(monitor);
        return 0;
    }

    monitorFd.fd = udev_monitor_get_fd(monitor);

    if (monitorFd.fd < 0)
    {
        fprintf(stderr, "SRM Error: Failed to get udev monitor fd.\n");
        udev_monitor_unref(monitor);
        return 0;
    }

    monitorFd.events = POLLIN | POLLHUP;
    monitorFd.revents = 0;

    return 1;
}

void SRMCore::SRMCorePrivate::updateBestConfiguration()
{
    SRMDevice *bestAllocatorDevice = findBestAllocatorDevice();

    if (!bestAllocatorDevice)
    {
        fprintf(stderr, "No allocator device found.\n");
        return;
    }

    if (allocatorDevice && allocatorDevice != bestAllocatorDevice)
    {
        // TODO may require update
    }

    allocatorDevice = bestAllocatorDevice;

    assignRendererDevices();
}

SRMDevice *SRMCore::SRMCorePrivate::findBestAllocatorDevice()
{
    SRMDevice *bestAllocatorDev = nullptr;
    int bestScore = 0;

    for (SRMDevice *allocDev : devices)
    {
        if (!allocDev->enabled())
            continue;

        int currentScore = allocDev->capPrimeExport() ? 100 : 10;

        for (SRMDevice *otherDev : devices)
        {
            if (!otherDev->enabled() || allocDev == otherDev)
                continue;

            // GPU can render
            if (allocDev->capPrimeExport() && otherDev->capPrimeImport())
            {
                currentScore += 100;
                continue;
            }
            // GPU can not render but glRead > dumbBuffer
            else if (otherDev->capDumbBuffer())
            {
                currentScore += 20;
                continue;
            }
            // GPU can not render glRead > CPU > glTex2D > render
            else
            {
                currentScore += 10;
                continue;
            }

        }

        if (currentScore > bestScore)
        {
            bestScore = currentScore;
            bestAllocatorDev = allocDev;
        }

    }

    return bestAllocatorDev;
}

void SRMCore::SRMCorePrivate::assignRendererDevices()
{
    int enabledDevicesN = 0;

    // Count enabled GPUs
    for (SRMDevice *dev : devices)
        enabledDevicesN += (int)dev->enabled();

    for (SRMDevice *dev : devices)
    {
        if (!dev->enabled())
        {
            dev->imp()->rendererDevice = nullptr;
            continue;
        }

        // If only one GPU or can import from allocator
        if (enabledDevicesN == 1 || (dev->capPrimeImport() && allocatorDevice->capPrimeExport()))
        {
            dev->imp()->rendererDevice = dev;
            continue;
        }

        int bestScore = 0;
        SRMDevice *bestRendererForDev = nullptr;

        for (SRMDevice *rendererDevice : devices)
        {
            if (!rendererDevice->enabled())
                continue;

            int currentScore = 0;

            if (dev == rendererDevice)
                currentScore += 10;

            if (!rendererDevice->capDumbBuffer())
                currentScore += 20;

            if (rendererDevice->capPrimeImport() && allocatorDevice->capPrimeExport())
                currentScore += 100;

            if (rendererDevice == allocatorDevice)
                currentScore += 50;

            if (currentScore > bestScore)
            {
                bestRendererForDev = rendererDevice;
                bestScore = currentScore;
            }
        }

        dev->imp()->rendererDevice = bestRendererForDev;

    }
}
