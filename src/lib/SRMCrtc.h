#ifndef SRMCRTC_H
#define SRMCRTC_H

#include "SRMTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup SRMCrtc SRMCrtc
 *
 * @brief Cathode Ray Tube Controllers (CRTCs).
 *
 * A Cathode Ray Tube Controller (CRTC) is responsible for controlling the display timings and attributes
 * for a connected display device. This module provides functions to work with CRTCs in a SRM context.
 *
 * @note This module is primarily used by SRM internally and may not be of much use to users.
 *
 * @{
 */

/**
 * @brief Get the DRM ID of this CRTC.
 *
 * @param crtc A pointer to the @ref SRMCrtc instance.
 *
 * @return The DRM ID of the CRTC.
 */
UInt32 srmCrtcGetID(SRMCrtc *crtc);

/**
 * @brief Get the device this CRTC belongs to.
 *
 * @param crtc A pointer to the @ref SRMCrtc instance.
 *
 * @return A pointer to the @ref SRMDevice that this CRTC belongs to.
 */
SRMDevice *srmCrtcGetDevice(SRMCrtc *crtc);

/**
 * @brief Returns a pointer to the @ref SRMConnector that is currently using this CRTC,
 *        or `NULL` if it is not used by any connector.
 *
 * @param crtc A pointer to the @ref SRMCrtc instance.
 *
 * @return A pointer to the @ref SRMConnector currently using this CRTC, or `NULL` if not in use.
 */
SRMConnector *srmCrtcGetCurrentConnector(SRMCrtc *crtc);

/**
 * @brief Gets the number of elements used to represent each RGB gamma correction curve.
 *
 * This function retrieves the number of elements (N) used to represent each RGB gamma correction curve,
 * where N is the count of @ref UInt16 elements for red, green, and blue curves.
 * 
 * @see srmConnectorSetGamma()
 * 
 * @param connector Pointer to the @ref SRMCrtc.
 * @return The number of elements used to represent each RGB gamma correction curve, or 0
 *         if the driver does not support gamma correction.
 */
UInt64 srmCrtcGetGammaSize(SRMCrtc *crtc);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif // SRMCRTC_H
