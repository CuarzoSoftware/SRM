#ifndef SRMPLANEPRIVATE_H
#define SRMPLANEPRIVATE_H

#include <SRMPlane.h>

struct SRMPlanePropIDs
{
    UInt32
    FB_ID,
    FB_DAMAGE_CLIPS,
    IN_FORMATS,
    CRTC_ID,
    CRTC_X,
    CRTC_Y,
    CRTC_W,
    CRTC_H,
    SRC_X,
    SRC_Y,
    SRC_W,
    SRC_H,
    rotation,
    type;
};

struct SRMPlaneStruct
{
    UInt32 id;
    SRMDevice *device;
    SRMListItem *deviceLink;
    SRMConnector *currentConnector;
    SRMList *crtcs;
    SRM_PLANE_TYPE type;
    struct SRMPlanePropIDs propIDs;
};

SRMPlane *srmPlaneCreate(SRMDevice *device, UInt32 id);
void srmPlaneDestroy(SRMPlane *plane);
UInt8 srmPlaneUpdateProperties(SRMPlane *plane);
UInt8 srmPlaneUpdateCrtcs(SRMPlane *plane);

#endif // SRMPLANEPRIVATE_H
