#include <private/SRMDevicePrivate.h>
#include <private/SRMCorePrivate.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>

using namespace SRM;

SRMDevice *SRMDevice::createDevice(SRMCore *core, const char *path)
{
    SRMDevice *device = new SRMDevice(core, path);

#ifndef SRM_CONFIG_TESTS
    device->imp()->fd = device->core()->imp()->interface->openRestricted(path,
                                                                         O_RDWR | O_CLOEXEC,
                                                                         device->core()->imp()->userdata);

    if (device->imp()->fd < 0)
    {
        fprintf(stderr, "SRM Error: Failed to open DRM device %s in RDONLY mode.\n", path);
        goto fail;
    }
#endif

    if (!device->imp()->initializeGBM())
    {
        fprintf(stderr, "SRM Error: Failed to create device %s GBM.\n", path);
        goto fail;
    }

    if (!device->imp()->initializeEGL())
    {
        fprintf(stderr, "SRM Error: Failed to create device %s EGL Display.\n", path);
        goto fail;
    }

    if (!device->imp()->updateClientCaps())
    {
        fprintf(stderr, "SRM Error: Failed to get device %s client caps.\n", path);
        goto fail;
    }

    if (!device->imp()->updateCaps())
    {
        fprintf(stderr, "SRM Error: Failed to get device %s caps.\n", path);
        delete device;
        return nullptr;
    }

    if (!device->imp()->updateCrtcs())
    {
        fprintf(stderr, "SRM Error: Failed to get device %s crtcs.\n", path);
        goto fail;
    }

    if (!device->imp()->updateEncoders())
    {
        fprintf(stderr, "SRM Error: Failed to get device %s encoders.\n", path);
        goto fail;
    }

    if (!device->imp()->updatePlanes())
    {
        fprintf(stderr, "SRM Error: Failed to get device %s planes.\n", path);
        goto fail;
    }

    if (!device->imp()->updateConnectors())
    {
        fprintf(stderr, "SRM Error: Failed to get device %s connectors.\n", path);
        goto fail;
    }

    return device;

    fail:
    delete device;
    return nullptr;
}

SRMDevice::~SRMDevice()
{
    if (imp()->name)
        delete[] imp()->name;

    if (imp()->fd >= 0)
        core()->imp()->interface->closeRestricted(imp()->fd, core()->imp()->userdata);

    delete m_imp;
}

SRMCore *SRMDevice::core() const
{
    return imp()->core;
}

const char *SRMDevice::name() const
{
    return imp()->name;
}

int SRMDevice::fd() const
{
    return imp()->fd;
}

bool SRMDevice::clientCapStereo3D() const
{
    return imp()->clientCapStereo3D;
}

bool SRMDevice::clientCapUniversalPlanes() const
{
    return imp()->clientCapUniversalPlanes;
}

bool SRMDevice::clientCapAtomic() const
{
    return imp()->clientCapAtomic;
}

bool SRMDevice::clientCapAspectRatio() const
{
    return imp()->clientCapAspectRatio;
}

bool SRMDevice::clientCapWritebackConnectors() const
{
    return imp()->clientCapWritebackConnectors;
}

bool SRMDevice::capDumbBuffer() const
{
    return imp()->capDumbBuffer;
}

bool SRMDevice::capPrimeImport() const
{
    return imp()->capPrimeImport;
}

bool SRMDevice::capPrimeExport() const
{
    return imp()->capPrimeExport;
}

bool SRMDevice::capAddFb2Modifiers() const
{
    return imp()->capAddFb2Modifiers;
}

int SRMDevice::setEnabled(bool enabled)
{
    // If it is the only GPU it can not be disabled
    if (!enabled && core()->devices().size() == 1)
    {
        fprintf(stderr, "SRM Error: Can not disable device. There must be at least one enabled device.\n");
        return 0;
    }
    imp()->enabled = enabled;

    // TODO update SRM config

    return 1;
}

bool SRMDevice::enabled() const
{
    return imp()->enabled;
}

bool SRMDevice::isRenderer() const
{
    // The device is renderer if it relies on itself to render
    return imp()->rendererDevice == this;
}

SRM::SRMDevice *SRMDevice::rendererDevice() const
{
    return imp()->rendererDevice;
}

SRM_RENDER_MODE SRMDevice::renderMode() const
{
    return SRM_RENDER_MODE_DUMB;

    if (this == rendererDevice())
        return SRM_RENDER_MODE_ITSELF;
    else if (capDumbBuffer())
        return SRM_RENDER_MODE_DUMB;
    else
        return SRM_RENDER_MODE_CPU;
}

std::list<SRMCrtc *> &SRMDevice::crtcs() const
{
    return imp()->crtcs;
}

std::list<SRMEncoder *> &SRMDevice::encoders() const
{
    return imp()->encoders;
}

std::list<SRMPlane *> &SRMDevice::planes() const
{
    return imp()->planes;
}

std::list<SRMConnector *> &SRMDevice::connectors() const
{
    return imp()->connectors;
}

SRMDevice::SRMDevicePrivate *SRMDevice::imp() const
{
    return m_imp;
}

SRMDevice::SRMDevice(SRMCore *core, const char *path)
{
    m_imp = new SRMDevicePrivate(this);
    m_imp->core = core;

    int len = strlen(path);
    imp()->name = new char[len+1];
    memcpy(imp()->name, path, len);
    imp()->name[len] = '\0';
}
