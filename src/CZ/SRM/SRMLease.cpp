#include <CZ/SRM/SRMLease.h>
#include <CZ/SRM/SRMDevice.h>
#include <CZ/SRM/SRMConnector.h>
#include <CZ/SRM/SRMCrtc.h>
#include <CZ/SRM/SRMPlane.h>

using namespace CZ;

SRMLease::~SRMLease() noexcept
{
    if (!m_device)
    {
        SRMLog(CZError, CZLN, "Failed to revoke lease (device already destroyed)");
        return;
    }

    drmModeRevokeLease(m_device->fd(), m_lessee);

    for (auto &r : m_resources.connectors)
        if (r) r->m_leased = false;

    for (auto &r : m_resources.crtcs)
        if (r) r->m_leased = false;

    for (auto &r : m_resources.planes)
        if (r) r->m_leased = false;

    m_device->log(CZTrace, "Lease {} revoked", m_lessee);
}

SRMLease::SRMLease(Resources &&resources, SRMDevice *device, int fd, UInt32 lessee) noexcept :
    m_resources(resources),
    m_device(device),
    m_fd(fd),
    m_lessee(lessee) {
}
