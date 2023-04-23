#ifndef SRMDEVICE_H
#define SRMDEVICE_H

#include <SRMNamespaces.h>

class SRM::SRMDevice
{
public:
    static SRMDevice *createDevice(SRMCore *core, const char *path);
    ~SRMDevice();

    SRMCore *core() const;

    const char *name() const;
    int fd() const;

    // Client caps
    bool clientCapStereo3D() const;
    bool clientCapUniversalPlanes() const;
    bool clientCapAtomic() const;
    bool clientCapAspectRatio() const;
    bool clientCapWritebackConnectors() const;

    // Caps
    bool capDumbBuffer() const;
    bool capPrimeImport() const;
    bool capPrimeExport() const;
    bool capAddFb2Modifiers() const;

    // Enables or disables the GPU
    int setEnabled(bool enabled);
    bool enabled() const;
    bool isRenderer() const;
    SRMDevice *rendererDevice() const;
    SRM_RENDER_MODE renderMode() const;

    std::list<SRMCrtc*>&crtcs() const;
    std::list<SRMEncoder*>&encoders() const;
    std::list<SRMPlane*>&planes() const;
    std::list<SRMConnector*>&connectors() const;

    class SRMDevicePrivate;
    SRMDevicePrivate *imp() const;

private:
    SRMDevice(SRMCore *core, const char *path);
    SRMDevicePrivate *m_imp = nullptr;

};

#endif // SRMDEVICE_H
