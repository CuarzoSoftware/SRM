#ifndef SRMENCODER_H
#define SRMENCODER_H

#include "SRMTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

UInt32 srmEncoderGetID(SRMEncoder *encoder);
SRMDevice *srmEncoderGetDevice(SRMEncoder *encoder);
SRMList *srmEncoderGetCrtcs(SRMEncoder *encoder);
SRMConnector *srmEncoderGetCurrentConnector(SRMEncoder *encoder);

#ifdef __cplusplus
}
#endif

#endif // SRMENCODER_H
