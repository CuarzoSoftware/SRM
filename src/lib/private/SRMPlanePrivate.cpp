#include <private/SRMPlanePrivate.h>
#include <xf86drmMode.h>
#include <SRMDevice.h>
#include <stdio.h>
#include <string.h>

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
        fprintf(stderr, "SRM Error: Unable to get DRM plane properties.\n");
        return 0;
    }

    for (UInt32 i = 0; i < props->count_props; i++)
    {
        drmModePropertyPtr prop = drmModeGetProperty(device->fd(), props->props[i]);

        if (strcmp(prop->name, "type") == 0)
        {
            type = (SRM_PLANE_TYPE)props->prop_values[i];
            drmModeFreeProperty(prop);
            drmModeFreeObjectProperties(props);
            return 1;
        }

        drmModeFreeProperty(prop);
    }

    drmModeFreeObjectProperties(props);

    return 0;
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
