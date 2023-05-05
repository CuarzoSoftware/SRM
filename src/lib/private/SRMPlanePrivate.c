#include <private/SRMPlanePrivate.h>
#include <private/SRMDevicePrivate.h>

#include <SRMList.h>
#include <SRMLog.h>

#include <xf86drmMode.h>
#include <string.h>
#include <stdlib.h>

SRMPlane *srmPlaneCreate(SRMDevice *device, UInt32 id)
{
    SRMPlane *plane = calloc(1, sizeof(SRMPlane));
    plane->id = id;
    plane->device = device;

    if (!srmPlaneUpdateProperties(plane))
        goto fail;

    plane->crtcs = srmListCreate();
    if (!srmPlaneUpdateCrtcs(plane))
        goto fail;

    return plane;

    fail:
    srmPlaneDestroy(plane);
    return NULL;
}

void srmPlaneDestroy(SRMPlane *plane)
{
    if (plane->crtcs)
        srmListDestoy(plane->crtcs);

    free(plane);
}

UInt8 srmPlaneUpdateProperties(SRMPlane *plane)
{
    drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(plane->device->fd, plane->id, DRM_MODE_OBJECT_PLANE);

    if (!props)
    {
        SRMError("Unable to get device %s plane %d properties.", plane->device->name, plane->id);
        return 0;
    }

    plane->type = 10;
    memset(&plane->propIDs, 0, sizeof(struct SRMPlanePropIDs));

    for (UInt32 i = 0; i < props->count_props; i++)
    {
        drmModePropertyPtr prop = drmModeGetProperty(plane->device->fd, props->props[i]);

        if (!prop)
        {
            SRMWarning("Could not get device %s property %d of plane %d.", plane->device->name, props->props[i], plane->id);
            continue;
        }

        if (strcmp(prop->name, "FB_ID") == 0)
            plane->propIDs.FB_ID = prop->prop_id;
        else if (strcmp(prop->name, "FB_DAMAGE_CLIPS") == 0)
            plane->propIDs.FB_DAMAGE_CLIPS = prop->prop_id;
        else if (strcmp(prop->name, "IN_FORMATS") == 0)
            plane->propIDs.IN_FORMATS = prop->prop_id;
        else if (strcmp(prop->name, "CRTC_ID") == 0)
            plane->propIDs.CRTC_ID = prop->prop_id;
        else if (strcmp(prop->name, "CRTC_X") == 0)
            plane->propIDs.CRTC_X = prop->prop_id;
        else if (strcmp(prop->name, "CRTC_Y") == 0)
            plane->propIDs.CRTC_Y = prop->prop_id;
        else if (strcmp(prop->name, "CRTC_W") == 0)
            plane->propIDs.CRTC_W = prop->prop_id;
        else if (strcmp(prop->name, "CRTC_H") == 0)
            plane->propIDs.CRTC_H = prop->prop_id;
        else if (strcmp(prop->name, "SRC_X") == 0)
            plane->propIDs.SRC_X = prop->prop_id;
        else if (strcmp(prop->name, "SRC_Y") == 0)
            plane->propIDs.SRC_Y= prop->prop_id;
        else if (strcmp(prop->name, "SRC_W") == 0)
            plane->propIDs.SRC_W = prop->prop_id;
        else if (strcmp(prop->name, "SRC_H") == 0)
            plane->propIDs.SRC_H = prop->prop_id;
        else if (strcmp(prop->name, "rotation") == 0)
            plane->propIDs.rotation = prop->prop_id;
        else if (strcmp(prop->name, "type") == 0)
        {
            plane->propIDs.type = prop->prop_id;
            plane->type = (SRM_PLANE_TYPE)props->prop_values[i];
        }

        drmModeFreeProperty(prop);
    }

    drmModeFreeObjectProperties(props);

    if (plane->type == 10)
    {
        SRMError("Could not get device %s plane %d type.", plane->device->name, plane->id);
        return 0;
    }

    return 1;
}

UInt8 srmPlaneUpdateCrtcs(SRMPlane *plane)
{
    drmModePlane *planeRes = drmModeGetPlane(plane->device->fd, plane->id);

    if (!planeRes)
    {
        SRMError("Failed to get device %s crtcs for plane %d.", plane->device->name, plane->id);
        return 0;
    }

    int i = 0;

    SRMListForeach(item, plane->device->crtcs)
    {
        SRMCrtc *crtc = srmListItemGetData(item);
        const UInt32 mask = 1 << i;

        if (mask & planeRes->possible_crtcs)
            srmListAppendData(plane->crtcs, crtc);

        i++;
    }

    drmModeFreePlane(planeRes);

    return 1;
}

