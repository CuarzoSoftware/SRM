#ifndef SRMCRTC_H
#define SRMCRTC_H

#include <SRMNamespaces.h>

class SRM::SRMCrtc
{
public:
    class SRMCrtcPrivate;
    SRMCrtcPrivate *imp() const;
    UInt32 id() const;
    SRMConnector *currentConnector() const;
private:
    friend class SRMDevice;
    static SRMCrtc *createCrtc(SRMDevice *device, UInt32 id);
    SRMCrtc(SRMDevice *device, UInt32 id);
    ~SRMCrtc();
    SRMCrtcPrivate *m_imp = nullptr;
};

#endif // SRMCRTC_H
