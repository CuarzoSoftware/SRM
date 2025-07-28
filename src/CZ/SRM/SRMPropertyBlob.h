#ifndef SRMPROPERTYBLOB_H
#define SRMPROPERTYBLOB_H

#include <CZ/SRM/SRMObject.h>
#include <memory>

class CZ::SRMPropertyBlob final : public SRMObject
{
public:
    static std::shared_ptr<SRMPropertyBlob> Make(SRMDevice *device, const void *data, size_t size) noexcept;
    ~SRMPropertyBlob() noexcept;
    UInt32 id() const noexcept { return m_id; };
    SRMDevice *device() const noexcept { return m_device; }
private:
    SRMPropertyBlob(SRMDevice *device, UInt32 id) noexcept;
    SRMDevice *m_device;
    UInt32 m_id;
};

#endif // SRMPROPERTYBLOB_H
