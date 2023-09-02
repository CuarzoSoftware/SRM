#ifndef SRMTYPES_H
#define SRMTYPES_H

#define SRM_PAR_CPY 0
#define SRM_MAX_COPY_THREADS 32

#include <stdint.h>
#include <drm/drm_fourcc.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup SRMTypes SRMTypes
 *
 * @brief SRM data types
 *
 * @{
 */

/**
 * @def SRM_UNUSED(var)
 * @brief Macro to suppress "unused parameter" warnings.
 *
 * This macro is used to indicate that a function parameter is intentionally not used in the
 * current implementation, thereby suppressing any "unused parameter" warnings from the compiler.
 *
 * Usage:
 * ```
 * void myFunction(int a, int b) {
 *     SRM_UNUSED(a); // Suppress "unused parameter 'a'" warning
 *     // Your code here
 * }
 * ```
 *
 * @param var The variable (parameter) to mark as unused.
 */
#define SRM_UNUSED(var) (void)var

/**
 * @typedef Int8
 * @brief Alias for a signed 8-bit integer (int8_t).
 */
typedef int8_t   Int8;

/**
 * @typedef UInt8
 * @brief Alias for an unsigned 8-bit integer (uint8_t).
 */
typedef uint8_t  UInt8;

/**
 * @typedef Int32
 * @brief Alias for a signed 32-bit integer (int32_t).
 */
typedef int32_t  Int32;

/**
 * @typedef UInt32
 * @brief Alias for an unsigned 32-bit integer (uint32_t).
 */
typedef uint32_t UInt32;

/**
 * @typedef Int64
 * @brief Alias for a signed 64-bit integer (int64_t).
 */
typedef int64_t  Int64;

/**
 * @typedef UInt64
 * @brief Alias for an unsigned 64-bit integer (uint64_t).
 */
typedef uint64_t UInt64;

/// @ingroup SRMCore
typedef struct SRMCoreStruct SRMCore;

/// @ingroup SRMDevice
typedef struct SRMDeviceStruct SRMDevice;

/// @ingroup SRMCrtc
typedef struct SRMCrtcStruct SRMCrtc;

/// @ingroup SRMEncoder
typedef struct SRMEncoderStruct SRMEncoder;

/// @ingroup SRMPlane
typedef struct SRMPlaneStruct SRMPlane;

/// @ingroup SRMConnector
typedef struct SRMConnectorStruct SRMConnector;

/// @ingroup SRMConnectorMode
typedef struct SRMConnectorModeStruct SRMConnectorMode;

/// @ingroup SRMList
typedef struct SRMListStruct SRMList;
/// @ingroup SRMList
typedef struct SRMListItemStruct SRMListItem;

/// @ingroup SRMListener
typedef struct SRMListenerStruct SRMListener;

/// @ingroup SRMBuffer
typedef struct SRMBufferStruct SRMBuffer;
typedef UInt32 SRM_BUFFER_FORMAT; ///< A DRM format defined in drm_fourcc.h.
typedef UInt64 SRM_BUFFER_MODIFIER; ///< A DRM format modifier defined in drm_fourcc.h.

/**
 * @brief Structure representing a rectangle with integer coordinates.
 */
typedef struct SRMRectStruct
{
    Int32 x;       ///< The x-coordinate of the top-left corner of the rectangle.
    Int32 y;       ///< The y-coordinate of the top-left corner of the rectangle.
    Int32 width;   ///< The width of the rectangle.
    Int32 height;  ///< The height of the rectangle.
} SRMRect;

typedef enum SRM_RENDER_MODE_ENUM SRM_RENDER_MODE;

/**
 * @brief Get a string representation of a rendering mode.
 *
 * @param mode The rendering mode to retrieve the string for.
 *
 * @return A pointer to the string representation of the rendering mode.
 */
const char *srmGetRenderModeString(SRM_RENDER_MODE mode);

typedef enum SRM_PLANE_TYPE_ENUM SRM_PLANE_TYPE;

/**
 * @brief Get a string representation of a plane type.
 *
 * @param type The plane type to retrieve the string for.
 *
 * @return A pointer to the string representation of the plane type.
 */
const char *srmGetPlaneTypeString(SRM_PLANE_TYPE type);

typedef enum SRM_CONNECTOR_STATE_ENUM SRM_CONNECTOR_STATE;

/**
 * @brief Get a string representation of a connector state.
 *
 * @param state The connector state to retrieve the string for.
 *
 * @return A pointer to the string representation of the connector state.
 */
const char *srmGetConnectorStateString(SRM_CONNECTOR_STATE state);

/**
 * @brief Get a string representation of a connector type.
 *
 * @param type The connector type to retrieve the string for.
 *
 * @return A pointer to the string representation of the connector type.
 */
const char *srmGetConnectorTypeString(UInt32 type);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif // SRMTYPES_H
