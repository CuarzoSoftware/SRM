#ifndef SRMTYPES_H
#define SRMTYPES_H

#define SRM_PAR_CPY 0
#define SRM_MAX_COPY_THREADS 32

#include <stdint.h>
#include <drm_fourcc.h>

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

struct SRMCoreStruct;
/// @ingroup SRMCore
typedef struct SRMCoreStruct SRMCore;

struct SRMDeviceStruct;
/// @ingroup SRMDevice
typedef struct SRMDeviceStruct SRMDevice;

struct SRMCrtcStruct;
/// @ingroup SRMCrtc
typedef struct SRMCrtcStruct SRMCrtc;

struct SRMEncoderStruct;
/// @ingroup SRMEncoder
typedef struct SRMEncoderStruct SRMEncoder;

struct SRMPlaneStruct;
/// @ingroup SRMPlane
typedef struct SRMPlaneStruct SRMPlane;

struct SRMConnectorStruct;
/// @ingroup SRMConnector
typedef struct SRMConnectorStruct SRMConnector;

struct SRMConnectorModeStruct;
/// @ingroup SRMConnectorMode
typedef struct SRMConnectorModeStruct SRMConnectorMode;

struct SRMListStruct;
/// @ingroup SRMList
typedef struct SRMListStruct SRMList;

struct SRMListItemStruct;
/// @ingroup SRMList
typedef struct SRMListItemStruct SRMListItem;

struct SRMListenerStruct;
/// @ingroup SRMListener
typedef struct SRMListenerStruct SRMListener;

struct SRMBufferStruct;
/// @ingroup SRMBuffer
typedef struct SRMBufferStruct SRMBuffer;
typedef UInt32 SRM_BUFFER_FORMAT; ///< A DRM format defined in drm_fourcc.h.
typedef UInt64 SRM_BUFFER_MODIFIER; ///< A DRM format modifier defined in drm_fourcc.h.

/**
 * @brief SRM version data.
 */
typedef struct SRMVersionStruct
{
    UInt32 major;   ///< Major version.
    UInt32 minor;   ///< Minor version.
    UInt32 patch;   ///< Patch version.
    UInt32 build;   ///< Build number.
} SRMVersion;

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

/**
 * @ingroup SRMDevice
 * @brief Enumeration of devices rendering modes.
 */
typedef enum SRM_RENDER_MODE_ENUM
{
    SRM_RENDER_MODE_ITSELF = 0, ///< The device is in "ITSELF" rendering mode.
    SRM_RENDER_MODE_DUMB = 1,   ///< The device is in "DUMB" rendering mode.
    SRM_RENDER_MODE_CPU = 2,    ///< The device is in "CPU" rendering mode.
    SRM_RENDER_MODE_NONE = 3    ///< No rendering mode is defined.
} SRM_RENDER_MODE;

/**
 * @brief Get a string representation of a rendering mode.
 *
 * @param mode The rendering mode to retrieve the string for.
 *
 * @return A pointer to the string representation of the rendering mode.
 */
const char *srmGetRenderModeString(SRM_RENDER_MODE mode);

/**
 * @ingroup SRMPlane
 * @brief Enumeration of plane types.
 */
typedef enum SRM_PLANE_TYPE_ENUM
{
    SRM_PLANE_TYPE_OVERLAY = 0, ///< The plane type is "OVERLAY."
    SRM_PLANE_TYPE_PRIMARY = 1, ///< The plane type is "PRIMARY."
    SRM_PLANE_TYPE_CURSOR = 2   ///< The plane type is "CURSOR."
} SRM_PLANE_TYPE;

/**
 * @brief Get a string representation of a plane type.
 *
 * @param type The plane type to retrieve the string for.
 *
 * @return A pointer to the string representation of the plane type.
 */
const char *srmGetPlaneTypeString(SRM_PLANE_TYPE type);

/**
 * @ingroup SRMConnector
 * @brief Enumeration of connector states.
 */
typedef enum SRM_CONNECTOR_STATE_ENUM
{
    SRM_CONNECTOR_STATE_UNINITIALIZED = 0,  ///< The connector is uninitialized.
    SRM_CONNECTOR_STATE_INITIALIZED = 1,    ///< The connector is initialized.
    SRM_CONNECTOR_STATE_UNINITIALIZING = 2, ///< The connector is in the process of uninitializing.
    SRM_CONNECTOR_STATE_INITIALIZING = 3,   ///< The connector is in the process of initializing.
    SRM_CONNECTOR_STATE_CHANGING_MODE = 4,  ///< The connector is changing display mode.
    SRM_CONNECTOR_STATE_REVERTING_MODE = 5, ///< Special case when changing mode fails and reverts to its previous mode.
    SRM_CONNECTOR_STATE_SUSPENDING = 6,     ///< The connector state is changing from initialized to suspended.
    SRM_CONNECTOR_STATE_SUSPENDED = 7,      ///< The connector is suspended.
    SRM_CONNECTOR_STATE_RESUMING = 8        ///< The connector state is changing from suspended to initialized.
} SRM_CONNECTOR_STATE;

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
 * @ingroup SRMConnector
 * @brief Enumeration of connector subpixel layouts.
 *
 * This enumeration defines different subpixel layouts that can be associated with a connector.
 * Subpixels are individual color elements that make up a pixel on a display. Understanding the subpixel layout
 * is crucial for accurate color interpretation and display.
 */
typedef enum SRM_CONNECTOR_SUBPIXEL_ENUM
{
    /**
     * @brief Unknown subpixel layout.
     */
    SRM_CONNECTOR_SUBPIXEL_UNKNOWN = 1,

    /**
     * @brief Horizontal subpixel layout with RGB order.
     *
     * Red, green, and blue subpixels are arranged horizontally.
     */
    SRM_CONNECTOR_SUBPIXEL_HORIZONTAL_RGB = 2,

    /**
     * @brief Horizontal subpixel layout with BGR order.
     *
     * Blue, green, and red subpixels are arranged horizontally.
     */
    SRM_CONNECTOR_SUBPIXEL_HORIZONTAL_BGR = 3,

    /**
     * @brief Vertical subpixel layout with RGB order.
     *
     * Red, green, and blue subpixels are arranged vertically.
     */
    SRM_CONNECTOR_SUBPIXEL_VERTICAL_RGB = 4,

    /**
     * @brief Vertical subpixel layout with BGR order.
     *
     * Blue, green, and red subpixels are arranged vertically.
     */
    SRM_CONNECTOR_SUBPIXEL_VERTICAL_BGR = 5,

    /**
     * @brief No specific subpixel layout.
     *
     * The connector does not have a well-defined order for subpixels.
     */
    SRM_CONNECTOR_SUBPIXEL_NONE = 6,
} SRM_CONNECTOR_SUBPIXEL;

/**
 * @brief Get a string representation of a connector subpixel layout.
 *
 * @param subpixel The connector subpixel layout to retrieve the string for.
 *
 * @return A pointer to the string representation of the subpixel layout.
 */
const char *srmGetConnectorSubPixelString(SRM_CONNECTOR_SUBPIXEL subpixel);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif // SRMTYPES_H
