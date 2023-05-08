#ifndef SRMCONNECTORPRIVATE_H
#define SRMCONNECTORPRIVATE_H

#include <GLES2/gl2.h>
#include <SRMConnector.h>
#include <gbm.h>
#include <EGL/egl.h>
#include <xf86drm.h>

/*
class SRM::SRMConnector::SRMConnectorPrivate
{
public:
    SRMConnectorPrivate(SRMDevice *device, SRMConnector *connector, UInt32 id);
    ~SRMConnectorPrivate() = default;

    int updateEncoders();
    int updateName();
    int updateModes();
    int updateProperties();
    int getBestConfiguration(SRMEncoder **bestEncoder, SRMCrtc **bestCrtc, SRMPlane **bestPrimaryPlane, SRMPlane **bestCursorPlane);

    SRMConnectorMode *findPreferredMode();

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

    GLuint dumbRendererFBO;
    GLuint dumbRenderbuffer;

    bool repaintRequested = false;
    std::mutex renderMutex;
    std::condition_variable renderConditionVariable;
    drm_mode_create_dumb dumbBuffers[2];
    UInt8 *dumbMaps[2];

    struct SRMRendererInterface
    {
        int(*createGBMSurfaces)(SRMConnector*connector);
        int(*getEGLConfiguration)(SRMConnector*connector);
        int(*createEGLContexts)(SRMConnector*connector);
        int(*createEGLSurfaces)(SRMConnector*connector);
        int(*createDRMFramebuffers)(SRMConnector*connector);
        int(*initCrtc)(SRMConnector*connector);
        int(*render)(SRMConnector*connector);
        int(*pageFlip)(SRMConnector*connector);
    }rendererInterface;


};
*/

struct SRMConnectorRenderInterface
{
    UInt8(*initialize)(SRMConnector *connector);
    UInt8(*render)(SRMConnector *connector);
    UInt8(*flipPage)(SRMConnector *connector);
    UInt8(*updateMode)(SRMConnector *connector);
    void (*uninitialize)(SRMConnector *connector);
};

struct SRMConnectorPropIDs
{
    UInt32
    CRTC_ID,
    DPMS,
    EDID,
    PATH,
    link_status,
    non_desktop,
    panel_orientation,
    subconnector,
    vrr_capable;
};

struct SRMConnectorStruct
{
    UInt32 id;
    UInt32 nameID; // Used to name the connector e.g HDMI-A-<0>
    UInt32 type;
    SRMDevice *device;
    SRMListItem *deviceLink;
    struct SRMConnectorPropIDs propIDs;
    SRMList *encoders, *modes;
    UInt32 mmWidth, mmHeight;
    SRMConnectorMode *preferredMode, *currentMode,
    *targetMode; // This one is used while changing mode

    SRMEncoder *currentEncoder;
    SRMCrtc *currentCrtc;
    SRMPlane *currentPrimaryPlane, *currentCursorPlane;
    SRM_CONNECTOR_STATE state;

    // Used to
    Int8 renderInitResult;
    UInt8 connected;
    char *name, *manufacturer, *model, *serial;

    // Cursor
    struct gbm_bo *cursorBO;

    // Interface for OpenGL events
    SRMConnectorInterface *interface;
    void *interfaceData;
    pthread_t renderThread;

    // Render common
    drmEventContext drmEventCtx;
    UInt8 pendingPageFlip;
    pthread_cond_t repaintCond;
    pthread_mutex_t repaintMutex;
    UInt8 repaintRequested;

    // Render specific
    struct SRMConnectorRenderInterface renderInterface;
    void *renderData;


};

SRMConnector *srmConnectorCreate(SRMDevice *device, UInt32 id);
UInt8 srmConnectorUpdateProperties(SRMConnector *connector);

UInt8 srmConnectorUpdateNames(SRMConnector *connector);
void srmConnectorDestroyNames(SRMConnector *connector);

UInt8 srmConnectorUpdateEncoders(SRMConnector *connector);
void srmConnectorDestroyEncoders(SRMConnector *connector);

UInt8 srmConnectorUpdateModes(SRMConnector *connector);
void srmConnectorDestroyModes(SRMConnector *connector);

SRMConnectorMode *srmConnectorFindPreferredMode(SRMConnector *connector);
UInt8 srmConnectorGetBestConfiguration(SRMConnector *connector, SRMEncoder **bestEncoder, SRMCrtc **bestCrtc, SRMPlane **bestPrimaryPlane, SRMPlane **bestCursorPlane);
void *srmConnectorRenderThread(void *conn);
void srmConnectorUnlockRenderThread(SRMConnector *connector);
void srmConnectorDestroy(SRMConnector *connector);

#endif // SRMCONNECTORPRIVATE_H
