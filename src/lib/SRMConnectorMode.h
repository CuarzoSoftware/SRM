#ifndef SRMCONNECTORMODE_H
#define SRMCONNECTORMODE_H

#include "SRMTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup SRMConnectorMode SRMConnectorMode
 *
 * @brief Resolution and refresh rate configuration for a connector.
 *
 * SRMConnectorMode represents the resolution and refresh rate settings for a connector.
 * Use the functions in this module to work with connector modes.
 *
 * @{
 */

/**
 * @brief Set user data for the connector mode.
 *
 * This function sets user data for the given SRMConnectorMode. You can associate custom data with the mode using this function.
 *
 * @param connectorMode Pointer to the SRMConnectorMode for which to set user data.
 * @param userData Pointer to the user data to associate with the connector mode.
 */
void srmConnectorModeSetUserData(SRMConnectorMode *connectorMode, void *userData);

/**
 * @brief Get user data for the connector mode.
 *
 * This function retrieves the user data associated with the given SRMConnectorMode.
 *
 * @param connectorMode Pointer to the SRMConnectorMode from which to retrieve user data.
 * @return Pointer to the user data associated with the connector mode.
 */
void *srmConnectorModeGetUserData(SRMConnectorMode *connectorMode);

/**
 * @brief Get the connector associated with the connector mode.
 *
 * This function returns the SRMConnector associated with the given SRMConnectorMode.
 *
 * @param connectorMode Pointer to the SRMConnectorMode for which to retrieve the associated connector.
 * @return Pointer to the SRMConnector associated with the connector mode.
 */
SRMConnector *srmConnectorModeGetConnector(SRMConnectorMode *connectorMode);

/**
 * @brief Get the width of the connector mode.
 *
 * This function returns the width in pixels of the resolution associated with the given SRMConnectorMode.
 *
 * @param connectorMode Pointer to the SRMConnectorMode for which to retrieve the width.
 * @return The width of the connector mode.
 */
UInt32 srmConnectorModeGetWidth(SRMConnectorMode *connectorMode);

/**
 * @brief Get the height of the connector mode.
 *
 * This function returns the height in pixels of the resolution associated with the given SRMConnectorMode.
 *
 * @param connectorMode Pointer to the SRMConnectorMode for which to retrieve the height.
 * @return The height of the connector mode.
 */
UInt32 srmConnectorModeGetHeight(SRMConnectorMode *connectorMode);

/**
 * @brief Get the refresh rate of the connector mode.
 *
 * This function returns the refresh rate in Hertz (Hz) associated with the given SRMConnectorMode.
 *
 * @param connectorMode Pointer to the SRMConnectorMode for which to retrieve the refresh rate.
 * @return The refresh rate of the connector mode.
 */
UInt32 srmConnectorModeGetRefreshRate(SRMConnectorMode *connectorMode);

/**
 * @brief Check if the connector mode is the preferred mode by the connector.
 *
 * This function checks if the given SRMConnectorMode is the preferred mode for the associated SRMConnector.
 *
 * @param connectorMode Pointer to the SRMConnectorMode to check if it is the preferred mode.
 * @return 1 if the connector mode is preferred, 0 otherwise.
 */
UInt8 srmConnectorModeIsPreferred(SRMConnectorMode *connectorMode);

/** @} */ // End of SRMConnectorMode module

#ifdef __cplusplus
}
#endif

#endif // SRMCONNECTORMODE_H
