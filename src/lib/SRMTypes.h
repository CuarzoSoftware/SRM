#ifndef SRMTYPES_H
#define SRMTYPES_H

/* ENV VARS
 *
 * SRM_DEBUG = [0,1,2,3,4]
 * SRM_EGL_DEBUG = [0,1,2,3,4]
 */

#include <stdint.h>
#include <drm/drm_fourcc.h>

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

struct SRMFormatStruct
{
    UInt32 format;
    UInt64 modifier;
};
typedef struct SRMFormatStruct SRMFormat;

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

enum SRM_BUFFER_FORMAT_ENUM
{
    SRM_BUFFER_FORMAT_XBGR8888 = DRM_FORMAT_XBGR8888,
    SRM_BUFFER_FORMAT_ABGR8888 = DRM_FORMAT_ABGR8888,
    SRM_BUFFER_FORMAT_XRGB8888 = DRM_FORMAT_XRGB8888,
    SRM_BUFFER_FORMAT_ARGB8888 = DRM_FORMAT_ARGB8888
};
typedef enum SRM_BUFFER_FORMAT_ENUM SRM_BUFFER_FORMAT;


const char *srmGetConnectorStateString(SRM_CONNECTOR_STATE state);
const char *srmGetConnectorTypeString(UInt32 type);

#endif // SRMTYPES_H
