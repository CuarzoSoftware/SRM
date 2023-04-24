#include <cstring>
#include <private/SRMCrtcPrivate.h>
#include <SRMDevice.h>
#include <xf86drmMode.h>
#include <stdio.h>
#include <SRMLog.h>

SRM::SRMCrtc::SRMCrtcPrivate::SRMCrtcPrivate(SRMCrtc *crtc)
{
    this->crtc = crtc;
}

int SRM::SRMCrtc::SRMCrtcPrivate::updateProperties()
{
    drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(device->fd(), id, DRM_MODE_OBJECT_CRTC);

    if (!props)
    {
        SRMLog::error("Unable to get DRM crtc %d properties.", id);
        return 0;
    }

    memset(&propIDs, 0, sizeof(SRMCrtcPropIDs));

    for (UInt32 i = 0; i < props->count_props; i++)
    {
        drmModePropertyPtr prop = drmModeGetProperty(device->fd(), props->props[i]);

        if (!prop)
        {
            SRMLog::warning("Could not get property %d of crtc %d.", props->props[i], id);
            continue;
        }

        if (strcmp(prop->name, "ACTIVE") == 0)
            propIDs.ACTIVE = prop->prop_id;
        else if (strcmp(prop->name, "GAMMA_LUT") == 0)
            propIDs.GAMMA_LUT = prop->prop_id;
        else if (strcmp(prop->name, "GAMMA_LUT_SIZE") == 0)
            propIDs.GAMMA_LUT_SIZE = prop->prop_id;
        else if (strcmp(prop->name, "MODE_ID") == 0)
            propIDs.MODE_ID = prop->prop_id;
        else if (strcmp(prop->name, "VRR_ENABLED") == 0)
            propIDs.VRR_ENABLED = prop->prop_id;

        drmModeFreeProperty(prop);
    }

    drmModeFreeObjectProperties(props);


    return 1;
}
