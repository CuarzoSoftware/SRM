#ifndef SRMCORE_H
#define SRMCORE_H

#include <SRMTypes.h>
#include <SRMEGL.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup SRMCore SRMCore
 *
 * @brief Core functionality of SRM.
 *
 * The @ref SRMCore is in charge of setting up all DRM devices, finding the best allocator device, defining the rendering mode of each device,
 * letting you listen to udev hotplugging events, and more.
 *
 * @warning You must create a single @ref SRMCore instance per process, creating more than one could lead to undefined behavior.
 *
 * ### Framebuffer damage
 *
 * **PRIME**, **DUMB** and **CPU** modes can greatly benefit from receiving information about the changes occurring in the buffer within a frame, commonly known as "damage." By providing this damage information, we can optimize the performance of these modes.
 * To define the generated damage, after rendering within a `paintGL()` event, you can add an array of rectangles (@ref SRMRect) with the damage area using the srmConnectorSetBufferDamage() function. 
 * It is important to ensure that the coordinates of these rectangles originate from the top-left corner of the framebuffer and do not extend beyond its boundaries to avoid segmentation errors.
 * The **ITSELF** mode does not benefit from buffer damages, and therefore, calling the function in that case is a no-op. To determine if a connector supports buffer damages, use the srmConnectorHasBufferDamageSupport() function.
 * @{
 */

/**
 * @brief Interface for managing DRM devices (/dev/dri/card*).
 * 
 * SRM provides this interface for opening and closing DRM devices.\n
 * Rather than relying solely on the `open()` and `close()` functions, you have the option to use [libseat](https://github.com/kennylevinsen/seatd) for enabling multi-session support.\n
 * You can refer to the [srm-multi-session](md_md__examples.html) example to see how to enable multi-session support with [libseat](https://github.com/kennylevinsen/seatd).
 */
typedef struct SRMInterfaceStruct
{
    /**
     * Function to open a DRM device. Must return the opened DRM device file descriptor.
     *
     * @param path The path to the DRM device file (e.g., `/dev/dri/card0`).
     * @param flags Flags for opening the DRM device (e.g., `O_RDWR`).
     * @param data A pointer to the user data provided in srmCoreCreate().
     * @return The file descriptor of the opened DRM device.
     */
    int (*openRestricted)(const char *path, int flags, void *data);

    /**
     * Function to close a DRM device file descriptor.
     *
     * @param fd The file descriptor of the DRM device to close.
     * @param data A pointer to the user data provided in srmCoreCreate().
     */
    void (*closeRestricted)(int fd, void *data);
} SRMInterface;

/**
 * @brief Creates a new @ref SRMCore instance.
 *
 * Creates a new @ref SRMCore instance, which will scan and open all available DRM devices
 * using the provided interface, find the best allocator device, and configure its rendering modes.
 *
 * @param interface A pointer to the @ref SRMInterface that provides access to DRM devices.
 * @param userData  A pointer to the user data to associate with the @ref SRMCore instance.
 *
 * @return A pointer to the newly created @ref SRMCore instance on success, or `NULL` on failure.
 *
 * @note The caller is responsible for releasing the @ref SRMCore instance using srmCoreDestroy() when no longer needed.
 */
SRMCore *srmCoreCreate(SRMInterface *interface, void *userData);

/**
 * @brief Temporarily suspends SRM.
 *
 * This function temporarily suspends all connector rendering threads and evdev events within SRM.\n
 * It should be used when switching to another session in a multi-session system.\n
 * While the core is suspended, SRM no longer acts as the DRM master, and KMS operations cannot be performed.\n
 * For guidance on enabling multi-session functionality using libseat, please refer to the [srm-multi-session](md_md__examples.html) example.
 *
 * @note Pending hotplugging events will be emitted once the core is resumed.
 *
 * @param core A pointer to the @ref SRMCore instance.
 * @return Returns 1 on success and 0 if the operation fails.
 */
UInt8 srmCoreSuspend(SRMCore *core);

/**
 * @brief Resumes SRM.
 *
 * This function resumes a previously suspended @ref SRMCore, allowing connectors rendering threads
 * and evdev events to continue processing. It should be used after calling srmCoreSuspend()
 * to bring the @ref SRMCore back to an active state.
 *
 * @param core A pointer to the @ref SRMCore instance to resume.
 * @return Returns 1 on success and 0 if the operation fails.
 */
UInt8 srmCoreResume(SRMCore *core);

/**
 * @brief Check if @ref SRMCore is currently suspended.
 *
 * This function checks whether an @ref SRMCore instance is currently in a suspended state,
 * meaning that connector rendering threads and evdev events are temporarily halted.
 *
 * @param core A pointer to the @ref SRMCore instance to check.
 * @return Returns 1 if the @ref SRMCore is suspended, and 0 if it is active.
 */
UInt8 srmCoreIsSuspended(SRMCore *core);

/**
 * @brief Get the user data associated with the @ref SRMCore instance.
 *
 * @param core A pointer to the @ref SRMCore instance.
 * 
 * @note This is the same user data passed in srmCoreCreate().
 *
 * @return A pointer to the user data associated with the @ref SRMCore instance.
 */
void *srmCoreGetUserData(SRMCore *core);

/**
 * @brief Set user-defined data for the @ref SRMCore instance.
 *
 * @param core A pointer to the @ref SRMCore instance.
 * @param userData A pointer to the user data to associate with the @ref SRMCore instance.
 * 
 * @note This replaces the user data passed in srmCoreCreate().
 */
void srmCoreSetUserData(SRMCore *core, void *userData);

/**
 * @brief Get a list of all available devices (@ref SRMDevice).
 *
 * @param core A pointer to the @ref SRMCore instance.
 *
 * @return A list containing all available @ref SRMDevice instances.
 */
SRMList *srmCoreGetDevices(SRMCore *core);

/**
 * @brief Get the SRM version of the core.
 *
 * @param core A pointer to the @ref SRMCore instance.
 *
 * @return A pointer to the core's @ref SRMVersion instance.
 */
SRMVersion *srmCoreGetVersion(SRMCore *core);

/**
 * @brief Get the allocator device.
 *
 * The allocator device is responsible for creating buffers (textures) that can be shared among all devices.
 *
 * @param core A pointer to the @ref SRMCore instance.
 *
 * @return A pointer to the allocator @ref SRMDevice instance.
 */
SRMDevice *srmCoreGetAllocatorDevice(SRMCore *core);

/**
 * @brief Get a pollable udev monitor file descriptor for listening to hotplugging events.
 *
 * The returned fd can be used to monitor devices and connectors hotplugging events using polling mechanisms.\n
 * Use srmCoreProcessMonitor() to dispatch pending events.
 *
 * @param core A pointer to the @ref SRMCore instance.
 *
 * @return The file descriptor for monitoring hotplugging events.
 */
Int32 srmCoreGetMonitorFD(SRMCore *core);

/**
 * @brief Dispatch pending udev monitor events or block until an event occurs or a timeout is reached.
 *
 * Passing a timeout value of -1 makes the function block indefinitely until an event occurs.
 *
 * @param core       A pointer to the @ref SRMCore instance.
 * @param msTimeout  The timeout value in milliseconds. Use -1 to block indefinitely.
 *
 * @return (>= 0) on success, or -1 on error.
 */
Int32 srmCoreProcessMonitor(SRMCore *core, Int32 msTimeout);

/**
 * @brief Registers a new listener to be invoked when a new device (GPU) becomes available.
 *
 * @param core     A pointer to the @ref SRMCore instance.
 * @param callback A callback function to be called when a new device is available.
 * @param userData A pointer to user-defined data to be passed to the callback.
 *
 * @return A pointer to the newly registered @ref SRMListener instance for device creation events.
 */
SRMListener *srmCoreAddDeviceCreatedEventListener(SRMCore *core, void(*callback)(SRMListener*, SRMDevice*), void *userData);

/**
 * @brief Registers a new listener to be invoked when an already available device (GPU) becomes unavailable.
 *
 * @param core     A pointer to the @ref SRMCore instance.
 * @param callback A callback function to be called when a device becomes unavailable.
 * @param userData A pointer to user-defined data to be passed to the callback.
 *
 * @return A pointer to the newly registered @ref SRMListener instance for device removal events.
 */
SRMListener *srmCoreAddDeviceRemovedEventListener(SRMCore *core, void(*callback)(SRMListener*, SRMDevice*), void *userData);

/**
 * @brief Registers a new listener to be invoked when a new connector is plugged.
 *
 * @param core     A pointer to the @ref SRMCore instance.
 * @param callback A callback function to be called when a new connector is plugged.
 * @param userData A pointer to user-defined data to be passed to the callback.
 *
 * @return A pointer to the newly registered @ref SRMListener instance for connector plugged events.
 */
SRMListener *srmCoreAddConnectorPluggedEventListener(SRMCore *core, void(*callback)(SRMListener*, SRMConnector*), void *userData);

/**
 * @brief Registers a new listener to be invoked when an already plugged connector is unplugged.
 *
 * @param core     A pointer to the @ref SRMCore instance.
 * @param callback A callback function to be called when a connector is unplugged.
 * @param userData A pointer to user-defined data to be passed to the callback.
 *
 * @return A pointer to the newly registered @ref SRMListener instance for connector unplugged events.
 */
SRMListener *srmCoreAddConnectorUnpluggedEventListener(SRMCore *core, void(*callback)(SRMListener*, SRMConnector*), void *userData);

/**
 * @brief Returns a structure with boolean variables indicating which EGL extensions are available.
 *
 * @param core A pointer to the @ref SRMCore instance.
 *
 * @return A pointer to the @ref SRMEGLCoreExtensions structure indicating the availability of EGL extensions.
 */
const SRMEGLCoreExtensions *srmCoreGetEGLExtensions(SRMCore *core);

/**
 * @brief Returns a structure with pointers to many available EGL functions.
 *
 * @param core A pointer to the @ref SRMCore instance.
 *
 * @return A pointer to the @ref SRMEGLCoreFunctions structure containing pointers to avaliable EGL functions.
 */
const SRMEGLCoreFunctions *srmCoreGetEGLFunctions(SRMCore *core);

/**
 * @brief Get a list of DMA formats supported by all rendering GPUs.
 *
 * @param core A pointer to the @ref SRMCore instance.
 *
 * @return A list containing supported DMA texture formats (@ref SRMFormat).
 */
SRMList *srmCoreGetSharedDMATextureFormats(SRMCore *core);

/**
 * @brief Uninitializes all initialized connectors, removes all resources associated, closes all DRM devices, and listeners.
 *
 * @note Buffers must be destroyed manually before calling this method.
 *
 * @param core A pointer to the @ref SRMCore instance to be destroyed.
 */
void srmCoreDestroy(SRMCore *core);

/** @} */ // End of SRMCore module

#ifdef __cplusplus
}
#endif

#endif // SRMCORE_H
