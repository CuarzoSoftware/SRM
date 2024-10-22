#include <private/SRMDevicePrivate.h>
#include <private/SRMCrtcPrivate.h>
#include <SRMList.h>
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
    if (crtc->deviceLink)
        srmListRemoveItem(crtc->device->crtcs, crtc->deviceLink);

    free(crtc);
}

UInt8 srmCrtcUpdateProperties(SRMCrtc *crtc)
{
    drmModeCrtc *crtcRes = drmModeGetCrtc(crtc->device->fd, crtc->id);

    if (!crtcRes)
    {
        SRMError("[%s] Unable to get CRTC %d resources.", crtc->device->shortName, crtc->id);
        return 0;
    }

    crtc->gammaSizeLegacy = (UInt64)crtcRes->gamma_size;
    drmModeFreeCrtc(crtcRes);

    drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(crtc->device->fd, crtc->id, DRM_MODE_OBJECT_CRTC);

    if (!props)
    {
        SRMError("[%s] Unable to get CRCT %d properties.", crtc->device->shortName, crtc->id);
        return 0;
    }

    memset(&crtc->propIDs, 0, sizeof(struct SRMCrtcPropIDs));

    for (UInt32 i = 0; i < props->count_props; i++)
    {
        drmModePropertyPtr prop = drmModeGetProperty(crtc->device->fd, props->props[i]);

        if (!prop)
        {
            SRMWarning("[%s] Could not get property %d of crtc %d.", crtc->device->shortName, props->props[i], crtc->id);
            continue;
        }

        if (strcmp(prop->name, "ACTIVE") == 0)
            crtc->propIDs.ACTIVE = prop->prop_id;
        else if (strcmp(prop->name, "GAMMA_LUT") == 0)
            crtc->propIDs.GAMMA_LUT = prop->prop_id;
        else if (strcmp(prop->name, "GAMMA_LUT_SIZE") == 0)
        {
            crtc->propIDs.GAMMA_LUT_SIZE = prop->prop_id;
            crtc->gammaSize = (UInt64)props->prop_values[i];
        }
        else if (strcmp(prop->name, "MODE_ID") == 0)
            crtc->propIDs.MODE_ID = prop->prop_id;
        else if (strcmp(prop->name, "VRR_ENABLED") == 0)
            crtc->propIDs.VRR_ENABLED = prop->prop_id;

        drmModeFreeProperty(prop);
    }

    drmModeFreeObjectProperties(props);
    return 1;
}

