#include <private/SRMPlanePrivate.h>

UInt32 srmPlaneGetID(SRMPlane *plane)
{
    return plane->id;
}

SRMDevice *srmPlaneGetDevice(SRMPlane *plane)
{
    return plane->device;
}

SRMList *srmPlaneGetCrtcs(SRMPlane *plane)
{
    return plane->crtcs;
}

SRMConnector *srmPlaneGetCurrentConnector(SRMPlane *plane)
{
    return plane->currentConnector;
}

SRM_PLANE_TYPE srmPlaneGetType(SRMPlane *plane)
{
    return plane->type;
}
