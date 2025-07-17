#include <CZ/SRM/SRMLog.h>
#include <CZ/SRM/SRMCrtc.h>
#include <CZ/SRM/SRMDevice.h>
#include <cstring>
#include <memory>
#include <xf86drmMode.h>

using namespace CZ;

SRMCrtc *SRMCrtc::Make(UInt32 id, SRMDevice *device) noexcept
{
    drmModeCrtcPtr res { drmModeGetCrtc(device->fd(), id) };

    if (!res)
    {
        device->log(CZError, CZLN, "Failed to get drmModeCrtcPtr for SRMCrtc {}", id);
        return {};
    }

    std::unique_ptr<SRMCrtc> obj { new SRMCrtc(id, device) };

    if (obj->initPropIds() && obj->initLegacyGamma(res))
    {
        drmModeFreeCrtc(res);
        return obj.release();
    }

    drmModeFreeCrtc(res);
    device->log(CZError, CZLN, "Failed to create SRMCrtc {}", id);
    return {};
}

bool SRMCrtc::initPropIds() noexcept
{
    drmModeObjectPropertiesPtr props { drmModeObjectGetProperties(device()->fd(), id(), DRM_MODE_OBJECT_CRTC) };

    if (!props)
    {
        device()->log(CZError, CZLN, "Failed to get drmModeObjectPropertiesPtr for SRMCrtc {}", id());
        return false;
    }

    for (UInt32 i = 0; i < props->count_props; i++)
    {
        drmModePropertyPtr prop { drmModeGetProperty(device()->fd(), props->props[i]) };

        if (!prop)
        {
            device()->log(CZWarning, CZLN, "Could not get property {} for SRMCrtc {}", props->props[i], id());
            continue;
        }

        if (strcmp(prop->name, "ACTIVE") == 0)
            m_propIDs.ACTIVE = prop->prop_id;
        else if (strcmp(prop->name, "GAMMA_LUT") == 0)
            m_propIDs.GAMMA_LUT = prop->prop_id;
        else if (strcmp(prop->name, "GAMMA_LUT_SIZE") == 0)
        {
            m_propIDs.GAMMA_LUT_SIZE = prop->prop_id;
            m_gammaSize = (UInt64)props->prop_values[i];
        }
        else if (strcmp(prop->name, "MODE_ID") == 0)
            m_propIDs.MODE_ID = prop->prop_id;
        else if (strcmp(prop->name, "VRR_ENABLED") == 0)
            m_propIDs.VRR_ENABLED = prop->prop_id;

        drmModeFreeProperty(prop);
    }

    drmModeFreeObjectProperties(props);
    return true;
}

bool SRMCrtc::initLegacyGamma(drmModeCrtcPtr res) noexcept
{
    m_gammaSizeLegacy = static_cast<UInt64>(res->gamma_size);
    return true;
}

UInt64 SRMCrtc::gammaSize() const noexcept
{
    if (device()->clientCaps().Atomic && m_propIDs.GAMMA_LUT_SIZE)
        return m_gammaSize;

    return m_gammaSizeLegacy;
}
