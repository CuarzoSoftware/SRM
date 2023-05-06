#include <private/SRMCorePrivate.h>
#include <private/SRMDevicePrivate.h>
#include <private/SRMConnectorPrivate.h>
#include <SRMLog.h>
#include <SRMList.h>

int srmCoreCreateUdev(SRMCore *core)
{
    core->udev = udev_new();

    if(!core->udev)
    {
        SRMFatal("Failed to create udev context.");
        return 0;
    }

    return 1;
}

int srmCoreEnumerateDevices(SRMCore *core)
{
    struct udev_enumerate *enumerate;
    struct udev_list_entry *devices, *dev_list_entry;
    struct udev_device *dev;

    enumerate = udev_enumerate_new(core->udev);

    if (!enumerate)
    {
        SRMFatal("Failed to create udev enumerate.");
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
        dev = udev_device_new_from_syspath(core->udev, path);

        SRMDevice *device = srmDeviceCreate(core, udev_device_get_devnode(dev));

        if (device)
        {
            device->coreLink = srmListAppendData(core->devices, device);

            // Update device connector names
            SRMListForeach (item, device->connectors)
            {
                SRMConnector *connector = srmListItemGetData(item);
                srmConnectorUpdateNames(connector);
            }
        }


        udev_device_unref(dev);
    }

    udev_enumerate_unref(enumerate);

    return 1;
}

int srmCoreInitMonitor(SRMCore *core)
{
    core->monitor = udev_monitor_new_from_netlink(core->udev, "udev");

    if (!core->monitor)
    {
        SRMFatal("Failed to create udev monitor.");
        return 0;
    }

    if (udev_monitor_filter_add_match_subsystem_devtype(core->monitor, "drm", "drm_minor") < 0)
    {
        SRMFatal("Failed to add udev monitor filter.");
        goto fail;
    }

    if (udev_monitor_enable_receiving(core->monitor) < 0)
    {
        SRMFatal("Failed to enable udev monitor receiving.");
        goto fail;
    }

    core->monitorFd.fd = udev_monitor_get_fd(core->monitor);

    if (core->monitorFd.fd < 0)
    {
        SRMFatal("Failed to get udev monitor fd.");
        goto fail;
    }

    core->monitorFd.events = POLLIN | POLLHUP;
    core->monitorFd.revents = 0;

    return 1;

    fail:
    udev_monitor_unref(core->monitor);
    core->monitor = NULL;
    return 0;

}

SRMDevice *srmCoreFindBestAllocatorDevice(SRMCore *core)
{
    SRMDevice *bestAllocatorDev = NULL;
    int bestScore = 0;

    SRMListForeach(item1, core->devices)
    {
        SRMDevice *allocDev = srmListItemGetData(item1);

        if (!srmDeviceIsEnabled(allocDev))
            continue;

        int currentScore = srmDeviceGetCapPrimeExport(allocDev) ? 100 : 10;

        SRMListForeach(item2, core->devices)
        {
            SRMDevice *otherDev = srmListItemGetData(item2);

            if (!srmDeviceIsEnabled(otherDev) || allocDev == otherDev)
                continue;

            // GPU can render
            if (srmDeviceGetCapPrimeExport(allocDev) && srmDeviceGetCapPrimeImport(otherDev))
            {
                currentScore += 100;
                continue;
            }
            // GPU can not render but glRead > dumbBuffer
            else if (srmDeviceGetCapDumbBuffer(otherDev))
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

void srmCoreAssignRendererDevices(SRMCore *core)
{
    int enabledDevicesN = 0;

    // Count enabled GPUs
    SRMListForeach(item, core->devices)
    {
        SRMDevice *dev = srmListItemGetData(item);
        enabledDevicesN += dev->enabled;
    }

    SRMListForeach(item, core->devices)
    {
        SRMDevice *dev = srmListItemGetData(item);

        if (!dev->enabled)
        {
            dev->rendererDevice = NULL;
            continue;
        }

        // If only one GPU or can import from allocator
        if (enabledDevicesN == 1 || (dev->capPrimeImport && core->allocatorDevice->capPrimeExport))
        {
            dev->rendererDevice = dev;
            continue;
        }

        int bestScore = 0;
        SRMDevice *bestRendererForDev = NULL;

        SRMListForeach(item, core->devices)
        {
            SRMDevice *rendererDevice = srmListItemGetData(item);

            if (!rendererDevice->enabled)
                continue;

            int currentScore = 0;

            if (dev == rendererDevice)
                currentScore += 10;

            if (!rendererDevice->capDumbBuffer)
                currentScore += 20;

            if (rendererDevice->capPrimeImport && core->allocatorDevice->capPrimeExport)
                currentScore += 100;

            if (rendererDevice == core->allocatorDevice)
                currentScore += 50;

            if (currentScore > bestScore)
            {
                bestRendererForDev = rendererDevice;
                bestScore = currentScore;
            }
        }

        dev->rendererDevice = bestRendererForDev;

    }
}

int srmCoreUpdateBestConfiguration(SRMCore *core)
{
    SRMDevice *bestAllocatorDevice = srmCoreFindBestAllocatorDevice(core);

    if (!bestAllocatorDevice)
    {
        SRMFatal("No allocator device found.");
        return 0;
    }

    eglMakeCurrent(bestAllocatorDevice->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, bestAllocatorDevice->eglSharedContext);

    /*
    if (allocatorDevice && allocatorDevice != bestAllocatorDevice)
    {
        // TODO may require update
    }
    */

    core->allocatorDevice = bestAllocatorDevice;

    srmCoreAssignRendererDevices(core);

    return 1;
}



