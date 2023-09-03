#ifndef SRMPLANE_H
#define SRMPLANE_H

#include "SRMTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup SRMPlane SRMPlane
 *
 * @brief DRM plane associated with a DRM device.
 *
 * The SRMPlane module provides functions for querying information about planes associated with DRM devices.
 * These planes can be used for various purposes, such as displaying content on connectors.
 *
 * @{
 */

/**
 * @brief Get the unique identifier of a DRM plane.
 *
 * @param plane A pointer to the SRMPlane instance.
 *
 * @return The unique identifier of the DRM plane.
 */
UInt32 srmPlaneGetID(SRMPlane *plane);

/**
 * @brief Get the DRM device to which a DRM plane belongs.
 *
 * @param plane A pointer to the SRMPlane instance.
 *
 * @return A pointer to the SRMDevice representing the DRM device.
 */
SRMDevice *srmPlaneGetDevice(SRMPlane *plane);

/**
 * @brief Get a list of CRTCs (scanout engines) associated with a DRM plane.
 *
 * @param plane A pointer to the SRMPlane instance.
 *
 * @return A list of SRMCrtc instances representing the CRTCs associated with the plane.
 */
SRMList *srmPlaneGetCrtcs(SRMPlane *plane);

/**
 * @brief Get the current connector associated with a DRM plane.
 *
 * @param plane A pointer to the SRMPlane instance.
 *
 * @return A pointer to the SRMConnector representing the current connector, or NULL if not associated with any.
 */
SRMConnector *srmPlaneGetCurrentConnector(SRMPlane *plane);

/**
 * @brief Get the type of a DRM plane.
 *
 * @param plane A pointer to the SRMPlane instance.
 *
 * @return The type of the DRM plane, as an SRM_PLANE_TYPE enumeration value.
 */
SRM_PLANE_TYPE srmPlaneGetType(SRMPlane *plane);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif // SRMPLANE_H
