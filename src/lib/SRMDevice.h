#ifndef SRMDEVICE_H
#define SRMDEVICE_H

#include "SRMTypes.h"

#include <EGL/egl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup SRMDevice SRMDevice
 *
 * @brief Representation of an open DRM device, typically a GPU.
 *
 * An SRMDevice represents an open DRM device, typically a GPU, which can contain multiple connectors
 * and a specific rendering mode.
 *
 * @{
 */

/**
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
 * @brief Get the SRMCore instance to which this device belongs.
 *
 * @param device A pointer to the SRMDevice instance.
 *
 * @return A pointer to the SRMCore instance that this device belongs to.
 */
SRMCore *srmDeviceGetCore(SRMDevice *device);

/**
 * @brief Get the DRM device name (e.g., /dev/dri/card0) associated with this device.
 *
 * @param device A pointer to the SRMDevice instance.
 *
 * @return The name of the DRM device associated with this SRMDevice.
 */
const char *srmDeviceGetName(SRMDevice *device);

/**
 * @brief Get the file descriptor (FD) of the DRM device associated with this device.
 *
 * @param device A pointer to the SRMDevice instance.
 *
 * @return The file descriptor (FD) of the DRM device.
 */
Int32 srmDeviceGetFD(SRMDevice *device);

/**
 * @brief Get the client DRM driver's support for Stereo 3D capability.
 *
 * @param device A pointer to the SRMDevice instance.
 *
 * @return 1 if the client DRM driver supports Stereo 3D capability, 0 otherwise.
 */
UInt8 srmDeviceGetClientCapStereo3D(SRMDevice *device);

/**
 * @brief Get the client DRM driver's support for Universal Planes capability.
 *
 * @param device A pointer to the SRMDevice instance.
 *
 * @return 1 if the client DRM driver supports Universal Planes capability, 0 otherwise.
 */
UInt8 srmDeviceGetClientCapUniversalPlanes(SRMDevice *device);

/**
 * @brief Get the client DRM driver's support for Atomic API capability.
 *
 * @param device A pointer to the SRMDevice instance.
 *
 * @return 1 if the client DRM driver supports Atomic API capability, 0 otherwise.
 *
 * @note Setting the SRM_FORCE_LEGACY_API ENV to 1, disables the Atomic API.
 */
UInt8 srmDeviceGetClientCapAtomic(SRMDevice *device);

/**
 * @brief Get the client DRM driver's support for Aspect Ratio capability.
 *
 * @param device A pointer to the SRMDevice instance.
 *
 * @return 1 if the client DRM driver supports Aspect Ratio capability, 0 otherwise.
 */
UInt8 srmDeviceGetClientCapAspectRatio(SRMDevice *device);

/**
 * @brief Get the client DRM driver's support for Writeback Connectors capability.
 *
 * @param device A pointer to the SRMDevice instance.
 *
 * @return 1 if the client DRM driver supports Writeback Connectors capability, 0 otherwise.
 */
UInt8 srmDeviceGetClientCapWritebackConnectors(SRMDevice *device);


/**
 * @brief Get the driver's support for Dumb Buffer capability.
 *
 * @param device A pointer to the SRMDevice instance.
 *
 * @return 1 if the DRM device driver supports Dumb Buffer capability, 0 otherwise.
 */
UInt8 srmDeviceGetCapDumbBuffer(SRMDevice *device);

/**
 * @brief Get the driver's support for Prime Import capability.
 *
 * @param device A pointer to the SRMDevice instance.
 *
 * @return 1 if the DRM device driver supports Prime Import capability, 0 otherwise.
 */
UInt8 srmDeviceGetCapPrimeImport(SRMDevice *device);

/**
 * @brief Get the driver's support for Prime Export capability.
 *
 * @param device A pointer to the SRMDevice instance.
 *
 * @return 1 if the DRM device driver supports Prime Export capability, 0 otherwise.
 */
UInt8 srmDeviceGetCapPrimeExport(SRMDevice *device);

/**
 * @brief Get the driver's support for Add FB2 Modifiers capability.
 *
 * @param device A pointer to the SRMDevice instance.
 *
 * @return 1 if the DRM device driver supports Add FB2 Modifiers capability, 0 otherwise.
 */
UInt8 srmDeviceGetCapAddFb2Modifiers(SRMDevice *device);

/**
 * @brief Get the driver's support for Async Page Flip capability.
 *
 * @param device A pointer to the SRMDevice instance.
 *
 * @return 1 if the DRM device driver supports Async Page Flip capability, 0 otherwise.
 */
UInt8 srmDeviceGetCapAsyncPageFlip(SRMDevice *device);

/**
 * @brief Check if the device can perform rendering.
 *
 * This function returns true when the device can import buffers from the allocator device,
 * which also means it can perform rendering for other non-rendering devices.
 *
 * @param device A pointer to the SRMDevice instance.
 *
 * @return 1 if the device is capable of rendering, 0 otherwise.
 */
UInt8 srmDeviceIsRenderer(SRMDevice *device);

/**
 * @brief Get the device that performs rendering for this device.
 *
 * If the rendering device is equal to this device, then this device's rendering mode is ITSELF mode.
 * Otherwise, this device's rendering mode could be DUMB or CPU mode, depending on whether it can create DUMB buffers.
 *
 * @param device A pointer to the SRMDevice instance.
 *
 * @return A pointer to the SRMDevice that performs rendering for this device.
 */
SRMDevice *srmDeviceGetRendererDevice(SRMDevice *device);

/**
 * @brief Get the rendering mode of the device.
 *
 * If the rendering device is equal to this device, then this device's rendering mode is ITSELF mode.
 * Otherwise, this device's rendering mode could be DUMB or CPU mode, depending on whether it can create DUMB buffers.
 *
 * @param device A pointer to the SRMDevice instance.
 *
 * @return The rendering mode of the device (SRM_RENDER_MODE).
 */
SRM_RENDER_MODE srmDeviceGetRenderMode(SRMDevice *device);

/**
 * @brief Get a list of CRTCs (Cathode Ray Tube Controllers) of this device.
 *
 * @param device A pointer to the SRMDevice instance.
 *
 * @return A list of pointers to the CRTCs (SRMCrtc*) associated with this device.
 */
SRMList *srmDeviceGetCrtcs(SRMDevice *device);

/**
 * @brief Get a list of encoders of this device.
 *
 * @param device A pointer to the SRMDevice instance.
 *
 * @return A list of pointers to the encoders (SRMEncoder*) associated with this device.
 */
SRMList *srmDeviceGetEncoders(SRMDevice *device);

/**
 * @brief Get a list of planes of this device.
 *
 * @param device A pointer to the SRMDevice instance.
 *
 * @return A list of pointers to the planes (SRMPlane*) associated with this device.
 */
SRMList *srmDeviceGetPlanes(SRMDevice *device);

/**
 * @brief Get a list of connectors of this device.
 *
 * @param device A pointer to the SRMDevice instance.
 *
 * @return A list of pointers to the connectors (SRMConnector*) associated with this device.
 */
SRMList *srmDeviceGetConnectors(SRMDevice *device);

/**
 * @brief Get a list of DMA (Direct Memory Access) texture formats supported by the device.
 *
 * DMA texture formats are used for efficient memory transfers between devices and are typically
 * supported for hardware acceleration purposes.
 *
 * @param device A pointer to the SRMDevice instance.
 *
 * @return A list of DMA texture formats supported by the device.
 */
SRMList *srmDeviceGetDMATextureFormats(SRMDevice *device);

/**
 * @brief Get a list of DMA (Direct Memory Access) render formats supported by the device.
 *
 * DMA render formats are used for efficient rendering operations, such as texture rendering, and
 * are typically supported for hardware acceleration purposes.
 *
 * @param device A pointer to the SRMDevice instance.
 *
 * @return A list of DMA render formats supported by the device.
 */
SRMList *srmDeviceGetDMARenderFormats(SRMDevice *device);

/**
 * @brief Get the EGLDisplay associated with the device.
 *
 * The EGLDisplay is an EGL display connection that represents the device for EGL rendering contexts.
 *
 * @param device A pointer to the SRMDevice instance.
 *
 * @return A pointer to the EGLDisplay associated with the device.
 */
EGLDisplay *srmDeviceGetEGLDisplay(SRMDevice *device);

/**
 * @brief Get the EGLContext associated with the device.
 *
 * The EGLContext represents the rendering context associated with the device for EGL rendering operations.
 *
 * @param device A pointer to the SRMDevice instance.
 *
 * @return A pointer to the EGLContext associated with the device.
 */
EGLContext *srmDeviceGetEGLContext(SRMDevice *device);

// TODO: Enabling /disabling GPUs on the fly is still unsupported
UInt8 srmDeviceSetEnabled(SRMDevice *device, UInt8 enabled);
UInt8 srmDeviceIsEnabled(SRMDevice *device);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif // SRMDEVICE_H
