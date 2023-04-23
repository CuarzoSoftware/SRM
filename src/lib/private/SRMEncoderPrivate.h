#ifndef SRMENCODERPRIVATE_H
#define SRMENCODERPRIVATE_H

#include <SRMEncoder.h>

class SRM::SRMEncoder::SRMEncoderPrivate
{
public:
    SRMEncoderPrivate(SRMDevice *device, UInt32 id, SRMEncoder *encoder);
    ~SRMEncoderPrivate() = default;
    int updateCrtcs();
    SRMDevice *device;
    SRMEncoder *encoder;
    SRMConnector *currentConnector = nullptr;
    UInt32 id;
    std::list<SRMCrtc*>crtcs;
    std::list<SRMEncoder*>::iterator deviceLink;
};

#endif // SRMENCODERPRIVATE_H
