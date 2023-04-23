#ifndef SRMDEVICEPRIVATE_H
#define SRMDEVICEPRIVATE_H

#include <SRMDevice.h>
#include <gbm.h>
#include <EGL/egl.h>
#include <mutex>

using namespace SRM;

class SRMDevice::SRMDevicePrivate
{
public:
    SRMDevicePrivate(SRMDevice *device);
    ~SRMDevicePrivate() = default;

    SRMCore *core = nullptr;
    SRMDevice *device = nullptr;

    // GBM
    int initializeGBM();
    void uninitializeGBM();
    gbm_device *gbm = nullptr;

    // EGL
    int initializeEGL();
    void uninitializeEGL();
    EGLDisplay eglDisplay = EGL_NO_DISPLAY;
    EGLContext eglSharedContext = EGL_NO_CONTEXT;

    // Client caps
    int updateClientCaps();
    bool clientCapStereo3D = false;
    bool clientCapUniversalPlanes = false;
    bool clientCapAtomic = false;
    bool clientCapAspectRatio = false;
    bool clientCapWritebackConnectors = false;

    // Caps
    int updateCaps();
    bool capDumbBuffer = false;
    bool capPrimeImport = false;
    bool capPrimeExport = false;
    bool capAddFb2Modifiers = false;

    // Crtcs
    int updateCrtcs();
    std::list<SRMCrtc*>crtcs;

    // Encoders
    int updateEncoders();
    std::list<SRMEncoder*>encoders;

    // Planes
    int updatePlanes();
    std::list<SRMPlane*>planes;

    // Connectors
    int updateConnectors();
    std::list<SRMConnector*>connectors;

    // All GPUs are enabled by default
    bool enabled = true;

    // Renderer device could be itself or another if PIRME IMPORT not supported
    SRMDevice *rendererDevice = nullptr;

    // Page flipping mutex
    std::mutex pageFlipMutex;

    int fd = -1;
    std::list<SRMDevice*>::iterator coreLink;
    char *name = nullptr;
};

#endif // SRMDEVICEPRIVATE_H
