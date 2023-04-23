#include <private/SRMEncoderPrivate.h>
#include <xf86drmMode.h>
#include <SRMDevice.h>
#include <stdio.h>

using namespace SRM;

SRMEncoder::SRMEncoderPrivate::SRMEncoderPrivate(SRMDevice *device, UInt32 id, SRMEncoder *encoder)
{
    this->id = id;
    this->encoder = encoder;
    this->device = device;
}

int SRMEncoder::SRMEncoderPrivate::updateCrtcs()
{
    drmModeEncoder *encoderRes = drmModeGetEncoder(device->fd(), id);

    if (!encoderRes)
    {
        fprintf(stderr, "SRM Error: Failed to get crtcs for encoder %d.\n", id);
        return 0;
    }

    int i = 0;

    for (SRMCrtc *crtc : device->crtcs())
    {
        const UInt32 mask = 1 << i;

        if (mask & encoderRes->possible_crtcs)
            this->crtcs.push_back(crtc);

        i++;
    }

    drmModeFreeEncoder(encoderRes);

    return 1;
}
