#ifndef SRMNAMESPACES_H
#define SRMNAMESPACES_H

/* #define SRM_CONFIG_TESTS 1 */

#include <stdio.h>
#include <stdint.h>
#include <list>

namespace SRM
{
    typedef int8_t   Int8;
    typedef uint8_t  UInt8;
    typedef int32_t  Int32;
    typedef uint32_t UInt32;
    typedef int64_t  Int64;
    typedef uint64_t UInt64;

    class SRMCore;
    class SRMDevice;
    class SRMCrtc;
    class SRMEncoder;
    class SRMPlane;
    class SRMConnector;
    class SRMConnectorMode;
    class SRMListener;
    class SRMLog;

    struct SRMInterface
    {
        int (*openRestricted)(const char *path, int flags, void *data);
        void (*closeRestricted)(int fd, void *data);
    };

    struct SRMConnectorInterface
    {
        void (*initializeGL)(SRMConnector *connector, void *data);
        void (*paintGL)(SRMConnector *connector, void *data);
        void (*resizeGL)(SRMConnector *connector, void *data);
        void (*uninitializeGL)(SRMConnector *connector, void *data);
    };

    enum SRM_RENDER_MODE
    {
        SRM_RENDER_MODE_ITSELF = 0,
        SRM_RENDER_MODE_DUMB = 1,
        SRM_RENDER_MODE_CPU = 2
    };

    const char *getRenderModeString(SRM_RENDER_MODE mode);

    enum SRM_PLANE_TYPE
    {
        SRM_PLANE_TYPE_OVERLAY = 0,
        SRM_PLANE_TYPE_PRIMARY = 1,
        SRM_PLANE_TYPE_CURSOR = 2
    };

    const char *getPlaneTypeString(SRM_PLANE_TYPE type);

    enum SRM_CONNECTOR_STATE
    {
        SRM_CONNECTOR_STATE_UNINITIALIZED = 0,
        SRM_CONNECTOR_STATE_INITIALIZED = 1,
        SRM_CONNECTOR_STATE_UNINITIALIZING = 2,
        SRM_CONNECTOR_STATE_INITIALIZING = 3,
        SRM_CONNECTOR_STATE_CHANGING_MODE = 4
    };

    const char *getConnectorStateString(SRM_CONNECTOR_STATE state);

}

#endif // SRMNAMESPACES_H
