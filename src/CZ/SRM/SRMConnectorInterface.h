#ifndef SRMCONNECTORINTERFACE_H
#define SRMCONNECTORINTERFACE_H

#include <CZ/SRM/SRM.h>

namespace CZ
{
    /**
     * @brief Connector interface.
     *
     * This interface defines the lifecycle and rendering events for an SRMConnector.
     *
     * @note Rendering should only be performed inside the @ref paint callback.
     */
    struct SRMConnectorInterface
    {
        /**
         * @brief Notifies that the connector has been successfully initialized.
         *
         * Called after SRMConnector::initialize() completes successfully.
         *
         * @param connector Pointer to the initialized SRMConnector.
         * @param data      User-defined data passed to SRMConnector::initialize().
         */
        void (*initialized)(SRMConnector *connector, void *data);

        /**
         * @brief Paint event callback.
         *
         * Invoked after a call to SRMConnector::repaint(). Use SRMConnector::currentImage()
         * to retrieve the destination image to render into for this frame.
         *
         * Each invocation is followed by either a @ref presented or @ref discarded
         * notification in submission order.
         *
         * @param connector Pointer to the SRMConnector instance.
         * @param data      User-defined data passed to SRMConnector::initialize().
         */
        void (*paint)(SRMConnector *connector, void *data);

        /**
         * @brief Notification that the rendered frame has been presented.
         *
         * Called after the content submitted in a recent @ref paint callback
         * has been successfully displayed.
         *
         * @param connector Pointer to the SRMConnector instance.
         * @param info      Presentation timing information.
         * @param data      User-defined data passed to SRMConnector::initialize().
         */
        void (*presented)(SRMConnector *connector, const CZPresentationTime &info, void *data);

        /**
         * @brief Notification that a rendered frame could not be presented.
         *
         * Called when the frame submitted in a previous @ref paint event was not displayed,
         * for example due to a driver error or state change.
         *
         * @note This does not affect internal damage tracking.
         *
         * @param connector     Pointer to the SRMConnector instance.
         * @param paintEventId  Identifier of the discarded paint event.
         * @param data          User-defined data passed to SRMConnector::initialize().
         */
        void (*discarded)(SRMConnector *connector, UInt64 paintEventId, void *data);

        /**
         * @brief Notifies a change in the image dimensions.
         *
         * Called when the connector’s current mode changes, affecting image size.
         *
         * @param connector Pointer to the SRMConnector instance.
         * @param data      User-defined data passed to SRMConnector::initialize().
         */
        void (*resized)(SRMConnector *connector, void *data);

        /**
         * @brief Notifies that the connector has been uninitialized.
         *
         * @note This callback is only triggered if the @ref initialized callback was previously called.
         *
         * @param connector Pointer to the SRMConnector instance.
         * @param data      User-defined data passed to SRMConnector::initialize().
         */
        void (*uninitialized)(SRMConnector *connector, void *data);
    };
}

#endif // SRMCONNECTORINTERFACE_H
