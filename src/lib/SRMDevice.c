#include <private/SRMDevicePrivate.h>
#include <private/SRMCorePrivate.h>
#include <SRMList.h>
#include <SRMLog.h>
#include <fcntl.h>

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

UInt8 srmDeviceGetCapAsyncPageFlip(SRMDevice *device)
{
    return device->capAsyncPageFlip;
}

UInt8 srmDeviceGetCapAtomicAsyncPageFlip(SRMDevice *device)
{
    return device->capAtomicAsyncPageFlip;
}

UInt8 srmDeviceGetCapTimestampMonotonic(SRMDevice *device)
{
    return device->capTimestampMonotonic;
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
    return device->renderMode;
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

SRMList *srmDeviceGetDMATextureFormats(SRMDevice *device)
{
    return device->dmaTextureFormats;
}

SRMList *srmDeviceGetDMARenderFormats(SRMDevice *device)
{
    return device->dmaRenderFormats;
}

SRMList *srmDeviceGetDMAExternalFormats(SRMDevice *device)
{
    return device->dmaExternalFormats;
}

EGLDisplay *srmDeviceGetEGLDisplay(SRMDevice *device)
{
    return device->eglDisplay;
}

EGLContext *srmDeviceGetEGLContext(SRMDevice *device)
{
    return device->eglSharedContext;
}

const SRMEGLDeviceExtensions *srmDeviceGetEGLExtensions(SRMDevice *device)
{
    return &device->eglExtensions;
}

const SRMEGLDeviceFunctions *srmDeviceGetEGLFunctions(SRMDevice *device)
{
    return &device->eglFunctions;
}

const SRMGLDeviceExtensions *srmDeviceGetGLExtensions(SRMDevice *device)
{
    return &device->glExtensions;
}

UInt8 srmDeviceSync(SRMDevice *device)
{
    if (!device->eglFunctions.eglCreateSyncKHR)
        return 0;

    // TODO
    EGLSyncKHR sync = device->eglFunctions.eglCreateSyncKHR(device->eglDisplay, EGL_SYNC_FENCE_KHR, NULL);

    if (sync == EGL_NO_SYNC_KHR)
    {
        SRMError("Failed to create EGL sync.");
        glFinish();
    }
    else
    {
        glFlush();

        EGLint res = device->eglFunctions.eglClientWaitSyncKHR(device->eglDisplay, sync, 0, EGL_FOREVER_KHR);
        device->eglFunctions.eglDestroySyncKHR(device->eglDisplay, sync);

        if (res != EGL_CONDITION_SATISFIED_KHR)
        {
            SRMError("EGL SYNC CONDITION NOT SATISFIED");
            return 0;
        }
    }

    return 1;
}
