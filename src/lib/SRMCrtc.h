#ifndef SRMCRTC_H
#define SRMCRTC_H

#include <SRMTypes.h>

UInt32 srmCrtcGetID(SRMCrtc *crtc);
SRMDevice *srmCrtcGetDevice(SRMCrtc *crtc);
SRMConnector *srmCrtcGetCurrentConnector(SRMCrtc *crtc);

#endif // SRMCRTC_H
