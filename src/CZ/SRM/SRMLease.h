#ifndef SRMLEASE_H
#define SRMLEASE_H

#include <CZ/SRM/SRMObject.h>
#include <CZ/Core/CZSpFd.h>
#include <CZ/Core/CZWeak.h>
#include <unordered_set>

/**
 * @brief DRM/KMS resource lease
 *
 * Represents a temporary delegation of connectors, CRTCs, and planes to another process. Created via SRMDevice::createLease().
 * The lease is revoked and the fd closed on destruction. Leased resources cannot be used by SRM until revoked.
 */
class CZ::SRMLease : public SRMObject
{
public:

    /**
     * @brief Leased resources
     */
    struct Resources
    {
        std::unordered_set<CZWeak<SRMConnector>> connectors;
        std::unordered_set<CZWeak<SRMPlane>> planes;
        std::unordered_set<CZWeak<SRMCrtc>> crtcs;
    };

    ~SRMLease() noexcept;

    /// The device that granted this lease
    SRMDevice *device() const noexcept { return m_device.get(); }

    /// The set of leased resources
    const Resources &resources() const noexcept { return m_resources; }

    /// DRM file descriptor for the lessee process (owned by the SRMLease)
    int fd() const noexcept { return m_fd.get(); }
private:
    friend class SRMDevice;
    SRMLease(Resources &&resources, SRMDevice *device, int fd, UInt32 lessee) noexcept;
    Resources m_resources;
    CZWeak<SRMDevice> m_device;
    CZSpFd m_fd;
    UInt32 m_lessee;
};

#endif // SRMLEASE_H
