#ifndef SRMENCODER_H
#define SRMENCODER_H

#include <SRMTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup SRMEncoder SRMEncoder
 *
 * @brief Connector encoder in a DRM context.
 *
 * An @ref SRMEncoder represents a connector encoder device responsible for driving displays (e.g., monitors or screens).
 * This module provides functions to work with display encoders, including retrieving their information
 * and associated resources.
 *
 * @note This module is primarily used by SRM internally and may not be of much use to external users.
 *
 * @{
 */

/**
 * @brief Get the unique identifier of the encoder.
 *
 * @param encoder A pointer to the @ref SRMEncoder instance.
 *
 * @return The ID of the encoder.
 */
UInt32 srmEncoderGetID(SRMEncoder *encoder);

/**
 * @brief Get the device to which this encoder belongs.
 *
 * @param encoder A pointer to the @ref SRMEncoder instance.
 *
 * @return A pointer to the @ref SRMDevice representing the device to which the encoder belongs.
 */
SRMDevice *srmEncoderGetDevice(SRMEncoder *encoder);

/**
 * @brief Get a list of CRTCs (Cathode Ray Tube Controllers) compatible with this encoder.
 *
 * @param encoder A pointer to the @ref SRMEncoder instance.
 *
 * @return A list of pointers to the CRTCs (@ref SRMCrtc) compatible with the encoder.
 */
SRMList *srmEncoderGetCrtcs(SRMEncoder *encoder);

/**
 * @brief Get the connector that is currently using this encoder.
 *
 * @param encoder A pointer to the @ref SRMEncoder instance.
 *
 * @return A pointer to the @ref SRMConnector currently utilizing this encoder, or `NULL` if not in use.
 */
SRMConnector *srmEncoderGetCurrentConnector(SRMEncoder *encoder);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif // SRMENCODER_H
