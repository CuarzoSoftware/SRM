#include <private/SRMDevicePrivate.h>
#include <private/SRMCorePrivate.h>
#include <SRMList.h>
#include <SRMLog.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>

/*

SRMDevice::~SRMDevice()
{
    if (imp()->name)
        delete[] imp()->name;

    if (imp()->fd >= 0)
        core()->imp()->interface->closeRestricted(imp()->fd, core()->imp()->userdata);

    delete m_imp;
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

SRMDevice::SRMDevice(SRMCore *core, const char *path)
{
    m_imp = new SRMDevicePrivate(this);
    m_imp->core = core;

    int len = strlen(path);
    imp()->name = new char[len+1];
    memcpy(imp()->name, path, len);
    imp()->name[len] = '\0';
}

*/

const char *srmDeviceGetName(SRMDevice *device)
{
    return device->name;
}

SRMCore *srmDeviceGetCore(SRMDevice *device)
{
    return device->core;
}

Int32 srmDeviceGetFD(SRMDevice *device)
{
    return device->fd;
}

UInt8 srmDeviceGetClientCapStereo3D(SRMDevice *device)
{
    return device->clientCapStereo3D;
}

UInt8 srmDeviceGetClientCapUniversalPlanes(SRMDevice *device)
{
    return device->clientCapUniversalPlanes;
}

UInt8 srmDeviceGetClientCapAtomic(SRMDevice *device)
{
    return device->clientCapAtomic;
}

UInt8 srmDeviceGetClientCapAspectRatio(SRMDevice *device)
{
    return device->clientCapAspectRatio;
}

UInt8 srmDeviceGetClientCapWritebackConnectors(SRMDevice *device)
{
    return device->clientCapWritebackConnectors;
}

UInt8 srmDeviceGetCapDumbBuffer(SRMDevice *device)
{
    return device->capDumbBuffer;
}

UInt8 srmDeviceGetCapPrimeImport(SRMDevice *device)
{
    return device->capPrimeImport;
}

UInt8 srmDeviceGetCapPrimeExport(SRMDevice *device)
{
    return device->capPrimeExport;
}

UInt8 srmDeviceGetCapAddFb2Modifiers(SRMDevice *device)
{
    return device->capAddFb2Modifiers;
}

UInt8 srmDeviceSetEnabled(SRMDevice *device, UInt8 enabled)
{
    // If it is the only GPU it can not be disabled
    if (!enabled && srmListGetLength(device->core->devices) == 1)
    {
        SRMError("Can not disable device. There must be at least one enabled device.");
        return 0;
    }

    device->enabled = enabled;

    // TODO update SRM config

    return 1;
}

UInt8 srmDeviceIsEnabled(SRMDevice *device)
{
    return device->enabled;
}

UInt8 srmDeviceIsRenderer(SRMDevice *device)
{
    // The device is renderer if it relies on itself to render
    return device->rendererDevice == device;
}

SRMDevice *srmDeviceGetRendererDevice(SRMDevice *device)
{
    return device->rendererDevice;
}

SRM_RENDER_MODE srmDeviceGetRenderMode(SRMDevice *device)
{
    if (device == device->rendererDevice)
        return SRM_RENDER_MODE_ITSELF;
    else if (device->capDumbBuffer)
        return SRM_RENDER_MODE_DUMB;
    else
        return SRM_RENDER_MODE_CPU;
}

SRMList *srmDeviceGetCrtcs(SRMDevice *device)
{
    return device->crtcs;
}

SRMList *srmDeviceGetEncoders(SRMDevice *device)
{
    return device->encoders;
}

SRMList *srmDeviceGetPlanes(SRMDevice *device)
{
    return device->planes;
}

SRMList *srmDeviceGetConnectors(SRMDevice *device)
{
    return device->connectors;
}
