#ifndef SRMENCODER_H
#define SRMENCODER_H

#include <SRMTypes.h>

UInt32 srmEncoderGetID(SRMEncoder *encoder);
SRMDevice *srmEncoderGetDevice(SRMEncoder *encoder);
SRMList *srmEncoderGetCrtcs(SRMEncoder *encoder);
SRMConnector *srmEncoderGetCurrentConnector(SRMEncoder *encoder);

#endif // SRMENCODER_H
