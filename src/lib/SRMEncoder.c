#include <private/SRMEncoderPrivate.h>

UInt32 srmEncoderGetID(SRMEncoder *encoder)
{
    return encoder->id;
}

SRMDevice *srmEncoderGetDevice(SRMEncoder *encoder)
{
    return encoder->device;
}

SRMList *srmEncoderGetCrtcs(SRMEncoder *encoder)
{
    return encoder->crtcs;
}

SRMConnector *srmEncoderGetCurrentConnector(SRMEncoder *encoder)
{
    return encoder->currentConnector;
}
