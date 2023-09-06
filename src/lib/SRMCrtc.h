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
 * @param crtc A pointer to the SRMCrtc instance.
 *
 * @return The DRM ID of the CRTC.
 */
UInt32 srmCrtcGetID(SRMCrtc *crtc);

/**
 * @brief Get the device this CRTC belongs to.
 *
 * @param crtc A pointer to the SRMCrtc instance.
 *
 * @return A pointer to the SRMDevice that this CRTC belongs to.
 */
SRMDevice *srmCrtcGetDevice(SRMCrtc *crtc);

/**
 * @brief Returns a pointer to the SRMConnector that is currently using this CRTC,
 *        or NULL if it is not used by any connector.
 *
 * @param crtc A pointer to the SRMCrtc instance.
 *
 * @return A pointer to the SRMConnector currently using this CRTC, or NULL if not in use.
 */
SRMConnector *srmCrtcGetCurrentConnector(SRMCrtc *crtc);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif // SRMCRTC_H
