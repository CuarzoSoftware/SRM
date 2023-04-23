#include <private/SRMPlanePrivate.h>
#include <SRMDevice.h>
#include <stdio.h>

using namespace SRM;

SRMPlane::SRMPlanePrivate *SRMPlane::imp() const
{
    return m_imp;
}

UInt32 SRMPlane::id() const
{
    return imp()->id;
}

SRM_PLANE_TYPE SRMPlane::type() const
{
    return imp()->type;
}

std::list<SRMCrtc *> &SRMPlane::crtcs() const
{
    return imp()->crtcs;
}

SRMDevice *SRMPlane::device() const
{
    return imp()->device;
}

SRMCore *SRMPlane::core() const
{
    return device()->core();
}

SRMConnector *SRMPlane::currentConnector() const
{
    return imp()->currentConnector;
}

SRM::SRMPlane *SRMPlane::createPlane(SRMDevice *device, UInt32 id)
{
    SRMPlane *plane = new SRMPlane(device, id);

    if (!plane->imp()->updateProperties())
    {
        fprintf(stderr, "SRM Error: Could not get plane %d properties.\n", id);
        goto fail;
    }

    if (!plane->imp()->updateCrtcs())
    {
        fprintf(stderr, "SRM Error: Could not create plane %d.\n", id);
        goto fail;
    }

    return plane;

    fail:
    delete plane;
    return nullptr;
}

SRMPlane::SRMPlane(SRMDevice *device, UInt32 id)
{
    m_imp = new SRMPlanePrivate(device, this, id);
}

SRMPlane::~SRMPlane()
{
    delete m_imp;
}
