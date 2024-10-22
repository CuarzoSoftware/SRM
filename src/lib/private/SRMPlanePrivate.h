#ifndef SRMPLANEPRIVATE_H
#define SRMPLANEPRIVATE_H

#include <SRMPlane.h>

#ifdef __cplusplus
extern "C" {
#endif

struct SRMPlanePropIDs
{
    UInt32
    FB_ID,
    FB_DAMAGE_CLIPS,
    IN_FORMATS,
    IN_FENCE_FD,
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
    SRMList *crtcs, *inFormats, *inFormatsBlacklist;
    SRM_PLANE_TYPE type;
    struct SRMPlanePropIDs propIDs;
};

SRMPlane *srmPlaneCreate(SRMDevice *device, UInt32 id);
void srmPlaneDestroy(SRMPlane *plane);
UInt8 srmPlaneUpdateProperties(SRMPlane *plane);
UInt8 srmPlaneUpdateCrtcs(SRMPlane *plane);
void srmPlaneUpdateInFormats(SRMPlane *plane, UInt64 blobID);
void srmPlaneDestroyInFormats(SRMPlane *plane);

UInt8 srmPlaneUpdateFormats(SRMPlane *plane);

#ifdef __cplusplus
}
#endif

#endif // SRMPLANEPRIVATE_H
