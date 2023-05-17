#ifndef SRMTYPES_H
#define SRMTYPES_H

#include <stdint.h>
#include <drm/drm_fourcc.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ENV VARS
 *
 * SRM_DEBUG = [0,1,2,3,4]
 * SRM_EGL_DEBUG = [0,1,2,3,4]
 */

#define SRM_UNUSED(var) (void)var

typedef int8_t   Int8;
typedef uint8_t  UInt8;
typedef int32_t  Int32;
typedef uint32_t UInt32;
typedef int64_t  Int64;
typedef uint64_t UInt64;

struct SRMCoreStruct;
typedef struct SRMCoreStruct SRMCore;

struct SRMDeviceStruct;
typedef struct SRMDeviceStruct SRMDevice;

struct SRMCrtcStruct;
typedef struct SRMCrtcStruct SRMCrtc;

struct SRMEncoderStruct;
typedef struct SRMEncoderStruct SRMEncoder;

struct SRMPlaneStruct;
typedef struct SRMPlaneStruct SRMPlane;

struct SRMConnectorStruct;
typedef struct SRMConnectorStruct SRMConnector;

struct SRMConnectorModeStruct;
typedef struct SRMConnectorModeStruct SRMConnectorMode;

struct SRMListStruct;
typedef struct SRMListStruct SRMList;

struct SRMListItemStruct;
typedef struct SRMListItemStruct SRMListItem;

struct SRMListenerStruct;
typedef struct SRMListenerStruct SRMListener;

struct SRMBufferStruct;
typedef struct SRMBufferStruct SRMBuffer;

struct SRMEGLCoreExtensionsStruct;
typedef struct SRMEGLCoreExtensionsStruct SRMEGLCoreExtensions;

struct SRMEGLCoreFunctionsStruct;
typedef struct SRMEGLCoreFunctionsStruct SRMEGLCoreFunctions;

struct SRMEGLDeviceExtensionsStruct;
typedef struct SRMEGLDeviceExtensionsStruct SRMEGLDeviceExtensions;

struct SRMEGLDeviceFunctionsStruct;
typedef struct SRMEGLDeviceFunctionsStruct SRMEGLDeviceFunctions;

struct SRMFormatStruct;
typedef struct SRMFormatStruct SRMFormat;

struct SRMGLFormatStruct;
typedef struct SRMGLFormatStruct SRMGLFormat;

struct SRMInterfaceStruct
{
    int (*openRestricted)(const char *path, int flags, void *data);
    void (*closeRestricted)(int fd, void *data);
};
typedef struct SRMInterfaceStruct SRMInterface;

struct SRMConnectorInterfaceStruct
{
    void (*initializeGL)(SRMConnector *connector, void *data);
    void (*paintGL)(SRMConnector *connector, void *data);
    void (*pageFlipped)(SRMConnector *connector, void *data);
    void (*resizeGL)(SRMConnector *connector, void *data);
    void (*uninitializeGL)(SRMConnector *connector, void *data);
};
typedef struct SRMConnectorInterfaceStruct SRMConnectorInterface;

enum SRM_RENDER_MODE_ENUM
{
    SRM_RENDER_MODE_ITSELF = 0,
    SRM_RENDER_MODE_DUMB = 1,
    SRM_RENDER_MODE_CPU = 2,
    SRM_RENDER_MODE_NONE = 3
};
typedef enum SRM_RENDER_MODE_ENUM SRM_RENDER_MODE;

const char *srmGetRenderModeString(SRM_RENDER_MODE mode);

enum SRM_PLANE_TYPE_ENUM
{
    SRM_PLANE_TYPE_OVERLAY = 0,
    SRM_PLANE_TYPE_PRIMARY = 1,
    SRM_PLANE_TYPE_CURSOR = 2
};
typedef enum SRM_PLANE_TYPE_ENUM SRM_PLANE_TYPE;

const char *srmGetPlaneTypeString(SRM_PLANE_TYPE type);

enum SRM_CONNECTOR_STATE_ENUM
{
    SRM_CONNECTOR_STATE_UNINITIALIZED = 0,
    SRM_CONNECTOR_STATE_INITIALIZED = 1,
    SRM_CONNECTOR_STATE_UNINITIALIZING = 2,
    SRM_CONNECTOR_STATE_INITIALIZING = 3,
    SRM_CONNECTOR_STATE_CHANGING_MODE = 4,
    SRM_CONNECTOR_STATE_REVERTING_MODE = 5 // Special case when changing mode fails
};
typedef enum SRM_CONNECTOR_STATE_ENUM SRM_CONNECTOR_STATE;

typedef UInt32 SRM_BUFFER_FORMAT;
typedef UInt64 SRM_BUFFER_MODIFIER;

const char *srmGetConnectorStateString(SRM_CONNECTOR_STATE state);
const char *srmGetConnectorTypeString(UInt32 type);

#ifdef __cplusplus
}
#endif

#endif // SRMTYPES_H
