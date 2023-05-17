#ifndef SRMENCODERPRIVATE_H
#define SRMENCODERPRIVATE_H

#include "../SRMEncoder.h"

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif // SRMENCODERPRIVATE_H
