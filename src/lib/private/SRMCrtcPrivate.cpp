#include <private/SRMCrtcPrivate.h>
#include <SRMDevice.h>
#include <xf86drmMode.h>
#include <stdio.h>

SRM::SRMCrtc::SRMCrtcPrivate::SRMCrtcPrivate(SRMCrtc *crtc)
{
    this->crtc = crtc;
}

int SRM::SRMCrtc::SRMCrtcPrivate::updateProperties()
{
    drmModeCrtc *res = drmModeGetCrtc(device->fd(), id);

    if (!res)
    {
        fprintf(stderr, "SRM Error: Could not get CRTC properties.\n");
        return 0;
    }

    // TODO use this props ?

    drmModeFreeCrtc(res);

    return 1;
}
