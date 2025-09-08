#ifndef SRMTYPES_H
#define SRMTYPES_H

#include <CZ/Ream/RPresentationTime.h>
#include <CZ/Cuarzo.h>

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
         * @brief Rendering callback for the current frame.
         *
         * This function is invoked some time after repaint() is called.
         * Implement this callback to perform all rendering operations for a frame.
         *
         * For each invocation of this callback, a corresponding notification
         * either @ref presented or @ref discarded will be issued later, in submission order.
         *
         * @param connector Pointer to the @ref SRMConnector instance.
         * @param data      User-defined data passed during srmConnectorInitialize().
         */
        void (*paintGL)(SRMConnector *connector, void *data);

        /**
         * @brief Notification that the rendered frame has been presented.
         *
         * Called after the content submitted in a recent @ref paintGL
         * callback has been successfully presented to the display.
         *
         * @param connector Pointer to the @ref SRMConnector instance.
         * @param info      Presentation timing information.
         * @param data      User-defined data passed during srmConnectorInitialize().
         */
        void (*presented)(SRMConnector *connector, const RPresentationTime &info, void *data);

        /**
         * @brief Notification that a rendered frame was not presented.
         *
         * Called when the content submitted in a previous @ref paintGL callback
         * could not be presented (e.g., due to driver issues).
         *
         * @param connector     Pointer to the @ref SRMConnector instance.
         * @param paintEventId  Identifier of the discarded paintGL event.
         * @param data          User-defined data passed during srmConnectorInitialize().
         */
        void (*discarded)(SRMConnector *connector, UInt64 paintEventId, void *data);

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
