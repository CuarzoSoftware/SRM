#ifndef SRMENCODERPRIVATE_H
#define SRMENCODERPRIVATE_H

#include <SRMEncoder.h>

struct SRMEncoderStruct
{
    UInt32 id;
    SRMDevice *device;
    SRMListItem *deviceLink;
    SRMConnector *currentConnector;
    SRMList *crtcs;
};

SRMEncoder *srmEncoderCreate(SRMDevice *device, UInt32 id);
void srmEncoderDestroy(SRMEncoder *encoder);
UInt8 srmEncoderUpdateCrtcs(SRMEncoder *encoder);


#endif // SRMENCODERPRIVATE_H
