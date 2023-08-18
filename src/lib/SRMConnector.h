#ifndef SRMCONNECTOR_H
#define SRMCONNECTOR_H

#include "SRMTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup SRMConnector SRMConnector
 * @brief Represents a display screen with associated rendering capabilities and modes.
 *
 * An SRMConnector represents a screen where content is displayed, ranging from laptop screens to HDMI monitors.
 * 
 * ### Rendering
 * 
 * Each connector has its own dedicated rendering thread, which manages fundamental OpenGL methods like `initializeGL()`, `paintGL()`, and `resizeGL()`.
 * You can initialize a connector using the `srmConnectorInitialize()` method.
 *
 * ### Modes
 * Connectors support multiple display modes that define factors such as refresh rate and resolution.
 * These modes can be enumerated using `srmConnectorGetModes()` and selected using `srmConnectorSetMode()`.
 * 
 * @addtogroup SRMConnector
 * @{
 */

/**
 * @brief Structure defining the interface for handling OpenGL-related operations on an SRMConnector.
 * 
 * The SRMConnectorInterfaceStruct defines a set of function pointers that allow customization of
 * OpenGL-related operations on an SRMConnector. These operations include initialization, rendering,
 * handling page flipping, resizing, and uninitialization.
 */
struct SRMConnectorInterfaceStruct
{
    /**
     * @brief Function to initialize OpenGL on a connector.
     * 
     * @param connector Pointer to the SRMConnector.
     * @param data Additional data for customization.
     */
    void (*initializeGL)(SRMConnector *connector, void *data);
    
    /**
     * @brief Function to perform OpenGL rendering on a connector.
     * 
     * @param connector Pointer to the SRMConnector.
     * @param data Additional data for customization.
     */
    void (*paintGL)(SRMConnector *connector, void *data);
    
    /**
     * @brief Function to handle page flipping on a connector.
     * 
     * @param connector Pointer to the SRMConnector.
     * @param data Additional data for customization.
     */
    void (*pageFlipped)(SRMConnector *connector, void *data);
    
    /**
     * @brief Function to handle resizing of OpenGL resources on a connector.
     * 
     * @param connector Pointer to the SRMConnector.
     * @param data Additional data for customization.
     */
    void (*resizeGL)(SRMConnector *connector, void *data);
    
    /**
     * @brief Function to uninitialize OpenGL on a connector.
     * 
     * @param connector Pointer to the SRMConnector.
     * @param data Additional data for customization.
     */
    void (*uninitializeGL)(SRMConnector *connector, void *data);
};
typedef struct SRMConnectorInterfaceStruct SRMConnectorInterface;

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

UInt8 srmConnectorHasBufferDamageSupport(SRMConnector *connector);
UInt8 srmConnectorSetBufferDamage(SRMConnector *connector, SRMRect *rects, Int32 n);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif // SRMCONNECTOR_H
