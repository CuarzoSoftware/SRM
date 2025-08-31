#ifndef SRMTYPES_H
#define SRMTYPES_H

#include <CZ/Core/Cuarzo.h>

#define SRM_VERSION_MAJOR 1
#define SRM_VERSION_MINOR 0
#define SRM_VERSION_PATCH 0
#define SRM_VERSION_BUILD 1
#define SRM_MAX_BUFFERING 3

namespace CZ
{
    class SRMObject;
    class SRMCore;
    class SRMDevice;
    class SRMEncoder;
    class SRMCrtc;
    class SRMConnector;
    class SRMConnectorMode;
    class SRMPlane;
    class SRMRenderer;

    class SRMAtomicRequest;
    class SRMPropertyBlob;

    /**
     * @brief Interface for OpenGL events handling.
     *
     * The @ref SRMConnectorInterface defines a set of functions for managing various OpenGL events,
     * including initialization, rendering, page flipping, resizing, and uninitialization.
     * This interface is used in the srmConnectorInitialize() function.
     */
    struct SRMConnectorInterface
    {
        /**
         * @brief Notifies that the connector has been initialized.
         *
         * In this event, you should set up shaders, load textures, and perform any necessary setup.
         *
         * @param connector Pointer to the @ref SRMConnector.
         * @param data User data passed in srmConnectorInitialize().
         */
        void (*initializeGL)(SRMConnector *connector, void *data);

        /**
         * @brief Render event.
         *
         * During this event, you should handle all rendering for the current frame.
         *
         * @param connector Pointer to the @ref SRMConnector.
         * @param data User data passed in srmConnectorInitialize().
         */
        void (*paintGL)(SRMConnector *connector, void *data);

        /**
         * @brief Notifies a page flip.
         *
         * This event is triggered when the framebuffer being displayed on the screen changes.
         *
         * @param connector Pointer to the @ref SRMConnector.
         * @param data User data passed in srmConnectorInitialize().
         */
        void (*pageFlipped)(SRMConnector *connector, void *data);

        /**
         * @brief Notifies a change in the framebuffer's dimensions.
         *
         * This event is invoked when the current connector mode changes.
         *
         * @param connector Pointer to the @ref SRMConnector.
         * @param data User data passed in srmConnectorInitialize().
         */
        void (*resizeGL)(SRMConnector *connector, void *data);

        /**
         * @brief Notifies the connector's uninitialization.
         *
         * In this method, you should release resources created during initialization.
         *
         * @param connector Pointer to the @ref SRMConnector.
         * @param data User data passed in srmConnectorInitialize().
         */
        void (*uninitializeGL)(SRMConnector *connector, void *data);
    };
};

#endif // SRMTYPES_H
