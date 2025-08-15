#include <CZ/SRM/SRMConnector.h>
#include <CZ/SRM/SRMCore.h>
#include <CZ/SRM/SRMLog.h>
#include <CZ/SRM/SRMDevice.h>
#include <CZ/Ream/RCore.h>
#include <CZ/Ream/DRM/RDRMPlatformHandle.h>
#include <CZ/Utils/CZVectorUtils.h>
#include <cstring>
#include <libudev.h>
#include <sys/epoll.h>
#include <sys/poll.h>

using namespace CZ;

std::shared_ptr<SRMCore> SRMCore::Make(const SRMInterface *iface, void *data) noexcept
{
    if (!iface || !iface->closeRestricted || !iface->openRestricted)
    {
        SRMLog(CZFatal, CZLN, "Invalid interface.");
        return {};
    }

    // Default env values
    setenv("SRM_FORCE_LEGACY_API",              "0", 0);
    setenv("SRM_FORCE_LEGACY_CURSOR",           "0", 0);
    setenv("SRM_FORCE_GL_ALLOCATION",           "0", 0);
    setenv("SRM_RENDER_MODE_SELF_FB_COUNT",     "2", 0);
    setenv("SRM_RENDER_MODE_PRIME_FB_COUNT",    "2", 0);
    setenv("SRM_RENDER_MODE_DUMB_FB_COUNT",     "2", 0);
    setenv("SRM_RENDER_MODE_CPU_FB_COUNT",      "2", 0);
    setenv("SRM_ENABLE_WRITEBACK_CONNECTORS",   "0", 0);
    setenv("SRM_DISABLE_CUSTOM_SCANOUT",        "0", 0);
    setenv("SRM_DISABLE_CURSOR",                "0", 0);
    setenv("SRM_NVIDIA_CURSOR",                 "1", 0);

    SRMLog(CZInfo, "SRM version {}.{}.{}-{}.",
             SRM_VERSION_MAJOR,
             SRM_VERSION_MINOR,
             SRM_VERSION_PATCH,
             SRM_VERSION_BUILD);

    if (RCore::Get())
    {
        SRMLog(CZFatal, CZLN, "The RCore instance must be created by SRM.");
        return {};
    }

    auto core { std::shared_ptr<SRMCore>(new SRMCore(iface, data)) };

    if (core->init())
        return core;

    return {};
}

SRMCore::~SRMCore() noexcept
{
    for (auto *dev : m_devices)
        for (auto *conn : dev->connectors())
            conn->uninitialize();

    CZVectorUtils::DeleteAndPopBackAll(m_devices);

    if (m_monitor)
    {
        udev_monitor_unref(m_monitor);
        m_monitor = nullptr;
    }

    if (m_udev)
    {
        udev_unref(m_udev);
        m_udev = nullptr;
    }

    if (m_ream)
        m_ream->clearGarbage();
}

bool SRMCore::init() noexcept
{
    const char *env { getenv("SRM_DISABLE_CUSTOM_SCANOUT") };
    m_disableScanout = env && atoi(env) == 1;
    SRMLog(CZInfo, "Direct Scanout Enabled: {}.", !m_disableScanout);

    env = getenv("SRM_DISABLE_CURSOR");
    m_disableCursor =env && atoi(env) == 1;
    SRMLog(CZInfo, "Cursor Planes Enabled: {}.", !m_disableCursor);

    env = getenv("SRM_FORCE_LEGACY_CURSOR");
    m_forceLegacyCursor = env && atoi(env) == 1;
    SRMLog(CZInfo, "Forcing Legacy Cursor IOCTLs: {}.", m_forceLegacyCursor);

    const bool ret {
        initUdev() &&
        initDevices() &&
        initMonitor() &&
        initReam()
    };

    return ret;
}

bool SRMCore::initUdev() noexcept
{
    m_udev = udev_new();

    if(!m_udev)
    {
        SRMLog(CZFatal, CZLN, "Failed to create udev context.");
        return false;
    }

    return true;
}

bool SRMCore::initDevices() noexcept
{
    udev_enumerate *enumerate;
    udev_list_entry *devices, *list;
    udev_device *dev;
    udev_device *pci;
    const char *path;
    const char *bootVGA;
    SRMDevice *device;

    enumerate = udev_enumerate_new(m_udev);

    if (!enumerate)
    {
        SRMLog(CZFatal, CZLN, "Failed to create udev_enumerate.");
        return false;
    }

    udev_enumerate_add_match_is_initialized(enumerate);
    udev_enumerate_add_match_sysname(enumerate, "card[0-9]*");
    udev_enumerate_add_match_property(enumerate, "DEVTYPE", "drm_minor");
    udev_enumerate_scan_devices(enumerate);
    devices = udev_enumerate_get_list_entry(enumerate);

    udev_list_entry_foreach(list, devices)
    {
        path = udev_list_entry_get_name(list);
        dev = udev_device_new_from_syspath(m_udev, path);
        pci = udev_device_get_parent_with_subsystem_devtype(dev, "pci", NULL);
        bootVGA = nullptr;

        if (pci)
            bootVGA = udev_device_get_sysattr_value(pci, "boot_vga");

        device = SRMDevice::Make(this, udev_device_get_devnode(dev), bootVGA && strcmp(bootVGA, "1") == 0);

        if (device)
            m_devices.emplace_back(device);

        udev_device_unref(dev);
    }

    udev_enumerate_unref(enumerate);
    return !m_devices.empty();
}

bool SRMCore::initMonitor() noexcept
{
    m_monitor = udev_monitor_new_from_netlink(m_udev, "udev");

    if (!m_monitor)
    {
        SRMLog(CZFatal, CZLN, "Failed to create udev_monitor.");
        return false;
    }

    if (udev_monitor_filter_add_match_subsystem_devtype(m_monitor, "drm", "drm_minor") < 0)
    {
        SRMLog(CZFatal, CZLN, "Failed to add udev monitor filter.");
        return false;
    }

    if (udev_monitor_enable_receiving(m_monitor) < 0)
    {
        SRMLog(CZFatal, CZLN, "Failed to enable udev monitor receiving.");
        return false;
    }

    if (udev_monitor_get_fd(m_monitor) < 0)
    {
        SRMLog(CZFatal, CZLN, "Failed to get udev monitor fd.");
        return false;
    }

    return true;
}

bool SRMCore::initReam() noexcept
{
    std::unordered_set<RDRMFdHandle> set;

    for (auto *dev : devices())
        set.emplace(RDRMFdHandle{ .fd = dev->fd(), .userData = dev });

    auto platformHandle { RDRMPlatformHandle::Make(set) };

    if (!platformHandle)
    {
        SRMLog(CZFatal, CZLN, "Failed to create RDRMPlatformHandle.");
        return false;
    }

    m_ream = RCore::Make(RCore::Options { .graphicsAPI = RGraphicsAPI::Auto, .platformHandle = platformHandle });

    if (!m_ream)
    {
        SRMLog(CZFatal, CZLN, "Failed to create RCore.");
        return false;
    }

    for (auto *dev : devices())
    {
        for (auto *reamDev : m_ream->devices())
        {
            if (dev == reamDev->drmUserData())
            {
                dev->m_reamDevice = reamDev;
                reamDev->m_srmDevice = dev;
                break;
            }
        }
    }

    SRMDevice *bestDev { nullptr };

    for (auto *dev : devices())
    {
        if (dev->isBootVGA())
            bestDev = dev;
    }

    if (bestDev)
        m_ream->overrideMainDevice(bestDev->reamDevice());

    return true;
}

void SRMCore::unplugAllConnectors() noexcept
{
    for (auto *dev : m_devices)
    {
        for (auto *conn : dev->connectors())
        {
            if (!conn->isConnected() || !conn->m_rend)
                continue;

            conn->m_rend->isDead = true;
        }
    }

    for (auto *dev : m_devices)
    {
        for (auto *conn : dev->connectors())
        {
            if (!conn->isConnected())
                continue;

            dev->log(CZInfo, "SRMConnector ({}) {}, {}, {} unplugged",
                conn->id(),
                conn->name().c_str(),
                conn->model().c_str(),
                conn->make().c_str());

            onConnectorUnplugged.notify(conn);

            conn->uninitialize();
            conn->m_isConnected = false;
        }
    }
}

bool SRMCore::isRenderThread(std::thread::id threadId) noexcept
{
    for (auto *dev : m_devices)
        for (auto *conn : dev->connectors())
            if (conn->m_rend && conn->m_rend->threadId == threadId)
                return true;
    return false;
}

bool SRMCore::suspend() noexcept
{
    if (isSuspended())
    {
        SRMLog(CZDebug, CZLN, "Already suspended");
        return true;
    }

    if (isRenderThread(std::this_thread::get_id()))
    {
        SRMLog(CZError, CZLN, "Calling suspend from a render thread is not allowed");
        return false;
    }

    unplugAllConnectors();
    m_isSuspended = true;
    return true;
}

bool SRMCore::resume() noexcept
{
    if (!isSuspended())
    {
        SRMLog(CZDebug, CZLN, "Already resumed");
        return true;
    }

    if (isRenderThread(std::this_thread::get_id()))
    {
        SRMLog(CZError, CZLN, "Calling resume from a render thread is not allowed");
        return false;
    }

    m_isSuspended = false;

    for (auto *dev : m_devices)
        dev->dispatchHotplugEvents();

    return true;
}

int SRMCore::fd() const noexcept
{
    return m_monitor ? udev_monitor_get_fd(m_monitor) : -1;
}

int SRMCore::dispatch(int timeoutMs) noexcept
{
    if (!isSuspended())
        for (auto *dev : devices())
            if (dev->m_rescanConnectors)
                dev->dispatchHotplugEvents();

    pollfd fds {};
    fds.events = POLLIN;
    fds.fd = fd();
    const int ret { poll(&fds, 1, timeoutMs) };

    if (!(ret > 0 && fds.revents & POLLIN))
        return ret;

    udev_device *dev { udev_monitor_receive_device(m_monitor) };

    if (!dev)
        return ret;

    const char *action { udev_device_get_action(dev) };
    const char *devnode { udev_device_get_devnode(dev) };

    // Events must be ignored until resumed
    if (isSuspended())
        goto unref;

    if (devnode && strncmp(devnode, "/dev/dri/card", 13) == 0)
    {
        SRMDevice *device {};

        for (auto *dev : devices())
        {
            if (dev->nodePath() == devnode)
            {
                device = dev;
                break;
            }
        }

        if (!device)
        {
            /* TODO: Handle GPU hotplug */
            goto unref;
        }

        // Possible connector hotplug event
        if (strcmp(action, "change") == 0)
        {
            device->dispatchHotplugEvents();
        }

        // GPU added
        else if (strcmp(action, "add") == 0)
        {
            /* TODO: Support GPU hotplug */
        }

        // GPU removed
        else if (strcmp(action, "remove") == 0)
        {
            /* TODO: Support GPU hotplug */
        }
    }

unref:
    udev_device_unref(dev);
    return ret;
}

