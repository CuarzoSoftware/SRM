#ifndef SRMCRTCPRIVATE_H
#define SRMCRTCPRIVATE_H

#include <SRMCrtc.h>

class SRM::SRMCrtc::SRMCrtcPrivate
{
public:
    SRMCrtcPrivate(SRMCrtc *crtc);
    ~SRMCrtcPrivate() = default;
    UInt32 id;
    SRMDevice *device = nullptr;
    SRMCrtc *crtc = nullptr;
    SRMConnector *currentConnector = nullptr;
    std::list<SRMCrtc*>::iterator deviceLink;
    int updateProperties();
};

#endif // SRMCRTCPRIVATE_H
