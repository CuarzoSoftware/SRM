#ifndef SRMPLANEPRIVATE_H
#define SRMPLANEPRIVATE_H

#include <SRMPlane.h>

class SRM::SRMPlane::SRMPlanePrivate
{
public:
    SRMPlanePrivate(SRMDevice *device, SRMPlane *plane, UInt32 id);
    ~SRMPlanePrivate() = default;
    SRMDevice *device;
    SRMPlane *plane;
    SRMConnector *currentConnector = nullptr;
    std::list<SRMCrtc*>crtcs;
    UInt32 id;
    SRM_PLANE_TYPE type;
    int updateProperties();
    int updateCrtcs();
    std::list<SRMPlane*>::iterator deviceLink;

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
    } propIDs;
};

#endif // SRMPLANEPRIVATE_H
