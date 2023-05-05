#include <private/SRMDevicePrivate.h>
#include <private/SRMCrtcPrivate.h>
#include <SRMLog.h>

#include <xf86drmMode.h>
#include <string.h>
#include <stdlib.h>

SRMCrtc *srmCrtcCreate(SRMDevice *device, UInt32 id)
{
    SRMCrtc *crtc = calloc(1, sizeof(SRMCrtc));

    crtc->device = device;
    crtc->id = id;

    if (!srmCrtcUpdateProperties(crtc))
    {
        srmCrtcDestroy(crtc);
        return NULL;
    }

    return crtc;
}

void srmCrtcDestroy(SRMCrtc *crtc)
{
    free(crtc);
}


UInt8 srmCrtcUpdateProperties(SRMCrtc *crtc)
{
    drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(crtc->device->fd, crtc->id, DRM_MODE_OBJECT_CRTC);

    if (!props)
    {
        SRMError("Unable to get device %s crtc %d properties.", crtc->device->name, crtc->id);
        return 0;
    }

    memset(&crtc->propIDs, 0, sizeof(struct SRMCrtcPropIDs));

    for (UInt32 i = 0; i < props->count_props; i++)
    {
        drmModePropertyPtr prop = drmModeGetProperty(crtc->device->fd, props->props[i]);

        if (!prop)
        {
            SRMWarning("Could not get property %d of device %s crtc %d.", props->props[i], crtc->device->name, crtc->id);
            continue;
        }

        if (strcmp(prop->name, "ACTIVE") == 0)
            crtc->propIDs.ACTIVE = prop->prop_id;
        else if (strcmp(prop->name, "GAMMA_LUT") == 0)
            crtc->propIDs.GAMMA_LUT = prop->prop_id;
        else if (strcmp(prop->name, "GAMMA_LUT_SIZE") == 0)
            crtc->propIDs.GAMMA_LUT_SIZE = prop->prop_id;
        else if (strcmp(prop->name, "MODE_ID") == 0)
            crtc->propIDs.MODE_ID = prop->prop_id;
        else if (strcmp(prop->name, "VRR_ENABLED") == 0)
            crtc->propIDs.VRR_ENABLED = prop->prop_id;

        drmModeFreeProperty(prop);
    }

    drmModeFreeObjectProperties(props);


    return 1;
}

