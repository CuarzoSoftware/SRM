#ifndef SRMCORE_H
#define SRMCORE_H

#include "SRMTypes.h"
#include "SRMEGL.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup SRMCore SRMCore
 *
 * @brief Core functionality of SRM.
 *
 * The SRMCore is in charge of setting up all DRM devices, finding the allocator device, defining the rendering mode of each device,
 * letting you listen to UDEV hotplugging events, and more.
 *
 * @warning You should create a single SRMCore instance per process; creating more than one could lead to undefined behavior.
 *
 * @{
 */

/**
 * @brief Structure representing an interface for restricted operations.
 */
typedef struct SRMInterfaceStruct
{
    int (*openRestricted)(const char *path, int flags, void *data); ///< Function pointer to openRestricted function.
    void (*closeRestricted)(int fd, void *data); ///< Function pointer to closeRestricted function.
} SRMInterface;

/**
 * @brief Creates a new SRMCore instance.
 *
 * Creates a new SRMCore instance, which will scan and open all available DRM devices
 * using the provided interface, find the best allocator device, and configure its rendering modes.
 *
 * @param interface A pointer to the SRMInterface that provides access to DRM devices.
 * @param userData  A pointer to user-defined data to associate with the SRMCore instance.
 *
 * @return A pointer to the newly created SRMCore instance on success, or NULL on failure.
 *
 * @note The caller is responsible for releasing the SRMCore instance using srmCoreDestroy() when no longer needed.
 */
SRMCore *srmCoreCreate(SRMInterface *interface, void *userData);

/**
 * @brief Get the user-defined data associated with the SRMCore instance.
 *
 * @param core A pointer to the SRMCore instance.
 *
 * @return A pointer to the user-defined data associated with the SRMCore instance.
 */
void *srmCoreGetUserData(SRMCore *core);

/**
 * @brief Set user-defined data for the SRMCore instance.
 *
 * @param core     A pointer to the SRMCore instance.
 * @param userData A pointer to the user-defined data to associate with the SRMCore instance.
 */
void srmCoreSetUserData(SRMCore *core, void *userData);

/**
 * @brief Get a list of all available devices (SRMDevice*).
 *
 * @param core A pointer to the SRMCore instance.
 *
 * @return A list containing pointers to all available SRMDevice instances.
 */
SRMList *srmCoreGetDevices(SRMCore *core);

/**
 * @brief Get the allocator device.
 *
 * The allocator device is responsible for creating buffers (textures) that can be shared among all devices.
 *
 * @param core A pointer to the SRMCore instance.
 *
 * @return A pointer to the allocator SRMDevice instance.
 */
SRMDevice *srmCoreGetAllocatorDevice(SRMCore *core);

/**
 * @brief Get a pollable udev monitor file descriptor (FD) for listening to hotplugging events.
 *
 * The returned FD can be used to monitor devices and connectors hotplug events using polling mechanisms.
 *
 * @param core A pointer to the SRMCore instance.
 *
 * @return The file descriptor (FD) for monitoring hotplug events.
 */
Int32 srmCoreGetMonitorFD(SRMCore *core);

/**
 * @brief Dispatch pending udev monitor events or block until an event occurs or a timeout is reached.
 *
 * Passing a timeout value of -1 makes the function block indefinitely until an event occurs.
 *
 * @param core       A pointer to the SRMCore instance.
 * @param msTimeout  The timeout value in milliseconds. Use -1 to block indefinitely.
 *
 * @return The result of event processing. Typically, 0 on success, or -1 on error.
 */
Int32 srmCoreProccessMonitor(SRMCore *core, Int32 msTimeout);

/**
 * @brief Registers a new listener to be invoked when a new device (GPU) becomes available.
 *
 * @param core     A pointer to the SRMCore instance.
 * @param callback A callback function to be called when a new device is available.
 * @param userData A pointer to user-defined data to be passed to the callback.
 *
 * @return A pointer to the newly registered SRMListener instance for device creation events.
 */
SRMListener *srmCoreAddDeviceCreatedEventListener(SRMCore *core, void(*callback)(SRMListener*, SRMDevice*), void *userData);

/**
 * @brief Registers a new listener to be invoked when an already available device (GPU) becomes unavailable.
 *
 * @param core     A pointer to the SRMCore instance.
 * @param callback A callback function to be called when a device becomes unavailable.
 * @param userData A pointer to user-defined data to be passed to the callback.
 *
 * @return A pointer to the newly registered SRMListener instance for device removal events.
 */
SRMListener *srmCoreAddDeviceRemovedEventListener(SRMCore *core, void(*callback)(SRMListener*, SRMDevice*), void *userData);

/**
 * @brief Registers a new listener to be invoked when a new connector (screen) is plugged.
 *
 * @param core     A pointer to the SRMCore instance.
 * @param callback A callback function to be called when a new connector is plugged.
 * @param userData A pointer to user-defined data to be passed to the callback.
 *
 * @return A pointer to the newly registered SRMListener instance for connector plugged events.
 */
SRMListener *srmCoreAddConnectorPluggedEventListener(SRMCore *core, void(*callback)(SRMListener*, SRMConnector*), void *userData);

/**
 * @brief Registers a new listener to be invoked when an already plugged connector (screen) is unplugged.
 *
 * @param core     A pointer to the SRMCore instance.
 * @param callback A callback function to be called when a connector is unplugged.
 * @param userData A pointer to user-defined data to be passed to the callback.
 *
 * @return A pointer to the newly registered SRMListener instance for connector unplugged events.
 */
SRMListener *srmCoreAddConnectorUnpluggedEventListener(SRMCore *core, void(*callback)(SRMListener*, SRMConnector*), void *userData);

/**
 * @brief Returns a structure with boolean variables indicating which EGL extensions are available.
 *
 * @param core A pointer to the SRMCore instance.
 *
 * @return A pointer to the SRMEGLCoreExtensions structure indicating the availability of EGL extensions.
 */
const SRMEGLCoreExtensions *srmCoreGetEGLExtensions(SRMCore *core);

/**
 * @brief Returns a structure with pointers to many available EGL functions.
 *
 * @param core A pointer to the SRMCore instance.
 *
 * @return A pointer to the SRMEGLCoreFunctions structure containing pointers to avaliable EGL functions.
 */
const SRMEGLCoreFunctions *srmCoreGetEGLFunctions(SRMCore *core);

/**
 * @brief Get a list of DMA formats supported by all rendering GPUs.
 *
 * @param core A pointer to the SRMCore instance.
 *
 * @return A list containing supported DMA texture formats (SRMFormat *).
 */
SRMList *srmCoreGetSharedDMATextureFormats(SRMCore *core);

/**
 * @brief Uninitializes all initialized connectors, removes all resources associated, closes all DRM devices, and listeners.
 *
 * @note Buffers must be destroyed manually before calling this method.
 *
 * @param core A pointer to the SRMCore instance to be destroyed.
 */
void srmCoreDestroy(SRMCore *core);

/** @} */ // End of SRMCore module

#ifdef __cplusplus
}
#endif

#endif // SRMCORE_H
