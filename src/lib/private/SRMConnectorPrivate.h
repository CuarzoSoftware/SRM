#ifndef SRMCONNECTORPRIVATE_H
#define SRMCONNECTORPRIVATE_H

#include "../SRMConnector.h"

#include <GLES2/gl2.h>
#include <gbm.h>
#include <EGL/egl.h>
#include <xf86drm.h>

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
    UInt32 cursorFB;
    Int32 cursorX, cursorY;
    UInt8 cursorVisible;
    UInt8 atomicCursorHasChanges;

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
    pthread_mutex_t stateMutex;
    SRMRect *damageRects;
    Int32 damageRectsCount;

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

#ifdef __cplusplus
}
#endif

#endif // SRMCONNECTORPRIVATE_H
