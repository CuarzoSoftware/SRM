#ifndef SRMENCODER_H
#define SRMENCODER_H

#include <SRMNamespaces.h>

class SRM::SRMEncoder
{
public:

    class SRMEncoderPrivate;
    SRMEncoderPrivate *imp() const;
    UInt32 id() const;
    std::list<SRMCrtc*>&crtcs() const;
    SRMDevice *device() const;
    SRMCore *core() const;
    SRMConnector *currentConnector() const;
private:
    friend class SRMDevice;
    static SRMEncoder *createEncoder(SRMDevice *device, UInt32 id);
    SRMEncoder(SRMDevice *device, UInt32 id);
    ~SRMEncoder();
    SRMEncoderPrivate *m_imp = nullptr;

};

#endif // SRMENCODER_H
