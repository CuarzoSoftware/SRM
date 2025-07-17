#include <CZ/SRM/SRMLog.h>
#include <CZ/SRM/SRMPlane.h>
#include <CZ/SRM/SRMDevice.h>
#include <CZ/Utils/CZVectorUtils.h>
#include <cstring>
#include <memory>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

using namespace CZ;

SRMPlane *SRMPlane::Make(UInt32 id, SRMDevice *device) noexcept
{
    drmModePlanePtr res { drmModeGetPlane(device->fd(), id) };

    if (!res)
    {
        device->log(CZError, CZLN, "Failed to get drmModePlanePtr for SRMPlane {}", id);
        return {};
    }

    std::unique_ptr<SRMPlane> obj { new SRMPlane(id, device) };

    if (obj->initPropIds() &&
        obj->initLegacyFormats(res) &&
        obj->initCrtcs(res))
    {
        drmModeFreePlane(res);
        return obj.release();
    }

    drmModeFreePlane(res);
    device->log(CZError, CZLN, "Failed to create SRMPlane {}", id);
    return {};
}

bool SRMPlane::initPropIds() noexcept
{
    drmModeObjectPropertiesPtr props { drmModeObjectGetProperties(device()->fd(), id(), DRM_MODE_OBJECT_PLANE) };

    if (!props)
    {
        device()->log(CZError, CZLN, "Failed to get properties for SRMPlane {}", id());
        return false;
    }

    bool hasType { false };

    for (UInt32 i = 0; i < props->count_props; i++)
    {
        drmModePropertyPtr prop { drmModeGetProperty(device()->fd(), props->props[i]) };

        if (!prop)
        {
            device()->log(CZWarning, CZLN, "Failed to get property {} for SRMPlane {}", props->props[i], id());
            continue;
        }

        if (strcmp(prop->name, "FB_ID") == 0)
            m_propIDs.FB_ID = prop->prop_id;
        else if (strcmp(prop->name, "FB_DAMAGE_CLIPS") == 0)
            m_propIDs.FB_DAMAGE_CLIPS = prop->prop_id;
        else if (strcmp(prop->name, "IN_FENCE_FD") == 0)
            m_propIDs.IN_FENCE_FD = prop->prop_id;
        else if (strcmp(prop->name, "IN_FORMATS") == 0)
        {
            m_propIDs.IN_FORMATS = prop->prop_id;
            initInFormats(props->prop_values[i]);
        }
        else if (strcmp(prop->name, "CRTC_ID") == 0)
            m_propIDs.CRTC_ID = prop->prop_id;
        else if (strcmp(prop->name, "CRTC_X") == 0)
            m_propIDs.CRTC_X = prop->prop_id;
        else if (strcmp(prop->name, "CRTC_Y") == 0)
            m_propIDs.CRTC_Y = prop->prop_id;
        else if (strcmp(prop->name, "CRTC_W") == 0)
            m_propIDs.CRTC_W = prop->prop_id;
        else if (strcmp(prop->name, "CRTC_H") == 0)
            m_propIDs.CRTC_H = prop->prop_id;
        else if (strcmp(prop->name, "SRC_X") == 0)
            m_propIDs.SRC_X = prop->prop_id;
        else if (strcmp(prop->name, "SRC_Y") == 0)
            m_propIDs.SRC_Y= prop->prop_id;
        else if (strcmp(prop->name, "SRC_W") == 0)
            m_propIDs.SRC_W = prop->prop_id;
        else if (strcmp(prop->name, "SRC_H") == 0)
            m_propIDs.SRC_H = prop->prop_id;
        else if (strcmp(prop->name, "rotation") == 0)
            m_propIDs.rotation = prop->prop_id;
        else if (strcmp(prop->name, "type") == 0)
        {
            m_propIDs.type = prop->prop_id;
            m_type = (Type)props->prop_values[i];
            hasType = true;
        }

        drmModeFreeProperty(prop);
    }

    drmModeFreeObjectProperties(props);

    if (!hasType)
    {
        device()->log(CZError, CZLN, "Failed to get SRMPlane {} type", id());
        return false;
    }

    return 1;
}

void SRMPlane::initInFormats(UInt64 blobId) noexcept
{
    if (device()->caps().AddFb2Modifiers)
    {
        drmModePropertyBlobRes *blob { drmModeGetPropertyBlob(device()->fd(), blobId) };

        if (blob)
        {
            drmModeFormatModifierIterator iter {};
            m_formats.clear();
            while (drmModeFormatModifierBlobIterNext(blob, &iter))
                m_formats[iter.fmt].insert(iter.mod);

            drmModeFreePropertyBlob(blob);
        }
    }
}

bool SRMPlane::initLegacyFormats(drmModePlanePtr res) noexcept
{
    for (UInt32 i = 0; i < res->count_formats; i++)
        m_formats[res->formats[i]].insert(DRM_FORMAT_MOD_INVALID);

    return true;
}

bool SRMPlane::initCrtcs(drmModePlanePtr res) noexcept
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
