#ifndef SRMCRTCPRIVATE_H
#define SRMCRTCPRIVATE_H

#include <SRMCrtc.h>

class SRM::SRMCrtc::SRMCrtcPrivate
{
public:
    SRMCrtcPrivate(SRMCrtc *crtc);
    ~SRMCrtcPrivate() = default;
    UInt32 id;
    SRMDevice *device = nullptr;
    SRMCrtc *crtc = nullptr;
    SRMConnector *currentConnector = nullptr;
    std::list<SRMCrtc*>::iterator deviceLink;
    int updateProperties();

    struct SRMCrtcPropIDs
    {
        UInt32
        ACTIVE,
        GAMMA_LUT,
        GAMMA_LUT_SIZE,
        MODE_ID,
        VRR_ENABLED;
    } propIDs;
};

#endif // SRMCRTCPRIVATE_H
