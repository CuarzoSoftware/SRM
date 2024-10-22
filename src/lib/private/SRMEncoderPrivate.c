#include <private/SRMEncoderPrivate.h>
#include <private/SRMDevicePrivate.h>
#include <SRMList.h>
#include <SRMLog.h>

#include <xf86drmMode.h>
#include <stdlib.h>

SRMEncoder *srmEncoderCreate(SRMDevice *device, UInt32 id)
{
    SRMEncoder *encoder = calloc(1, sizeof(SRMEncoder));
    encoder->id = id;
    encoder->device = device;
    encoder->crtcs = srmListCreate();

    if (!srmEncoderUpdateCrtcs(encoder))
    {
        srmEncoderDestroy(encoder);
        return NULL;
    }

    return encoder;
}

void srmEncoderDestroy(SRMEncoder *encoder)
{
    if (encoder->deviceLink)
        srmListRemoveItem(encoder->device->encoders, encoder->deviceLink);

    srmListDestroy(encoder->crtcs);
    free(encoder);
}

UInt8 srmEncoderUpdateCrtcs(SRMEncoder *encoder)
{
    drmModeEncoder *encoderRes = drmModeGetEncoder(encoder->device->fd, encoder->id);

    if (!encoderRes)
    {
        SRMError("[%s] Failed to get CRTCs for encoder %d.", encoder->device->shortName, encoder->id);
        return 0;
    }

    int i = 0;

    SRMListForeach(item, encoder->device->crtcs)
    {
        SRMCrtc *crtc = srmListItemGetData(item);
        const UInt32 mask = 1 << i;

        if (mask & encoderRes->possible_crtcs)
            srmListAppendData(encoder->crtcs, crtc);

        i++;
    }

    drmModeFreeEncoder(encoderRes);

    return 1;
}
