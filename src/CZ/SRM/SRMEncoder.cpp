#include <CZ/SRM/SRMEncoder.h>
#include <CZ/SRM/SRMDevice.h>
#include <CZ/SRM/SRMLog.h>
#include <memory>
#include <xf86drmMode.h>

using namespace CZ;

SRMEncoder *SRMEncoder::Make(UInt32 id, SRMDevice *device) noexcept
{
    drmModeEncoderPtr res { drmModeGetEncoder(device->fd(), id) };

    if (!res)
    {
        device->log(CZError, CZLN, "Failed to get drmModeEncoderPtr for SRMEncoder {}", id);
        return {};
    }

    std::unique_ptr<SRMEncoder> obj { new SRMEncoder(id, device) };

    if (obj->initCrtcs(res))
    {
        drmModeFreeEncoder(res);
        return obj.release();
    }

    drmModeFreeEncoder(res);
    device->log(CZError, CZLN, "[%s] Failed to create SRMEncoder {}", id);
    return {};
}

bool SRMEncoder::initCrtcs(drmModeEncoderPtr res) noexcept
{
    /* CRTCs are created in the same order by SRMDevice */
    for (size_t i = 0; i < device()->crtcs().size(); i++)
    {
        const UInt32 mask { 1U << i };

        if (mask & res->possible_crtcs)
            m_crtcs.emplace_back(device()->crtcs()[i]);
    }

    return true;
}
