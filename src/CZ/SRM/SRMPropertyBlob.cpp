#include <CZ/SRM/SRMPropertyBlob.h>
#include <CZ/SRM/SRMDevice.h>
#include <xf86drmMode.h>

using namespace CZ;

std::shared_ptr<SRMPropertyBlob> SRMPropertyBlob::Make(SRMDevice *device, const void *data, size_t size) noexcept
{
    if (!device)
        return {};

    UInt32 id;
    if (drmModeCreatePropertyBlob(device->fd(), data, size, &id) != 0)
        return {};

    return std::shared_ptr<SRMPropertyBlob>(new SRMPropertyBlob(device, id));
}

SRMPropertyBlob::~SRMPropertyBlob() noexcept
{
    drmModeDestroyPropertyBlob(device()->fd(), id());
}

SRMPropertyBlob::SRMPropertyBlob(SRMDevice *device, UInt32 id) noexcept : m_device(device), m_id(id) {}
