#include <CZ/SRM/SRMCrtc.h>
#include <CZ/SRM/SRMPlane.h>
#include <CZ/SRM/SRMEncoder.h>
#include <CZ/SRM/SRMLog.h>
#include <CZ/SRM/SRMDevice.h>
#include <CZ/SRM/SRMCore.h>
#include <CZ/SRM/SRMConnector.h>

#include <CZ/Core/Utils/CZStringUtils.h>
#include <CZ/Core/Utils/CZVectorUtils.h>

#include <cstring>
#include <fcntl.h>
#include <memory>
#include <xf86drm.h>
#include <xf86drmMode.h>

using namespace CZ;

static bool DeviceInBlacklist(const char *nodePath)
{
    const char *blacklist { getenv("CZ_SRM_DEVICE_BLACKLIST") };

    if (!blacklist)
        return false;

    const size_t extlen { strlen(nodePath) };
    const char *end { blacklist + strlen(blacklist) };

    while (blacklist < end)
    {
        if (*blacklist == ':')
        {
            blacklist++;
            continue;
        }

        const size_t n { strcspn(blacklist, ":") };

        if (n == extlen && strncmp(nodePath, blacklist, n) == 0)
            return true;

        blacklist += n;
    }

    return false;
}

SRMDevice *SRMDevice::Make(SRMCore *core, const char *nodePath, bool isBootVGA) noexcept
{
    if (DeviceInBlacklist(nodePath))
    {
        SRMLog(CZWarning, "SRMDevice {} is blacklisted. Ignoring it...", nodePath);
        return {};
    }

    std::unique_ptr<SRMDevice> obj { new SRMDevice(core, nodePath, isBootVGA) };

    if (obj->init())
        return obj.release();

    return {};
}

SRMDevice *SRMDevice::Make(SRMCore *core, int fd) noexcept
{
    const char *name { drmGetDeviceNameFromFd2(fd) };

    if (!name)
    {
        SRMLog(CZError, CZLN, "Failed to get node name (drmGetDeviceNameFromFd2)");
        return {};
    }

    std::unique_ptr<SRMDevice> obj { new SRMDevice(core, name, false) };
    obj->m_fd = fd;
    free((void*)name);

    if (obj->init())
        return obj.release();

    return {};
}

SRMDevice::SRMDevice(SRMCore *core, const char *nodePath, bool isBootVGA) noexcept :
    m_nodePath(nodePath),
    m_core(core),
    m_isBootVGA(isBootVGA)
{
    m_nodeName = CZStringUtils::SubStrAfterLastOf(m_nodePath, "/");
    if (m_nodeName.empty())
        m_nodeName = "Unknown Device";

    log = SRMLog.newWithContext(m_nodeName);
    log(CZInfo, "Is Boot VGA: {}", isBootVGA);
}

std::shared_ptr<SRMLease> SRMDevice::createLease(SRMLease::Resources &&resources) noexcept
{
    if (!clientCaps().Atomic)
    {
        log(CZError, CZLN, "SRM doesn't implement leasing for the legacy API");
        return {};
    }

    std::vector<UInt32> ids;
    ids.reserve(resources.connectors.size() + resources.crtcs.size() + resources.planes.size());

    for (auto &conn : resources.connectors)
    {
        if (!conn)
        {
            log(CZError, CZLN, "Invalid connector (nullptr)");
            return {};
        }

        if (conn->device() != this)
        {
            log(CZError, CZLN, "Invalid connector (belongs to another device)");
            return {};
        }

        if (conn->isInitialized() || conn->leased())
        {
            log(CZError, CZLN, "The connector can't be leased (initialized or already leased)");
            return {};
        }

        ids.emplace_back(conn->id());
    }

    for (auto &crtc : resources.crtcs)
    {
        if (!crtc)
        {
            log(CZError, CZLN, "Invalid CRTC (nullptr)");
            return {};
        }

        if (crtc->device() != this)
        {
            log(CZError, CZLN, "Invalid crtc (belongs to another device)");
            return {};
        }

        if (crtc->currentConnector() || crtc->leased())
        {
            log(CZError, CZLN, "The CRTC can't be leased (in use or already leased)");
            return {};
        }

        ids.emplace_back(crtc->id());
    }

    for (auto it = resources.planes.begin(); it != resources.planes.end();)
    {
        auto &plane { *it };

        if (!plane)
        {
            log(CZError, CZLN, "Invalid plane (nullptr)");
            return {};
        }

        if (plane->device() != this)
        {
            log(CZError, CZLN, "Invalid plane (belongs to another device)");
            return {};
        }

        if (plane->currentConnector() || plane->leased())
        {
            log(CZError, CZLN, "The plane can't be leased (in use or already leased)");
            return {};
        }

        if (core()->forcingLegacyCursor() && plane->type() == SRMPlane::Cursor)
        {
            log(CZWarning, CZLN, "Cursor planes can't be leased when the SRMCore::forcingLegacyCursor() is enabled. Skipping it...");
            it = resources.planes.erase(it);
            continue;
        }

        ids.emplace_back(plane->id());
        it++;
    }

    if (ids.empty())
    {
        log(CZError, CZLN, "Failed to create lease (the list of resources is empty)");
        return {};
    }

    UInt32 lessee;
    int leaseFd { drmModeCreateLease(fd(), ids.data(), ids.size(), O_CLOEXEC, &lessee) };

    if (leaseFd < 0)
    {
        log(CZError, CZLN, "Failed to create lease (drmModeCreateLease)");
        return {};
    }

    log(CZTrace, "Lease {} created", lessee);
    return std::shared_ptr<SRMLease>(new SRMLease(std::move(resources), this, leaseFd, lessee));
}

SRMDevice::~SRMDevice() noexcept
{
    CZVectorUtils::DeleteAndPopBackAll(m_connectors);
    CZVectorUtils::DeleteAndPopBackAll(m_planes);
    CZVectorUtils::DeleteAndPopBackAll(m_encoders);
    CZVectorUtils::DeleteAndPopBackAll(m_crtcs);

    if (fd() >= 0 && core()->m_fds.empty())
    {
        core()->m_iface->closeRestricted(fd(), core()->m_ifaceData);
        m_fd = -1;
    }
}

bool SRMDevice::init() noexcept
{
    if (core()->m_fds.empty())
    {
        m_fd = core()->m_iface->openRestricted(m_nodePath.c_str(), O_RDWR | O_CLOEXEC, core()->m_ifaceData);

        if (fd() < 0)
        {
            log(CZError, CZLN, "Failed to open DRM device");
            return false;
        }
    }

    log(CZInfo, "Is DRM Master: {}", drmIsMaster(fd()) != 0);

    drmVersion *version { drmGetVersion(fd()) };

    if (version)
    {
        log(CZInfo, "DRM Driver: {}", version->name);

        if (strcmp(version->name, "i915") == 0)
            m_driver = PDriver::i915;
        else if (strcmp(version->name, "nouveau") == 0)
            m_driver = PDriver::nouveau;
        else if (strcmp(version->name, "lima") == 0)
            m_driver = PDriver::lima;
        else if (strcmp(version->name, "nvidia-drm") == 0)
            m_driver = PDriver::nvidia;
        else if (strcmp(version->name, "nvidia") == 0)
            m_driver = PDriver::nvidia;

        drmFreeVersion(version);
    }

    initClientCaps();
    initCaps();

    drmModeResPtr res { drmModeGetResources(fd()) };

    if (!res)
    {
        log(CZError, CZLN, "Failed to get DRM resources");
        return false;
    }

    const bool ret {
        initCrtcs(res) &&
        initEncoders(res) &&
        initPlanes() &&
        initCrtcs(res) &&
        initConnectors(res)
    };

    drmModeFreeResources(res);
    return ret;
}

bool SRMDevice::initClientCaps() noexcept
{
    const char *envStereo3D { getenv("CZ_SRM_ENABLE_STEREO_3D") };

    if (envStereo3D && atoi(envStereo3D))
        m_clientCaps.Stereo3D = drmSetClientCap(fd(), DRM_CLIENT_CAP_STEREO_3D, 1) == 0;

    const char *envForceLegacyAPI { getenv("CZ_SRM_FORCE_LEGACY_API") };

    if (!envForceLegacyAPI || atoi(envForceLegacyAPI) != 1)
        m_clientCaps.Atomic = drmSetClientCap(fd(), DRM_CLIENT_CAP_ATOMIC, 1) == 0;

    if (m_clientCaps.Atomic)
    {
        // Enabled implicitly by atomic
        m_clientCaps.AspectRatio = true;
        m_clientCaps.UniversalPlanes = true;

        const char *envWriteback { getenv("CZ_SRM_ENABLE_WRITEBACK_CONNECTORS") };

        if (envWriteback && atoi(envWriteback) == 1)
            m_clientCaps.WritebackConnectors = drmSetClientCap(fd(), DRM_CLIENT_CAP_WRITEBACK_CONNECTORS, 1) == 0;
    }
    else
    {
        m_clientCaps.AspectRatio = drmSetClientCap(fd(), DRM_CLIENT_CAP_ASPECT_RATIO, 1) == 0;
        m_clientCaps.UniversalPlanes = drmSetClientCap(fd(), DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1) == 0;
    }

    return true;
}

bool SRMDevice::initCaps() noexcept
{
    UInt64 value { 0 };
    drmGetCap(fd(), DRM_CAP_DUMB_BUFFER, &value);
    m_caps.DumbBuffer = value == 1;

    value = 0;
    drmGetCap(fd(), DRM_CAP_PRIME, &value);
    m_caps.PrimeImport = value & DRM_PRIME_CAP_IMPORT;
    m_caps.PrimeExport = value & DRM_PRIME_CAP_EXPORT;

    value = 0;
    drmGetCap(fd(), DRM_CAP_ADDFB2_MODIFIERS, &value);
    m_caps.AddFb2Modifiers = value == 1;

    value = 0;
    drmGetCap(fd(), DRM_CAP_TIMESTAMP_MONOTONIC, &value);
    m_caps.TimestampMonotonic = value == 1;
    m_clock = m_caps.TimestampMonotonic ? CLOCK_MONOTONIC : CLOCK_REALTIME;

    value = 0;
    drmGetCap(fd(), DRM_CAP_ASYNC_PAGE_FLIP, &value);
    m_caps.AsyncPageFlip = value == 1;

#ifdef DRM_CAP_ATOMIC_ASYNC_PAGE_FLIP
    value = 0;
    drmGetCap(fd(), DRM_CAP_ATOMIC_ASYNC_PAGE_FLIP, &value);
    m_caps.AtomicAsyncPageFlip = value == 1;
#endif
    return true;
}

bool SRMDevice::initCrtcs(drmModeResPtr res) noexcept
{
    SRMCrtc *crtc;

    for (int i = 0; i < res->count_crtcs; i++)
    {
        crtc = SRMCrtc::Make(res->crtcs[i], this);

        if (!crtc) // All crtcs must exist
            return false;

        m_crtcs.emplace_back(crtc);
    }

    return true;
}

bool SRMDevice::initEncoders(drmModeResPtr res) noexcept
{
    SRMEncoder *encoder;

    for (int i = 0; i < res->count_encoders; i++)
    {
        encoder = SRMEncoder::Make(res->encoders[i], this);

        if (encoder)
            m_encoders.emplace_back(encoder);
    }

    return true;
}

bool SRMDevice::initPlanes() noexcept
{
    drmModePlaneResPtr res { drmModeGetPlaneResources(fd()) };

    if (!res)
    {
        log(CZError, CZLN, "Failed to get DRM planes");
        return false;
    }

    SRMPlane *plane;

    for (UInt32 i = 0; i < res->count_planes; i++)
    {
        plane = SRMPlane::Make(res->planes[i], this);

        if (plane)
            m_planes.emplace_back(plane);
    }

    drmModeFreePlaneResources(res);
    return true;
}

bool SRMDevice::initConnectors(drmModeResPtr res) noexcept
{
    SRMConnector *connector;

    for (int i = 0; i < res->count_connectors; i++)
    {
        connector = SRMConnector::Make(res->connectors[i], this);

        if (connector)
            m_connectors.emplace_back(connector);
    }

    return true;
}

bool SRMDevice::dispatchHotplugEvents() noexcept
{
    if (drmIsMaster(fd()) == 0)
    {
        m_rescanConnectors = true;
        log(CZWarning, CZLN, "Hotplug event dispatching delayed (not master)");
        return false;
    }

    // TODO: Check if the kernel registered/unregistered connectors

    m_rescanConnectors = false;

    for (auto *conn : connectors())
    {
        drmModeConnectorPtr res { drmModeGetConnector(fd(), conn->id()) };

        if (!res)
        {
            log(CZError, CZLN, "Failed to get drmModeConnectorPtr for SRMConnector {}", conn->id());
            continue;
        }

        const bool isConnected { res->connection == DRM_MODE_CONNECTED };

        if (conn->isConnected() != isConnected)
        {
            if (isConnected)
            {
                conn->updateProperties(res);
                conn->updateNames(res);
                conn->updateEncoders(res);
                conn->updateModes(res);

                log(CZInfo, "SRMConnector ({}) {}, {}, {} plugged",
                         conn->id(),
                         conn->name().c_str(),
                         conn->model().c_str(),
                         conn->make().c_str());

                core()->onConnectorPlugged.notify(conn);
            }
            else
            {
                log(CZInfo, "SRMConnector ({}) {}, {}, {} unplugged",
                    conn->id(),
                    conn->name().c_str(),
                    conn->model().c_str(),
                    conn->make().c_str());

                core()->onConnectorUnplugged.notify(conn);

                conn->uninitialize();
                conn->updateProperties(res);
                conn->updateNames(res);
                conn->updateEncoders(res);
                conn->updateModes(res);
            }
        }

        drmModeFreeConnector(res);
    }

    return true;
}
