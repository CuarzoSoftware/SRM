#ifndef CZ_SRMATOMICREQUEST_H
#define CZ_SRMATOMICREQUEST_H

#include <CZ/SRM/SRMObject.h>
#include <memory>
#include <unordered_set>
#include <xf86drmMode.h>

class CZ::SRMAtomicRequest final : public SRMObject
{
public:
    static std::shared_ptr<SRMAtomicRequest> Make(SRMDevice *device) noexcept;
    int addProperty(UInt32 objectId, UInt32 propertyId, UInt64 value) noexcept;
    int commit(UInt32 flags, void *data, bool forceRetry) noexcept;

    void attachPropertyBlob(std::shared_ptr<SRMPropertyBlob> blob) noexcept;
    void attachFd(int fd) noexcept;

    ~SRMAtomicRequest() noexcept;
    SRMDevice *device() const noexcept { return m_device; };
    drmModeAtomicReqPtr request() const noexcept { return m_req; }
private:
    SRMAtomicRequest(SRMDevice *device, drmModeAtomicReqPtr req) noexcept :
        m_device(device), m_req(req) {}
    std::vector<std::shared_ptr<SRMPropertyBlob>> m_blobs;
    std::unordered_set<int> m_fds;
    SRMDevice *m_device;
    drmModeAtomicReqPtr m_req;
};

#endif // CZ_SRMATOMICREQUEST_H
