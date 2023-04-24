#include <private/SRMPlanePrivate.h>
#include <xf86drmMode.h>
#include <SRMDevice.h>
#include <stdio.h>
#include <string.h>
#include <SRMLog.h>

using namespace SRM;

SRMPlane::SRMPlanePrivate::SRMPlanePrivate(SRMDevice *device, SRMPlane *plane, UInt32 id)
{
    this->device = device;
    this->plane = plane;
    this->id = id;
}

int SRMPlane::SRMPlanePrivate::updateProperties()
{
    drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(device->fd(), id, DRM_MODE_OBJECT_PLANE);

    if (!props)
    {
        SRMLog::error("Unable to get DRM plane %d properties.\n", id);
        return 0;
    }

    memset(&type, 10, sizeof(type));
    memset(&propIDs, 0, sizeof(SRMPlanePropIDs));

    for (UInt32 i = 0; i < props->count_props; i++)
    {
        drmModePropertyPtr prop = drmModeGetProperty(device->fd(), props->props[i]);

        if (!prop)
        {
            SRMLog::warning("Could not get property %d of plane %d.", props->props[i], id);
            continue;
        }

        if (strcmp(prop->name, "FB_ID") == 0)
            propIDs.FB_ID = prop->prop_id;
        else if (strcmp(prop->name, "FB_DAMAGE_CLIPS") == 0)
            propIDs.FB_DAMAGE_CLIPS = prop->prop_id;
        else if (strcmp(prop->name, "IN_FORMATS") == 0)
            propIDs.IN_FORMATS = prop->prop_id;
        else if (strcmp(prop->name, "CRTC_ID") == 0)
            propIDs.CRTC_ID = prop->prop_id;
        else if (strcmp(prop->name, "CRTC_X") == 0)
            propIDs.CRTC_X = prop->prop_id;
        else if (strcmp(prop->name, "CRTC_Y") == 0)
            propIDs.CRTC_Y = prop->prop_id;
        else if (strcmp(prop->name, "CRTC_W") == 0)
            propIDs.CRTC_W = prop->prop_id;
        else if (strcmp(prop->name, "CRTC_H") == 0)
            propIDs.CRTC_H = prop->prop_id;
        else if (strcmp(prop->name, "SRC_X") == 0)
            propIDs.SRC_X = prop->prop_id;
        else if (strcmp(prop->name, "SRC_Y") == 0)
            propIDs.SRC_Y= prop->prop_id;
        else if (strcmp(prop->name, "SRC_W") == 0)
            propIDs.SRC_W = prop->prop_id;
        else if (strcmp(prop->name, "SRC_H") == 0)
            propIDs.SRC_H = prop->prop_id;
        else if (strcmp(prop->name, "rotation") == 0)
            propIDs.rotation = prop->prop_id;
        else if (strcmp(prop->name, "type") == 0)
        {
            propIDs.type = prop->prop_id;
            type = (SRM_PLANE_TYPE)props->prop_values[i];
        }

        drmModeFreeProperty(prop);
    }

    drmModeFreeObjectProperties(props);

    if ((UInt32)type == 10)
    {
        SRMLog::error("Could not get plane %d type.\n", id);
        return 0;
    }

    return 1;
}

int SRMPlane::SRMPlanePrivate::updateCrtcs()
{
    drmModePlane *planeRes = drmModeGetPlane(device->fd(), id);

    if (!planeRes)
    {
        fprintf(stderr, "SRM Error: Failed to get crtcs for plane %d.\n", id);
        return 0;
    }

    int i = 0;

    for (SRMCrtc *crtc : device->crtcs())
    {
        const UInt32 mask = 1 << i;

        if (mask & planeRes->possible_crtcs)
            this->crtcs.push_back(crtc);

        i++;
    }

    drmModeFreePlane(planeRes);

    return 1;
}
