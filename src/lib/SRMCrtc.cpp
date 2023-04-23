#include <private/SRMCrtcPrivate.h>
#include <stdio.h>

using namespace SRM;

SRMCrtc::SRMCrtcPrivate *SRMCrtc::imp() const
{
    return m_imp;
}

UInt32 SRMCrtc::id() const
{
    return imp()->id;
}

SRMConnector *SRMCrtc::currentConnector() const
{
    return imp()->currentConnector;
}

SRMCrtc *SRMCrtc::createCrtc(SRMDevice *device, UInt32 id)
{
    SRMCrtc *crtc = new SRMCrtc(device, id);

    if (!crtc->imp()->updateProperties())
    {
        fprintf(stderr, "SRM Error: Could not create CRTC\n");
        delete crtc;
        return nullptr;
    }

    return crtc;
}

SRMCrtc::SRMCrtc(SRMDevice *device, UInt32 id)
{
    m_imp = new SRMCrtcPrivate(this);
    imp()->device = device;
    imp()->id = id;
}

SRMCrtc::~SRMCrtc()
{
    delete m_imp;
}
