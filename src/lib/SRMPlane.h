#ifndef SRMPLANE_H
#define SRMPLANE_H

#include <SRMNamespaces.h>

class SRM::SRMPlane
{
public:
    class SRMPlanePrivate;
    SRMPlanePrivate *imp() const;
    UInt32 id() const;
    SRM_PLANE_TYPE type() const;
    std::list<SRMCrtc*>&crtcs() const;
    SRMDevice *device() const;
    SRMCore *core() const;
    SRMConnector *currentConnector() const;
private:
    friend class SRMDevice;
    static SRMPlane *createPlane(SRMDevice *device, UInt32 id);
    SRMPlane(SRMDevice *device, UInt32 id);
    ~SRMPlane();

    SRMPlanePrivate * m_imp = nullptr;
};

#endif // SRMPLANE_H
