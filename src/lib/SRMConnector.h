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
typedef struct SRMConnectorInterfaceStruct
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
} SRMConnectorInterface;

/**
 * @brief Enumeration of connector states.
 */
typedef enum SRM_CONNECTOR_STATE_ENUM
{
    SRM_CONNECTOR_STATE_UNINITIALIZED = 0, ///< The connector is uninitialized.
    SRM_CONNECTOR_STATE_INITIALIZED = 1,   ///< The connector is initialized.
    SRM_CONNECTOR_STATE_UNINITIALIZING = 2, ///< The connector is in the process of uninitializing.
    SRM_CONNECTOR_STATE_INITIALIZING = 3,   ///< The connector is in the process of initializing.
    SRM_CONNECTOR_STATE_CHANGING_MODE = 4,  ///< The connector is changing display mode.
    SRM_CONNECTOR_STATE_REVERTING_MODE = 5, ///< Special case when changing mode fails and reverts.
    SRM_CONNECTOR_STATE_PAUSING = 6,       ///< The connector is pausing operation.
    SRM_CONNECTOR_STATE_PAUSED = 7,        ///< The connector is paused.
    SRM_CONNECTOR_STATE_RESUMING = 8       ///< The connector is resuming operation.
} SRM_CONNECTOR_STATE;

/**
 * @brief Sets the connector user data.
 *
 * This function sets the user data associated with the given SRMConnector.
 *
 * @param connector Pointer to the SRMConnector whose user data is to be set.
 * @param userData Pointer to the user data to be associated with the connector.
 */
void srmConnectorSetUserData(SRMConnector *connector, void *userData);

/**
 * @brief Retrieves the connector user data.
 *
 * This function retrieves the user data associated with the given SRMConnector.
 *
 * @param connector Pointer to the SRMConnector from which to retrieve the user data.
 * @return Pointer to the user data associated with the connector.
 */
void *srmConnectorGetUserData(SRMConnector *connector);

/**
 * @brief Get the device this connector belongs to.
 *
 * This function returns the device to which the connector belongs.
 *
 * @param connector Pointer to the SRMConnector for which to retrieve the device.
 * @return Pointer to the SRMDevice to which the connector belongs.
 *
 * @note The device may not always be the same as the renderer device in cases different to the ITSELF render mode.
 */
SRMDevice *srmConnectorGetDevice(SRMConnector *connector);

/**
 * @brief Get the DRM connector ID.
 *
 * This function returns the DRM connector ID associated with the connector.
 *
 * @param connector Pointer to the SRMConnector for which to retrieve the connector ID.
 * @return The DRM connector ID of the connector.
 */
UInt32 srmConnectorGetID(SRMConnector *connector);

/**
 * @brief Get the current state of the connector rendering.
 *
 * This function returns the current state of the rendering for the given SRMConnector.
 *
 * @param connector Pointer to the SRMConnector for which to retrieve the rendering state.
 * @return The current rendering state (e.g., initialized, uninitialized, etc).
 */
SRM_CONNECTOR_STATE srmConnectorGetState(SRMConnector *connector);

/**
 * @brief Check if the connector is connected.
 *
 * This function checks whether the given SRMConnector is connected.
 *
 * @param connector Pointer to the SRMConnector to check for connection.
 * @return 1 if the connector is connected, 0 otherwise.
 */
UInt8 srmConnectorIsConnected(SRMConnector *connector);

/**
 * @brief Get the physical width of the connector in millimeters.
 *
 * This function returns the physical width of the connector in millimeters.
 *
 * @param connector Pointer to the SRMConnector for which to retrieve the width.
 * @return The physical width of the connector in millimeters.
 */
UInt32 srmConnectorGetmmWidth(SRMConnector *connector);

/**
 * @brief Get the physical height of the connector in millimeters.
 *
 * This function returns the physical height of the connector in millimeters.
 *
 * @param connector Pointer to the SRMConnector for which to retrieve the height.
 * @return The physical height of the connector in millimeters.
 */
UInt32 srmConnectorGetmmHeight(SRMConnector *connector);

/**
 * @brief Get the DRM type of the connector.
 *
 * This function returns the DRM type associated with the connector.
 *
 * @param connector Pointer to the SRMConnector for which to retrieve the type.
 * @return The DRM type of the connector.
 */
UInt32 srmConnectorGetType(SRMConnector *connector);

/**
 * @brief Get the name of the connector.
 *
 * This function returns the name of the connector. The name is always unique, even across devices.
 * For example, if there are two HDMI-A connectors, one will be called HDMI-A-0, and the other HDMI-A-1.
 *
 * @param connector Pointer to the SRMConnector for which to retrieve the name.
 * @return The name of the connector.
 */
const char *srmConnectorGetName(SRMConnector *connector);

/**
 * @brief Get the manufacturer of the connector.
 *
 * This function returns the manufacturer of the connector.
 *
 * @param connector Pointer to the SRMConnector for which to retrieve the manufacturer.
 * @return The manufacturer of the connector.
 */
const char *srmConnectorGetManufacturer(SRMConnector *connector);

/**
 * @brief Get the model of the connector.
 *
 * This function returns the model of the connector.
 *
 * @param connector Pointer to the SRMConnector for which to retrieve the model.
 * @return The model of the connector.
 */
const char *srmConnectorGetModel(SRMConnector *connector);

/**
 * @brief Get a list of available connector encoders.
 *
 * This function returns a list of available encoders for the given SRMConnector.
 *
 * @param connector Pointer to the SRMConnector for which to retrieve the encoders.
 * @return A list of available connector encoders (SRMEncoder*).
 */
SRMList *srmConnectorGetEncoders(SRMConnector *connector);

/**
 * @brief Get a list of available connector modes.
 *
 * This function returns a list of available modes (resolutions and refresh rates) for the given SRMConnector.
 *
 * @param connector Pointer to the SRMConnector for which to retrieve the modes.
 * @return A list of available connector modes (SRMConnectorMode*).
 */
SRMList *srmConnectorGetModes(SRMConnector *connector);

/**
 * @brief Check if there is an available cursor plane for hardware cursor compositing.
 *
 * This function checks if there is an available cursor plane for hardware cursor compositing on the given SRMConnector.
 *
 * @param connector Pointer to the SRMConnector to check for hardware cursor support.
 * @return 1 if hardware cursor support is available, 0 otherwise.
 */
UInt8 srmConnectorHasHardwareCursor(SRMConnector *connector);

/**
 * @brief Set the pixels of the hardware cursor.
 *
 * This function sets the pixels of the hardware cursor for the given SRMConnector.
 *
 * The format of the buffer must be ARGB8888 with a size of 64x64 pixels.
 * Passing NULL as the buffer hides the cursor.
 *
 * @param connector Pointer to the SRMConnector for which to set the hardware cursor.
 * @param pixels Pointer to the ARGB8888 pixel buffer for the cursor image.
 * @return 1 if the hardware cursor was successfully set, 0 if an error occurred.
 */
UInt8 srmConnectorSetCursor(SRMConnector *connector, UInt8 *pixels);

/**
 * @brief Set the position of the hardware cursor relative to the connector's top-left origin.
 *
 * This function sets the position of the hardware cursor relative to the top-left origin of the connector.
 *
 * @param connector Pointer to the SRMConnector for which to set the cursor position.
 * @param x The X-coordinate of the cursor's position.
 * @param y The Y-coordinate of the cursor's position.
 * @return 1 if the cursor position was successfully set, 0 if an error occurred.
 */
UInt8 srmConnectorSetCursorPos(SRMConnector *connector, Int32 x, Int32 y);

/**
 * @brief Get the model of the connector.
 *
 * This function returns the model of the connector.
 *
 * @param connector Pointer to the SRMConnector for which to retrieve the model.
 * @return The model of the connector.
 */
const char *srmConnectorGetModel(SRMConnector *connector);

/**
 * @brief Get a list of available connector encoders.
 *
 * This function returns a list of available encoders for the given SRMConnector.
 *
 * @param connector Pointer to the SRMConnector for which to retrieve the encoders.
 * @return A list of available connector encoders (SRMEncoder*).
 */
SRMList *srmConnectorGetEncoders(SRMConnector *connector);

/**
 * @brief Get a list of available connector modes.
 *
 * This function returns a list of available modes (resolutions and refresh rates) for the given SRMConnector.
 *
 * @param connector Pointer to the SRMConnector for which to retrieve the modes.
 * @return A list of available connector modes (SRMConnectorMode*).
 */
SRMList *srmConnectorGetModes(SRMConnector *connector);

/**
 * @brief Check if there is an available cursor plane for hardware cursor compositing.
 *
 * This function checks if there is an available cursor plane for hardware cursor compositing on the given SRMConnector.
 *
 * @param connector Pointer to the SRMConnector to check for hardware cursor support.
 * @return 1 if hardware cursor support is available, 0 otherwise.
 */
UInt8 srmConnectorHasHardwareCursor(SRMConnector *connector);

/**
 * @brief Set the pixels of the hardware cursor.
 *
 * This function sets the pixels of the hardware cursor for the given SRMConnector.
 *
 * The format of the buffer must be ARGB8888 with a size of 64x64 pixels.
 * Passing NULL as the buffer hides the cursor.
 *
 * @param connector Pointer to the SRMConnector for which to set the hardware cursor.
 * @param pixels Pointer to the ARGB8888 pixel buffer for the cursor image.
 * @return 1 if the hardware cursor was successfully set, 0 if an error occurred.
 */
UInt8 srmConnectorSetCursor(SRMConnector *connector, UInt8 *pixels);

/**
 * @brief Set the position of the hardware cursor relative to the connector's top-left origin.
 *
 * This function sets the position of the hardware cursor relative to the top-left origin of the connector.
 *
 * @param connector Pointer to the SRMConnector for which to set the cursor position.
 * @param x The X-coordinate of the cursor's position.
 * @param y The Y-coordinate of the cursor's position.
 * @return 1 if the cursor position was successfully set, 0 if an error occurred.
 */
UInt8 srmConnectorSetCursorPos(SRMConnector *connector, Int32 x, Int32 y);

/**
 * @brief Get the currently used encoder for the connector.
 *
 * This function returns the currently used SRMEncoder associated with the given SRMConnector.
 *
 * @param connector Pointer to the SRMConnector for which to retrieve the current encoder.
 * @return The currently used SRMEncoder for the connector.
 */
SRMEncoder *srmConnectorGetCurrentEncoder(SRMConnector *connector);

/**
 * @brief Get the currently used CRT controller (CRTC) for the connector.
 *
 * This function returns the currently used SRMCrtc associated with the given SRMConnector.
 *
 * @param connector Pointer to the SRMConnector for which to retrieve the current CRTC.
 * @return The currently used SRMCrtc for the connector.
 */
SRMCrtc *srmConnectorGetCurrentCrtc(SRMConnector *connector);

/**
 * @brief Get the currently used primary plane for the connector.
 *
 * This function returns the currently used SRMPlane associated with the primary display plane for the given SRMConnector.
 *
 * @param connector Pointer to the SRMConnector for which to retrieve the current primary plane.
 * @return The currently used SRMPlane for the primary display.
 */
SRMPlane *srmConnectorGetCurrentPrimaryPlane(SRMConnector *connector);

/**
 * @brief Get the currently used cursor plane for the connector.
 *
 * This function returns the currently used SRMPlane associated with the cursor display plane for the given SRMConnector.
 *
 * @param connector Pointer to the SRMConnector for which to retrieve the current cursor plane.
 * @return The currently used SRMPlane for the cursor display.
 */
SRMPlane *srmConnectorGetCurrentCursorPlane(SRMConnector *connector);

/**
 * @brief Get the preferred connector mode.
 *
 * This function returns the preferred SRMConnectorMode for the given SRMConnector.
 *
 * @param connector Pointer to the SRMConnector for which to retrieve the preferred mode.
 * @return The preferred SRMConnectorMode for the connector.
 */
SRMConnectorMode *srmConnectorGetPreferredMode(SRMConnector *connector);

/**
 * @brief Get the current connector mode.
 *
 * This function returns the current SRMConnectorMode for the given SRMConnector.
 *
 * @param connector Pointer to the SRMConnector for which to retrieve the current mode.
 * @return The current SRMConnectorMode for the connector.
 */
SRMConnectorMode *srmConnectorGetCurrentMode(SRMConnector *connector);

/**
 * @brief Sets the current mode of the connector.
 *
 * This function sets the current mode of the given SRMConnector. You can use srmConnectorGetModes() to obtain a list of all available modes. If the connector is initialized and the mode resolution is different from the current one, the resizeGL() method is invoked.
 *
 * @param connector Pointer to the SRMConnector for which to set the mode.
 * @param mode Pointer to the SRMConnectorMode to set as the current mode.
 * @return 1 if setting the mode is successful, 0 if an error occurs.
 */
UInt8 srmConnectorSetMode(SRMConnector *connector, SRMConnectorMode *mode);

/**
 * @brief Initializes a connector, creating its rendering thread and invoking initializeGL() once initialized.
 *
 * This function initializes the given SRMConnector. After initialization, calling srmConnectorRepaint()
 * schedules a new rendering frame, which invokes paintGL() followed by a pageFlipped() event.
 *
 * @param connector Pointer to the SRMConnector to initialize.
 * @param interface Pointer to the SRMConnectorInterface struct to handle events.
 * @param userData Pointer to user data to be associated with the connector.
 * @return 1 if initialization is successful, 0 if an error occurs.
 *
 * @warning Make sure to correctly set up the interface struct to handle all its events, otherwise, it may crash.
 */
UInt8 srmConnectorInitialize(SRMConnector *connector, SRMConnectorInterface *interface, void *userData);

/**
 * @brief Schedules a new rendering frame.
 *
 * This function schedules a new rendering frame for the given SRMConnector. Calling this method
 * multiple times during the same frame will not invoke paintGL() multiple times, only once.
 * After each paintGL() event, this method must be called again to schedule a new frame.
 *
 * @param connector Pointer to the SRMConnector to schedule a new rendering frame.
 * @return 1 if scheduling is successful, 0 if an error occurs.
 *
 * @note This function triggers paintGL() and pageFlipped() events in the rendering process.
 */
UInt8 srmConnectorRepaint(SRMConnector *connector);

/**
 * @brief Uninitializes the connector.
 *
 * This function uninitializes the given SRMConnector, eventually calling uninitializeGL() once uninitialized.
 *
 * @param connector Pointer to the SRMConnector to uninitialize.
 */
void srmConnectorUninitialize(SRMConnector *connector);

/**
 * @brief Locks the rendering thread until srmConnectorResume() is called.
 *
 * This function locks the rendering thread of the SRMConnector until srmConnectorResume() is called to unlock it.
 *
 * @param connector Pointer to the SRMConnector to pause.
 * @return 1 if pausing is successful, 0 if an error occurs.
 */
UInt8 srmConnectorPause(SRMConnector *connector);

/**
 * @brief Unlocks the rendering thread if previously locked with srmConnectorPause().
 *
 * This function unlocks the rendering thread of the SRMConnector if it was previously locked with srmConnectorPause().
 *
 * @param connector Pointer to the SRMConnector to resume.
 * @return 1 if resuming is successful, 0 if an error occurs.
 */
UInt8 srmConnectorResume(SRMConnector *connector);

/**
 * @brief Returns the current framebuffer index.
 *
 * This function returns the index of the current framebuffer where the rendering frame is going to be stored.
 *
 * @param connector Pointer to the SRMConnector to query for the current framebuffer index.
 * @return The current framebuffer index.
 */
UInt32 srmConnectorGetCurrentBufferIndex(SRMConnector *connector);

/**
 * @brief Returns the number of framebuffers.
 *
 * This function returns the number of framebuffers available for the given SRMConnector. The count may be 1, 2, 3, or more, depending on the configuration (e.g., double buffer, triple buffering).
 *
 * The number of framebuffers can be modified by setting the SRM_RENDER_MODE_{ITSELF, DUMB, CPU}_FB_COUNT environment variables.
 *
 * @param connector Pointer to the SRMConnector to query for the number of framebuffers.
 * @return The number of framebuffers.
 */
UInt32 srmConnectorGetBuffersCount(SRMConnector *connector);

/**
 * @brief Returns the framebuffer buffer of a specific index, usable as a texture for rendering.
 *
 * This function returns the framebuffer buffer of the specified index for the given SRMConnector. This buffer can be used as a texture for rendering, but it may not always be supported. Always check if NULL is returned.
 *
 * Additionally, note that the buffer may not always be shared among all GPUs. In such cases, calling srmBufferGetTextureID() may return 0.
 *
 * @param connector Pointer to the SRMConnector for which to retrieve the framebuffer buffer.
 * @param bufferIndex The index of the framebuffer buffer to retrieve.
 * @return A pointer to the SRMBuffer if available, or NULL if not supported.
 */
SRMBuffer *srmConnectorGetBuffer(SRMConnector *connector, UInt32 bufferIndex);

/**
 * @brief Checks if the connector benefits from providing it damage information generated during the last paintGL call.
 *
 * This function checks if the given SRMConnector benefits from receiving damage information generated during the last paintGL call. Providing this information may improve performance, especially when using DUMB or CPU render modes, as the connector will only copy the specified rectangles.
 *
 * @param connector Pointer to the SRMConnector to check for buffer damage support.
 * @return 1 if the connector supports buffer damage notifications, 0 otherwise.
 */
UInt8 srmConnectorHasBufferDamageSupport(SRMConnector *connector);

/**
 * @brief Notifies the connector of new damage generated during the last paintGL call.
 *
 * This function notifies the given SRMConnector of new damage areas generated during the last paintGL call. Providing this damage information may improve performance, particularly when using DUMB or CPU render modes, as the connector will only copy the specified rectangles. The damage specified is only valid during the current frame and is cleared in the next frame.
 *
 * @param connector Pointer to the SRMConnector to notify of buffer damage.
 * @param rects An array of SRMRect structures representing the damaged areas.
 * @param n The number of damaged areas specified in the array.
 * @return 1 if the damage notification is successful, 0 if an error occurs.
 */
UInt8 srmConnectorSetBufferDamage(SRMConnector *connector, SRMRect *rects, Int32 n);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif // SRMCONNECTOR_H
