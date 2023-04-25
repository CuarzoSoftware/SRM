#include <private/SRMDevicePrivate.h>
#include <xf86drmMode.h>
#include <xf86drm.h>
#include <drm.h>
#include <stdio.h>
#include <unistd.h>
#include <private/SRMCrtcPrivate.h>
#include <private/SRMEncoderPrivate.h>
#include <private/SRMPlanePrivate.h>
#include <private/SRMConnectorPrivate.h>

#ifdef SRM_CONFIG_TESTS
    #include <random>
#endif

SRMDevice::SRMDevicePrivate::SRMDevicePrivate(SRMDevice *device)
{
    this->device = device;
}

int SRMDevice::SRMDevicePrivate::initializeGBM()
{
    gbm = gbm_create_device(fd);
    return gbm != nullptr;
}

void SRMDevice::SRMDevicePrivate::uninitializeGBM()
{
    if (gbm)
        gbm_device_destroy(gbm);
}

int SRMDevice::SRMDevicePrivate::initializeEGL()
{
    eglDisplay = eglGetDisplay(gbm);

    if (eglDisplay == EGL_NO_DISPLAY)
        return 0;

    if (!eglInitialize(eglDisplay, NULL, NULL))
    {
        eglDisplay = EGL_NO_DISPLAY;
        return 0;
    }

    return 1;
}

void SRMDevice::SRMDevicePrivate::uninitializeEGL()
{
    if (eglDisplay != EGL_NO_DISPLAY)
        eglTerminate(eglDisplay);
}

int SRMDevice::SRMDevicePrivate::updateClientCaps()
{
    #ifdef SRM_CONFIG_TESTS
        clientCapStereo3D = rand() % 2;
        clientCapUniversalPlanes = rand() % 2;
        clientCapAtomic = rand() % 2;
        clientCapAspectRatio = rand() % 2;
        clientCapWritebackConnectors = rand() % 2;
        return 1;
    #endif

    clientCapStereo3D = drmSetClientCap(fd, DRM_CLIENT_CAP_STEREO_3D, 1) == 0;
    clientCapUniversalPlanes = drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1) == 0;
    clientCapAtomic = drmSetClientCap(fd, DRM_CLIENT_CAP_ATOMIC, 1) == 0;
    clientCapAspectRatio = drmSetClientCap(fd, DRM_CLIENT_CAP_ASPECT_RATIO, 1) == 0;
    clientCapWritebackConnectors = drmSetClientCap(fd, DRM_CLIENT_CAP_WRITEBACK_CONNECTORS, 1) == 0;

    return 1;
}

int SRMDevice::SRMDevicePrivate::updateCaps()
{

    #ifdef SRM_CONFIG_TESTS
        capDumbBuffer = rand() % 2;
        capPrimeImport = rand() % 2;
        capPrimeExport = rand() % 2;
        capAddFb2Modifiers = rand() % 2;
        return 1;
    #endif

    UInt64 value;
    drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &value);
    capDumbBuffer = value == 1;

    drmGetCap(fd, DRM_CAP_PRIME, &value);
    capPrimeImport = value & DRM_PRIME_CAP_IMPORT;
    capPrimeExport = value & DRM_PRIME_CAP_EXPORT;

    // Validate EXPORT cap
    if (capPrimeExport)
    {
        bool primeExport = false;

        gbm_bo *bo = gbm_bo_create(gbm, 256, 256, GBM_FORMAT_ARGB8888, GBM_BO_USE_LINEAR);

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

        capPrimeExport = primeExport;
    }


    drmGetCap(fd, DRM_CAP_ADDFB2_MODIFIERS, &value);
    capAddFb2Modifiers = value == 1;

    return 1;
}

int SRMDevice::SRMDevicePrivate::updateCrtcs()
{
    drmModeRes *res = drmModeGetResources(fd);

    if (!res)
    {
        fprintf(stderr, "SRM Error: Could not get device resources.\n");
        return 0;
    }

    SRMCrtc *crtc;

    for (int i = 0; i < res->count_crtcs; i++)
    {
        crtc = SRMCrtc::createCrtc(device, res->crtcs[i]);

        if (crtc)
        {
            crtcs.push_back(crtc);
            crtc->imp()->deviceLink = std::prev(crtcs.end());
        }
    }

    drmModeFreeResources(res);

    if (crtcs.empty())
    {
        fprintf(stderr, "SRM Error: No crtc found for device %s.\n", device->name());
        return 0;
    }

    return 1;
}

int SRMDevice::SRMDevicePrivate::updateEncoders()
{
    drmModeRes *res = drmModeGetResources(fd);

    if (!res)
    {
        fprintf(stderr, "SRM Error: Could not get device resources.\n");
        return 0;
    }

    SRMEncoder *encoder;

    for (int i = 0; i < res->count_encoders; i++)
    {
        encoder = SRMEncoder::createEncoder(device, res->encoders[i]);

        if (encoder)
        {
            encoders.push_back(encoder);
            encoder->imp()->deviceLink = std::prev(encoders.end());
        }
    }

    drmModeFreeResources(res);

    if (encoders.empty())
    {
        fprintf(stderr, "SRM Error: No encoder found for device %s.\n", device->name());
        return 0;
    }

    return 1;
}

int SRMDevice::SRMDevicePrivate::updatePlanes()
{
    drmModePlaneRes *planeRes = drmModeGetPlaneResources(fd);

    if (!planeRes)
    {
        fprintf(stderr, "SRM Error: Could not get plane resources.\n");
        return 0;
    }

    SRMPlane *plane;

    for (UInt32 i = 0; i < planeRes->count_planes; i++)
    {
        plane = SRMPlane::createPlane(device, planeRes->planes[i]);

        if (plane)
        {
            planes.push_back(plane);
            plane->imp()->deviceLink = std::prev(planes.end());
        }
    }

    drmModeFreePlaneResources(planeRes);

    return 1;
}

int SRMDevice::SRMDevicePrivate::updateConnectors()
{
    drmModeRes *res = drmModeGetResources(fd);

    if (!res)
    {
        fprintf(stderr, "SRM Error: Could not get device resources.\n");
        return 0;
    }

    SRMConnector *connector;

    for (int i = 0; i < res->count_connectors; i++)
    {
        connector = SRMConnector::createConnector(device, res->connectors[i]);

        if (connector)
        {
            connectors.push_back(connector);
            connector->imp()->deviceLink = std::prev(connectors.end());
        }
    }

    drmModeFreeResources(res);

    if (connectors.empty())
    {
        fprintf(stderr, "SRM Error: No connector found for device %s.\n", device->name());
        return 0;
    }

    return 1;
}


