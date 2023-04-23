#ifndef SRMCONNECTORPRIVATE_H
#define SRMCONNECTORPRIVATE_H

#include <SRMConnector.h>
#include <thread>
#include <gbm.h>
#include <EGL/egl.h>
#include <condition_variable>
#include <xf86drm.h>

class SRM::SRMConnector::SRMConnectorPrivate
{
public:
    SRMConnectorPrivate(SRMDevice *device, SRMConnector *connector, UInt32 id);
    ~SRMConnectorPrivate() = default;
    SRMDevice *device;
    SRMConnector *connector;
    UInt32 id, mmWidth, mmHeight;
    std::list<SRMConnector*>::iterator deviceLink;

    std::list<SRMEncoder*>encoders;
    std::list<SRMConnectorMode*>modes;

    SRMConnectorMode *preferredMode = nullptr;
    SRMConnectorMode *currentMode = nullptr;
    SRMEncoder *currentEncoder = nullptr;
    SRMCrtc *currentCrtc = nullptr;
    SRMPlane *currentPrimaryPlane = nullptr;
    SRMPlane *currentCursorPlane = nullptr;

    SRM_CONNECTOR_STATE state = SRM_CONNECTOR_STATE_UNINITIALIZED;
    bool connected = false;

    char *name = nullptr;
    char *manufacturer = nullptr;
    char *model = nullptr;
    char *serial = nullptr;

    int updateEncoders();
    int updateName();
    int updateModes();
    int updateProperties();
    int getBestConfiguration(SRMEncoder **bestEncoder, SRMCrtc **bestCrtc, SRMPlane **bestPrimaryPlane, SRMPlane **bestCursorPlane);

    SRMConnectorMode *findPreferredMode();

    SRMConnectorInterface *interface = nullptr;
    void *interfaceData = nullptr;
    std::thread *renderThread = nullptr;

    static void initRenderer(SRMConnector *connector, Int32 *initResult);
    gbm_surface *connectorGBMSurface = nullptr;
    gbm_surface *rendererGBMSurface = nullptr;
    EGLConfig connectorEGLConfig;
    EGLConfig rendererEGLConfig;
    EGLContext connectorEGLContext = EGL_NO_CONTEXT;
    EGLContext rendererEGLContext = EGL_NO_CONTEXT;
    EGLSurface connectorEGLSurface = EGL_NO_SURFACE;
    EGLSurface rendererEGLSurface = EGL_NO_SURFACE;
    UInt32 connectorDRMFramebuffers[2];
    UInt32 redererDRMFramebuffers[2];
    gbm_bo *connectorBOs[2] = {nullptr, nullptr};
    gbm_bo *rendererBOs[2] = {nullptr, nullptr};
    UInt32 currentBufferIndex = 1;

    gbm_bo *cursorBO = nullptr;
    bool repaintRequested = false;
    std::mutex renderMutex;
    std::condition_variable renderConditionVariable;
    bool pendingPageFlip = false;
    drmEventContext drmEventCtx;

    struct SRMRendererInterface
    {
        int(*createGBMSurfaces)(SRMConnector*connector);
        int(*getEGLConfiguration)(SRMConnector*connector);
        int(*createEGLContexts)(SRMConnector*connector);
        int(*createEGLSurfaces)(SRMConnector*connector);
        int(*createDRMFramebuffers)(SRMConnector*connector);
        int(*render)(SRMConnector*connector);
        int(*pageFlip)(SRMConnector*connector);
    }rendererInterface;
};

#endif // SRMCONNECTORPRIVATE_H
