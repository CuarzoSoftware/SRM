#include <private/SRMEncoderPrivate.h>
#include <stdio.h>
#include <SRMDevice.h>

using namespace SRM;

SRMEncoder::SRMEncoderPrivate *SRM::SRMEncoder::imp() const
{
    return m_imp;
}

UInt32 SRMEncoder::id() const
{
    return imp()->id;
}

std::list<SRMCrtc *> &SRMEncoder::crtcs() const
{
    return imp()->crtcs;
}

SRMDevice *SRMEncoder::device() const
{
    return imp()->device;
}

SRMCore *SRMEncoder::core() const
{
    return device()->core();
}

SRMConnector *SRMEncoder::currentConnector() const
{
    return imp()->currentConnector;
}

SRMEncoder *SRM::SRMEncoder::createEncoder(SRMDevice *device, UInt32 id)
{
    SRMEncoder *encoder = new SRMEncoder(device, id);

    if (!encoder->imp()->updateCrtcs())
    {
        fprintf(stderr, "SRM Error: Failed to update encoder %d crtcs.\n", id);
        delete encoder;
        return nullptr;
    }

    return encoder;
}

SRMEncoder::SRMEncoder(SRMDevice *device, UInt32 id)
{
    m_imp = new SRMEncoderPrivate(device, id, this);
}

SRMEncoder::~SRMEncoder()
{
    delete m_imp;
}
