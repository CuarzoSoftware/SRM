#include <CZ/SRM/SRMAtomicRequest.h>
#include <CZ/SRM/SRMDevice.h>

using namespace CZ;

std::shared_ptr<SRMAtomicRequest> SRMAtomicRequest::Make(SRMDevice *device) noexcept
{
    if (!device || !device->clientCaps().Atomic)
    {
        SRMLog(CZDebug, CZLN, "Invalid device");
        return {};
    }

    drmModeAtomicReqPtr req { drmModeAtomicAlloc() };
    if (!req) return {};

    return std::shared_ptr<SRMAtomicRequest>(new SRMAtomicRequest(device, req));
}

int SRMAtomicRequest::commit(UInt32 flags, void *data, bool forceRetry) noexcept
{
    if (!forceRetry)
        return drmModeAtomicCommit(device()->fd(), m_req, flags, data);

    int ret;
retry:
    ret = drmModeAtomicCommit(device()->fd(), m_req, flags | DRM_MODE_ATOMIC_TEST_ONLY, data);

    if (ret == -16) // -EBUSY
    {
        usleep(2000);
        goto retry;
    }

    return drmModeAtomicCommit(device()->fd(), m_req, flags, data);
}

void SRMAtomicRequest::attachPropertyBlob(std::shared_ptr<SRMPropertyBlob> blob) noexcept
{
    m_blobs.emplace_back(blob);
}

int SRMAtomicRequest::addProperty(UInt32 objectId, UInt32 propertyId, UInt64 value) noexcept
{
    return drmModeAtomicAddProperty(m_req, objectId, propertyId, value);
}

SRMAtomicRequest::~SRMAtomicRequest() noexcept
{

    drmModeAtomicFree(m_req);
}
