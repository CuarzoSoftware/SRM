#ifndef SRMCRTCPRIVATE_H
#define SRMCRTCPRIVATE_H

#include "../SRMCrtc.h"

#ifdef __cplusplus
extern "C" {
#endif

struct SRMCrtcPropIDs
{
    UInt32
    ACTIVE,
    GAMMA_LUT,
    GAMMA_LUT_SIZE,
    MODE_ID,
    VRR_ENABLED;
};

struct SRMCrtcStruct
{
    UInt32 id;
    SRMDevice *device;
    SRMListItem *deviceLink;
    SRMConnector *currentConnector;
    struct SRMCrtcPropIDs propIDs;
    UInt64 gammaSizeLegacy;
    UInt64 gammaSize;
};

SRMCrtc *srmCrtcCreate(SRMDevice *device, UInt32 id);
void srmCrtcDestroy(SRMCrtc *crtc);
UInt8 srmCrtcUpdateProperties(SRMCrtc *crtc);

#ifdef __cplusplus
}
#endif

#endif // SRMCRTCPRIVATE_H
