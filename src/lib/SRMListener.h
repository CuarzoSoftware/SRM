#ifndef SRMLISTENER_H
#define SRMLISTENER_H

#include <SRMTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup SRMListener SRMListener
 *
 * @brief Module for managing event listeners.
 *
 * The SRMListener module provides functions for creating, setting, and managing event listeners.
 * Event listeners are used to handle events or callbacks in response to specific actions or changes.
 *
 * @see srmCoreAddDeviceCreatedEventListener()
 * @see srmCoreAddDeviceRemovedEventListener()
 * @see srmCoreAddConnectorPluggedEventListener()
 * @see srmCoreAddConnectorUnpluggedEventListener()
 * 
 * @{
 */

/**
 * @brief Set user data associated with an event listener.
 *
 * @param listener A pointer to the SRMListener instance.
 * @param userData A pointer to user-defined data to associate with the listener.
 */
void srmListenerSetUserData(SRMListener *listener, void *userData);

/**
 * @brief Get user data associated with an event listener.
 *
 * @param listener A pointer to the SRMListener instance.
 *
 * @return A pointer to the user-defined data associated with the listener.
 */
void *srmListenerGetUserData(SRMListener *listener);

/**
 * @brief Set the callback function associated with an event listener.
 *
 * @param listener A pointer to the SRMListener instance.
 * @param callbackFunction A pointer to the callback function to associate with the listener.
 */
void srmListenerSetCallbackFunction(SRMListener *listener, void *callbackFunction);

/**
 * @brief Get the callback function associated with an event listener.
 *
 * @param listener A pointer to the SRMListener instance.
 *
 * @return A pointer to the callback function associated with the listener.
 */
void *srmListenerGetCallbackFunction(SRMListener *listener);

/**
 * @brief Destroy an event listener and release associated resources.
 *
 * @param listener A pointer to the SRMListener instance to destroy.
 */
void srmListenerDestroy(SRMListener *listener);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif // SRMLISTENER_H
