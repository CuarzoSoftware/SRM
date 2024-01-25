#ifndef SRMPLANE_H
#define SRMPLANE_H

#include <SRMTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup SRMPlane SRMPlane
 *
 * @brief DRM plane associated with a DRM device.
 *
 * The @ref SRMPlane module provides functions for querying information about planes associated with DRM devices.
 * These planes can be used for various purposes, such as displaying content on connectors.
 *
 * @note This module is primarily used by SRM internally and may not be of much use to users.
 * 
 * @{
 */

/**
 * @brief Get the unique identifier of a DRM plane.
 *
 * @param plane A pointer to the @ref SRMPlane instance.
 *
 * @return The unique identifier of the DRM plane.
 */
UInt32 srmPlaneGetID(SRMPlane *plane);

/**
 * @brief Get the DRM device to which a DRM plane belongs.
 *
 * @param plane A pointer to the @ref SRMPlane instance.
 *
 * @return A pointer to the @ref SRMDevice representing the DRM device.
 */
SRMDevice *srmPlaneGetDevice(SRMPlane *plane);

/**
 * @brief Get a list of CRTCs associated with a DRM plane.
 *
 * @param plane A pointer to the @ref SRMPlane instance.
 *
 * @return A list of @ref SRMCrtc instances representing the CRTCs associated with the plane.
 */
SRMList *srmPlaneGetCrtcs(SRMPlane *plane);

/**
 * @brief Get the current connector associated with a DRM plane.
 *
 * @param plane A pointer to the @ref SRMPlane instance.
 *
 * @return A pointer to the @ref SRMConnector representing the current connector, or `NULL` if not associated with any.
 */
SRMConnector *srmPlaneGetCurrentConnector(SRMPlane *plane);

/**
 * @brief Get the type of a DRM plane.
 *
 * @param plane A pointer to the @ref SRMPlane instance.
 *
 * @return The type of the DRM plane, as an @ref SRM_PLANE_TYPE enumeration value.
 */
SRM_PLANE_TYPE srmPlaneGetType(SRMPlane *plane);

/**
 * @brief Get a list of DRM framebuffer formats supported by the plane.
 * *
 * @param plane A pointer to the @ref SRMPlane instance.
 *
 * @return A list of DRM formats/modifiers supported by the plane.
 */
SRMList *srmPlaneGetFormats(SRMPlane *plane);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif // SRMPLANE_H
