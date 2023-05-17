#ifndef SRMCRTC_H
#define SRMCRTC_H

#include "SRMTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

UInt32 srmCrtcGetID(SRMCrtc *crtc);
SRMDevice *srmCrtcGetDevice(SRMCrtc *crtc);
SRMConnector *srmCrtcGetCurrentConnector(SRMCrtc *crtc);

#ifdef __cplusplus
}
#endif

#endif // SRMCRTC_H
