#include <private/SRMCorePrivate.h>
#include <private/SRMDevicePrivate.h>
#include <private/SRMCrtcPrivate.h>
#include <private/SRMEncoderPrivate.h>
#include <private/SRMPlanePrivate.h>
#include <private/SRMConnectorPrivate.h>

#include <SRMList.h>
#include <SRMLog.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

/*

void SRMDevice::SRMDevicePrivate::uninitializeGBM()
{
    if (gbm)
        gbm_device_destroy(gbm);
}



void SRMDevice::SRMDevicePrivate::uninitializeEGL()
{
    if (eglDisplay != EGL_NO_DISPLAY)
        eglTerminate(eglDisplay);
}

*/

SRMDevice *srmDeviceCreate(SRMCore *core, const char *name)
{
    SRMDevice *device = calloc(1, sizeof(SRMDevice));

    Int32 len = strlen(name);
    device->name = malloc(len);
    memcpy(device->name, name, len);

    device->core = core;
    device->enabled = 1;


    device->fd = core->interface->openRestricted(name,
                                                 O_RDWR | O_CLOEXEC,
                                                 core->interfaceUserData);

    if (pthread_mutex_init(&device->pageFlipMutex, NULL))
    {
        SRMError("Failed to create page flip mutex for device %s.", device->name);
        goto fail;
    }

    if (device->fd < 0)
    {
        SRMError("Failed to open DRM device %s.", name);
        goto fail;
    }

    if (!srmDeviceInitializeGBM(device))
        goto fail;

    if (!srmDeviceInitializeEGL(device))
        goto fail;

    if (!srmDeviceUpdateClientCaps(device))
        goto fail;

    if (!srmDeviceUpdateCaps(device))
        goto fail;

    device->crtcs = srmListCreate();
    if (!srmDeviceUpdateCrtcs(device))
        goto fail;

    device->encoders = srmListCreate();
    if (!srmDeviceUpdateEncoders(device))
        goto fail;

    device->planes = srmListCreate();
    if (!srmDeviceUpdatePlanes(device))
        goto fail;

    device->connectors = srmListCreate();
    if (!srmDeviceUpdateConnectors(device))
        goto fail;

    return device;

    fail:
    srmDeviceDestroy(device);
    return NULL;
}

void srmDeviceDestroy(SRMDevice *device)
{
    pthread_mutex_destroy(&device->pageFlipMutex);

    // TODO free resources
    free(device);
}

UInt8 srmDeviceInitializeGBM(SRMDevice *device)
{
    device->gbm = gbm_create_device(device->fd);

    if (!device->gbm)
    {
        SRMError("Failed to initialize GBM on device %s.", device->name);
        return 0;
    }

    return 1;
}

UInt8 srmDeviceInitializeEGL(SRMDevice *device)
{
    device->eglDisplay = eglGetDisplay(device->gbm);

    if (device->eglDisplay == EGL_NO_DISPLAY)
    {
        SRMError("Failed to get EGL display for device %s.", device->name);
        return 0;
    }

    if (!eglInitialize(device->eglDisplay, NULL, NULL))
    {
        SRMError("Failed to initialize EGL display for device %s.", device->name);
        device->eglDisplay = EGL_NO_DISPLAY;
        return 0;
    }

    return 1;
}

UInt8 srmDeviceUpdateClientCaps(SRMDevice *device)
{
    device->clientCapStereo3D            = drmSetClientCap(device->fd, DRM_CLIENT_CAP_STEREO_3D, 1)             == 0;
    device->clientCapUniversalPlanes     = drmSetClientCap(device->fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1)      == 0;
    device->clientCapAtomic              = drmSetClientCap(device->fd, DRM_CLIENT_CAP_ATOMIC, 1)                == 0;
    device->clientCapAspectRatio         = drmSetClientCap(device->fd, DRM_CLIENT_CAP_ASPECT_RATIO, 1)          == 0;
    device->clientCapWritebackConnectors = drmSetClientCap(device->fd, DRM_CLIENT_CAP_WRITEBACK_CONNECTORS, 1)  == 0;
    return 1;
}

UInt8 srmDeviceUpdateCaps(SRMDevice *device)
{
    UInt64 value;
    drmGetCap(device->fd, DRM_CAP_DUMB_BUFFER, &value);
    device->capDumbBuffer = value == 1;

    drmGetCap(device->fd, DRM_CAP_PRIME, &value);
    device->capPrimeImport = value & DRM_PRIME_CAP_IMPORT;
    device->capPrimeExport = value & DRM_PRIME_CAP_EXPORT;

    // Validate EXPORT cap
    if (device->capPrimeExport)
    {
        bool primeExport = false;

        struct gbm_bo *bo = gbm_bo_create(device->gbm, 256, 256, GBM_FORMAT_ARGB8888, GBM_BO_USE_LINEAR);

        if (bo)
        {
            int dma = gbm_bo_get_fd(bo);

            if (dma != -1)
            {
                primeExport = true;
                close(dma);
            }

            gbm_bo_destroy(bo);
        }

        device->capPrimeExport = primeExport;
    }


    drmGetCap(device->fd, DRM_CAP_ADDFB2_MODIFIERS, &value);
    device->capAddFb2Modifiers = value == 1;

    return 1;
}

UInt8 srmDeviceUpdateCrtcs(SRMDevice *device)
{
    drmModeRes *res = drmModeGetResources(device->fd);

    if (!res)
    {
        SRMError("Could not get device %s resources.", device->name);
        return 0;
    }

    SRMCrtc *crtc;

    for (int i = 0; i < res->count_crtcs; i++)
    {
        crtc = srmCrtcCreate(device, res->crtcs[i]);

        if (crtc)
            crtc->deviceLink = srmListAppendData(device->crtcs, crtc);
    }

    drmModeFreeResources(res);

    if (srmListIsEmpty(device->crtcs))
    {
        SRMError("SRM Error: No crtc found for device %s.", device->name);
        return 0;
    }

    return 1;
}


UInt8 srmDeviceUpdateEncoders(SRMDevice *device)
{
    drmModeRes *res = drmModeGetResources(device->fd);

    if (!res)
    {
        SRMError("Could not get device %s resources.", device->name);
        return 0;
    }

    SRMEncoder *encoder;

    for (int i = 0; i < res->count_encoders; i++)
    {
        encoder = srmEncoderCreate(device, res->encoders[i]);

        if (encoder)
            encoder->deviceLink = srmListAppendData(device->encoders, encoder);
    }

    drmModeFreeResources(res);

    if (srmListIsEmpty(device->encoders))
    {
        SRMError("No encoder found for device %s.", device->name);
        return 0;
    }

    return 1;
}

UInt8 srmDeviceUpdatePlanes(SRMDevice *device)
{
    drmModePlaneRes *planeRes = drmModeGetPlaneResources(device->fd);

    if (!planeRes)
    {
        SRMError("Could not get device %s plane resources.", device->name);
        return 0;
    }

    SRMPlane *plane;

    for (UInt32 i = 0; i < planeRes->count_planes; i++)
    {
        plane = srmPlaneCreate(device, planeRes->planes[i]);

        if (plane)
            plane->deviceLink = srmListAppendData(device->planes, plane);
    }

    drmModeFreePlaneResources(planeRes);

    return 1;
}

UInt8 srmDeviceUpdateConnectors(SRMDevice *device)
{
    drmModeRes *res = drmModeGetResources(device->fd);

    if (!res)
    {
        SRMError("Could not get device %s resources.", device->name);
        return 0;
    }

    SRMConnector *connector;

    for (int i = 0; i < res->count_connectors; i++)
    {
        connector = srmConnectorCreate(device, res->connectors[i]);

        if (connector)
            connector->deviceLink = srmListAppendData(device->connectors, connector);
    }

    drmModeFreeResources(res);

    if (srmListIsEmpty(device->connectors))
    {
        SRMError("No connector found for device %s.", device->name);
        return 0;
    }

    return 1;
}
