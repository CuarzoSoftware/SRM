#include <private/SRMCrtcPrivate.h>

UInt32 srmCrtcGetID(SRMCrtc *crtc)
{
    return crtc->id;
}

SRMDevice *srmCrtcGetDevice(SRMCrtc *crtc)
{
    return crtc->device;
}

SRMConnector *srmCrtcGetCurrentConnector(SRMCrtc *crtc)
{
    return crtc->currentConnector;
}
