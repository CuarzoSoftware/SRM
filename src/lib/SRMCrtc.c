#include <private/SRMCrtcPrivate.h>
#include <private/SRMDevicePrivate.h>

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

UInt64 srmCrtcGetGammaSize(SRMCrtc *crtc)
{
    if (crtc->device->clientCapAtomic && crtc->propIDs.GAMMA_LUT_SIZE)
        return crtc->gammaSize;

    return crtc->gammaSizeLegacy;
}
