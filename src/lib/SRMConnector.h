#ifndef SRMCONNECTOR_H
#define SRMCONNECTOR_H

#include "SRMTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

void srmConnectorSetUserData(SRMConnector *connector, void *userData);
void *srmConnectorGetUserData(SRMConnector *connector);

SRMDevice *srmConnectorGetDevice(SRMConnector *connector);
UInt32 srmConnectorGetID(SRMConnector *connector);
SRM_CONNECTOR_STATE srmConnectorGetState(SRMConnector *connector);
UInt8 srmConnectorIsConnected(SRMConnector *connector);
UInt32 srmConnectorGetmmWidth(SRMConnector *connector);
UInt32 srmConnectorGetmmHeight(SRMConnector *connector);
UInt32 srmConnectorGetType(SRMConnector *connector);

const char *srmConnectorGetName(SRMConnector *connector);
const char *srmConnectorGetManufacturer(SRMConnector *connector);
const char *srmConnectorGetModel(SRMConnector *connector);

SRMList *srmConnectorGetEncoders(SRMConnector *connector);
SRMList *srmConnectorGetModes(SRMConnector *connector);

// Cursor
UInt8 srmConnectorHasHardwareCursor(SRMConnector *connector);
UInt8 srmConnectorSetCursor(SRMConnector *connector, UInt8 *pixels);
UInt8 srmConnectorSetCursorPos(SRMConnector *connector, Int32 x, Int32 y);

// Current state
SRMEncoder *srmConnectorGetCurrentEncoder(SRMConnector *connector);
SRMCrtc *srmConnectorGetCurrentCrtc(SRMConnector *connector);
SRMPlane *srmConnectorGetCurrentPrimaryPlane(SRMConnector *connector);
SRMPlane *srmConnectorGetCurrentCursorPlane(SRMConnector *connector);

// Modes
SRMConnectorMode *srmConnectorGetPreferredMode(SRMConnector *connector);
SRMConnectorMode *srmConnectorGetCurrentMode(SRMConnector *connector);
UInt8 srmConnectorSetMode(SRMConnector *connector, SRMConnectorMode *mode);

// Rendering
UInt8 srmConnectorInitialize(SRMConnector *connector, SRMConnectorInterface *interface, void *userData);
UInt8 srmConnectorRepaint(SRMConnector *connector);
void srmConnectorUninitialize(SRMConnector *connector);
UInt8 srmConnectorPause(SRMConnector *connector);
UInt8 srmConnectorResume(SRMConnector *connector);
UInt32 srmConnectorGetCurrentBufferIndex(SRMConnector *connector);
UInt32 srmConnectorGetBuffersCount(SRMConnector *connector);
SRMBuffer *srmConnectorGetBuffer(SRMConnector *connector, UInt32 bufferIndex);

#ifdef __cplusplus
}
#endif

#endif // SRMCONNECTOR_H
