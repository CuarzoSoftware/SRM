#ifndef SRMCONNECTORPRIVATE_H
#define SRMCONNECTORPRIVATE_H

#include <SRMConnector.h>
#include <SRMFormat.h>
#include <GLES2/gl2.h>
#include <gbm.h>
#include <EGL/egl.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#ifdef __cplusplus
extern "C" {
#endif

struct SRMConnectorRenderInterface
{
    UInt8(*initialize)(SRMConnector *connector);
    UInt8(*render)(SRMConnector *connector);
    UInt8(*flipPage)(SRMConnector *connector);
    UInt8(*updateMode)(SRMConnector *connector);
    UInt32(*getCurrentBufferIndex)(SRMConnector *connector);
    UInt32(*getBuffersCount)(SRMConnector *connector);
    SRMBuffer*(*getBuffer)(SRMConnector *connector, UInt32 bufferIndex);
    void (*uninitialize)(SRMConnector *connector);
    void (*pause)(SRMConnector *connector);
    void (*resume)(SRMConnector *connector);
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
    void *userData;
    UInt32 id;
    UInt32 nameID; // Used to name the connector e.g HDMI-A-<0>
    UInt32 type;
    SRMDevice *device;
    SRMListItem *deviceLink;
    struct SRMConnectorPropIDs propIDs;
    SRMList *encoders, *modes;
    UInt32 mmWidth, mmHeight;
    SRM_CONNECTOR_SUBPIXEL subpixel;
    SRMConnectorMode *preferredMode, *currentMode,
    *targetMode; // This one is used while changing mode
    UInt32 currentModeBlobId;

    SRMEncoder *currentEncoder;
    SRMCrtc *currentCrtc;
    SRMPlane *currentPrimaryPlane, *currentCursorPlane;
    SRM_CONNECTOR_STATE state;

    // Gamma table, used only in atomic API
    struct drm_color_lut *gamma;
    UInt32 gammaBlobId;

    Int8 renderInitResult;
    UInt8 connected;
    char *name, *manufacturer, *model, *serial;

    // Cursor
    struct AtomicCursor
    {
        struct gbm_bo *bo;
        UInt32 fb;
    } cursor[2];

    Int32 cursorIndex;
    Int32 cursorX, cursorY;
    UInt8 cursorVisible;
    UInt32 atomicChanges;

    // Interface for OpenGL events
    SRMConnectorInterface *interface;
    void *interfaceData;
    pthread_t renderThread;

    // Render common
    UInt32 lastFb;
    drmEventContext drmEventCtx;
    UInt8 pendingPageFlip;
    UInt8 firstPageFlip;

    UInt8 hasRepaintCond;
    pthread_cond_t repaintCond;

    UInt8 hasRepaintMutex;
    pthread_mutex_t repaintMutex;

    UInt8 repaintRequested;
    pthread_mutex_t stateMutex;
    SRMBox *damageBoxes;
    Int32 damageBoxesCount;
    SRMFormat currentFormat;
    SRMPresentationTime presentationTime;

    // Current VSYNC state (enabled by default)
    UInt8 currentVSync;

    // Requested by the user
    UInt8 pendingVSync;

    // Max refresh rate when vsync is off
    Int32 maxRefreshRate;

    // Protect stuff like cursor and gamma updates
    pthread_mutex_t propsMutex;

    // Render specific
    struct SRMConnectorRenderInterface renderInterface;
    void *renderData;
};

SRMConnector *srmConnectorCreate(SRMDevice *device, UInt32 id);
void srmConnectorDestroy(SRMConnector *connector);

UInt8 srmConnectorUpdateProperties(SRMConnector *connector);

UInt8 srmConnectorUpdateNames(SRMConnector *connector);
void srmConnectorDestroyNames(SRMConnector *connector);

UInt8 srmConnectorUpdateEncoders(SRMConnector *connector);
void srmConnectorDestroyEncoders(SRMConnector *connector);

UInt8 srmConnectorUpdateModes(SRMConnector *connector);
void srmConnectorDestroyModes(SRMConnector *connector);

void srmConnectorSetCursorPlaneToNeededConnector(SRMPlane *cursorPlane);

SRMConnectorMode *srmConnectorFindPreferredMode(SRMConnector *connector);
UInt8 srmConnectorGetBestConfiguration(SRMConnector *connector, SRMEncoder **bestEncoder, SRMCrtc **bestCrtc, SRMPlane **bestPrimaryPlane, SRMPlane **bestCursorPlane);
void *srmConnectorRenderThread(void *conn);
void srmConnectorUnlockRenderThread(SRMConnector *connector, UInt8 repaint);
void srmConnectorRenderThreadCleanUp(SRMConnector *connector);

#ifdef __cplusplus
}
#endif

#endif // SRMCONNECTORPRIVATE_H
