#ifndef SRMBUFFER_H
#define SRMBUFFER_H

#include <SRMTypes.h>
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <gbm.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup SRMBuffer SRMBuffer
 * 
 * @brief OpenGL texture shared across GPUs.
 *
 * An @ref SRMBuffer encapsulates an OpenGL ES 2.0 texture that is shared across multiple GPUs.
 * It can be created from a variety of sources including DMA planes, main memory, GBM bo buffers,
 * and more. This module provides functions to manage and use such shared textures.
 * 
 * @note Buffers can be shared across all GPUs only if they adhere to the formats and modifiers 
 *       specified in @ref srmCoreGetSharedDMATextureFormats.
 *
 * @addtogroup SRMBuffer
 * @{
 */

/**
 * @brief Max number of planes supported by a DMA buffer.
 */
#define SRM_MAX_PLANES 4

/**
 * @brief Enumerates the capabilities of an @ref SRMBuffer.
 */
typedef enum SRM_BUFFER_CAP_ENUM
{
    /**
     * Capability to read from the buffer.
     */
    SRM_BUFFER_CAP_READ = 1,

    /**
     * Capability to write to the buffer.
     */
    SRM_BUFFER_CAP_WRITE = 2,

    /**
     * Capability to map the buffer.
     */
    SRM_BUFFER_CAP_MAP = 4
} SRM_BUFFER_CAP;

/**
 * @brief Enumerates the possible sources of an @ref SRMBuffer.
 */
enum SRM_BUFFER_SRC
{
    /**
     * The buffer source is main memory.
     */
    SRM_BUFFER_SRC_CPU,

    /**
     * The buffer source are DMA planes.
     */
    SRM_BUFFER_SRC_DMA,

    /**
     * The buffer source is a Wayland `wl_drm` buffer.
     */
    SRM_BUFFER_SRC_WL_DRM,

    /**
     * The buffer source is a bo from the Generic Buffer Manager (GBM) library.
     */
    SRM_BUFFER_SRC_GBM,

    /**
     * The buffer source is an already existing OpenGL texture.
     */
    SRM_BUFFER_SRC_GL
};

/**
 * @brief Structure holding DMA planes data for an @ref SRMBuffer.
 * 
 * Use this structure to create SRM buffers from DMA planes data using the srmBufferCreateFromDMA() function.
 * 
 * @note It's important to clarify that DMA planes are distinct from DRM planes (@ref SRMPlane). 
 *       Be cautious not to conflate these concepts.
 */
typedef struct SRMBufferDMADataStruct
{
    /**
     * Width of the buffer.
     */
    UInt32 width;

    /**
     * Height of the buffer.
     */
    UInt32 height;

    /**
     * Format of the buffer.
     */
    UInt32 format;

    /**
     * Number of file descriptors (FDs) associated with the buffer.
     */
    UInt32 num_fds;

    /**
     * Array of FDs associated with the buffer's planes.
     */
    Int32 fds[SRM_MAX_PLANES];

    /**
     * Array of stride values for each plane of the buffer.
     */
    UInt32 strides[SRM_MAX_PLANES];

    /**
     * Array of offset values for each plane of the buffer.
     */
    UInt32 offsets[SRM_MAX_PLANES];

    /**
     * Array of modifier values for each plane of the buffer.
     */
    UInt64 modifiers[SRM_MAX_PLANES];
} SRMBufferDMAData;

/**
 * @brief Creates an @ref SRMBuffer from a GBM bo.
 *
 * This function creates an @ref SRMBuffer object using the provided 
 * [Generic Buffer Management (GBM)](https://en.wikipedia.org/wiki/Mesa_(computer_graphics)#Generic_Buffer_Management) buffer object.
 *
 * @warning The `struct gbm_bo` must remain valid during the buffer lifetime. It is not destroyed when
 *          the buffer is destroyed and must be destroyed manually.
 *
 * @note Depending on the backing storage of the bo, the buffer may not be shareable across all devices.
 *
 * @param core Pointer to the @ref SRMCore instance.
 * @param bo Pointer to the GBM buffer object.
 * @return A pointer to the created @ref SRMBuffer, or `NULL` on failure.
 */
SRMBuffer *srmBufferCreateFromGBM(SRMCore *core, struct gbm_bo *bo);

/**
 * @brief Creates an @ref SRMBuffer from Direct Memory Access (DMA) planes.
 *
 * This function creates an @ref SRMBuffer object using the provided DMA planes data, which includes information
 * about width, height, format, stride of planes, and more.
 * 
 * @param core Pointer to the @ref SRMCore instance.
 * @param allocator Pointer to the @ref SRMDevice responsible for memory allocation. 
 *                  Use `NULL` to enable texture sharing across all devices (GPUs).
 * @param dmaPlanes Pointer to the SRMBufferDMAData containing DMA planes data.
 * 
 * @note When `NULL` is passed as the allocator device, srmCoreGetAllocatorDevice() is used.\n
 *       Buffers created from this device are shared across all GPUs and can be used for rendering in all connectors.
 * 
 * @return A pointer to the created @ref SRMBuffer, or `NULL` on failure.
 */
SRMBuffer *srmBufferCreateFromDMA(SRMCore *core, SRMDevice *allocator, SRMBufferDMAData *dmaPlanes);

/**
 * @brief Creates an @ref SRMBuffer from main memory buffer.
 *
 * This function creates an @ref SRMBuffer object using a main memory buffer, allowing you to define
 * the buffer's width, height, stride, pixel data, and format.
 * 
 * @param core Pointer to the @ref SRMCore instance.
 * @param allocator Pointer to the @ref SRMDevice responsible for memory allocation. 
 *                  Use `NULL` to enable texture sharing across all devices (GPUs).
 * @param width Width of the src buffer.
 * @param height Height of the src buffer.
 * @param stride Stride (row pitch) of the src buffer in bytes.
 * @param pixels Pointer to the pixel data.
 * @param format Format of the pixel data.
 * 
 * @note When `NULL` is passed as the allocator device, srmCoreGetAllocatorDevice() is used.\n
 *       Buffers created from this device are shared across all GPUs and can be used for rendering in all connectors.
 * 
 * @return A pointer to the created @ref SRMBuffer, or `NULL` on failure.
 */
SRMBuffer *srmBufferCreateFromCPU(SRMCore *core, SRMDevice *allocator,
                                  UInt32 width, UInt32 height, UInt32 stride,
                                  const void *pixels, SRM_BUFFER_FORMAT format);

/**
 * @brief Creates an @ref SRMBuffer from a Wayland `wl_drm` buffer.
 *
 * This function creates an @ref SRMBuffer object using the provided Wayland `wl_drm` buffer.
 *
 * @note Depending on the backing storage of the bo, the buffer may not be shared across all devices.
 *
 * @param core Pointer to the @ref SRMCore instance.
 * @param wlBuffer Pointer to the Wayland DRM buffer.
 * @return A pointer to the created @ref SRMBuffer, or `NULL` on failure.
 */
SRMBuffer *srmBufferCreateFromWaylandDRM(SRMCore *core, void *wlBuffer);

/**
 * @brief Creates a wrapper for an already created OpenGL texture.
 *
 * This function allows you to use an existing OpenGL texture within the SRM framework.
 *
 * @note The texture can only be used by the provided @ref SRMDevice.
 *
 * @param device The @ref SRMDevice whose `EGLDisplay` was used to create the texture.
 * @param id The OpenGL texture ID of the existing texture.
 * @param target The target for the texture (e.g., `GL_TEXTURE_2D` or `GL_TEXTURE_EXTERNAL_OES`).
 * @param format The DRM format of the texture.
 * @param width The width of the texture in pixels.
 * @param height The height of the texture in pixels.
 * @param transferOwnership If non-zero, the texture will be destroyed when @ref srmBufferDestroy() is called.
 * @return A pointer to the created @ref SRMBuffer, or `NULL` on failure.
 */
SRMBuffer *srmBufferCreateGLTextureWrapper(SRMDevice *device, GLuint id, GLenum target, SRM_BUFFER_FORMAT format,
                                           UInt32 width, UInt32 height, UInt8 transferOwnership);

/**
 * @brief Destroys an SRMBuffer.
 *
 * This function destroys an @ref SRMBuffer, freeing associated resources.
 * 
 * @param buffer Pointer to the @ref SRMBuffer to be destroyed.
 */
void srmBufferDestroy(SRMBuffer *buffer);

/**
 * @brief Retrieves an OpenGL texture ID associated with an @ref SRMBuffer for a specific device (GPU).
 *
 * This function retrieves the OpenGL texture ID that corresponds to the given @ref SRMBuffer
 * for the specified device (GPU).
 * 
 * @note You probably will call this function when doing rendering on a connector.
 *       To get the connector's renderer device, use srmConnectorGetRendererDevice().
 *
 * @param device Pointer to the @ref SRMDevice representing the specific device (GPU).
 * @param buffer Pointer to the @ref SRMBuffer for which the texture ID is requested.
 * @return The OpenGL texture ID associated with the @ref SRMBuffer and device, or 0 on failure.
 */
GLuint srmBufferGetTextureID(SRMDevice *device, SRMBuffer *buffer);

/**
 * @brief Gets the OpenGL texture target associated with an @ref SRMBuffer.
 *
 * This function retrieves the OpenGL texture target associated with the provided @ref SRMBuffer.
 * 
 * For textures using the `GL_TEXTURE_EXTERNAL_OES` target, rendering requires the utilization of a 
 * custom shader, as demonstrated in the following example:
 * 
 * ### Fragment shader example
 * 
 * @code
 * #extension GL_OES_EGL_image_external : require
 * uniform samplerExternalOES texture;
 * varying vec2 v_texcoord;
 *
 * void main()
 * {
 *    gl_FragColor = texture2D(texture, v_texcoord);
 * }
 * @endcode
 *
 * @param buffer Pointer to the @ref SRMBuffer for which the texture target is needed.
 * @return The associated OpenGL texture target, which can be either `GL_TEXTURE_2D` or `GL_TEXTURE_EXTERNAL_OES`.
 */
GLenum srmBufferGetTextureTarget(SRMBuffer *buffer);

/**
 * @brief Gets an `EGLImage` associated with a specific device.
 *
 * The returned image is owned by the SRMBuffer and should not be destroyed manually.
 *
 * @param device The device whose `EGLDisplay` is used to create the image.
 * @param buffer Pointer to the @ref SRMBuffer.
 *
 * @return An `EGLImage` associated with the SRMDevice or `EGL_NO_IMAGE` on failure.
 */
EGLImage srmBufferGetEGLImage(SRMDevice *device, SRMBuffer *buffer);

/**
 * @brief Writes pixel data to an @ref SRMBuffer.
 *
 * This function writes pixel data to the specified region of an @ref SRMBuffer.
 *
 * @note The source and destination buffer pixel format must match.
 * 
 * @param buffer Pointer to the @ref SRMBuffer to write data to.
 * @param stride Stride of the source pixel data.
 * @param dstX X-coordinate of the top-left corner of the destination region.
 * @param dstY Y-coordinate of the top-left corner of the destination region.
 * @param dstWidth Width of the destination region.
 * @param dstHeight Height of the destination region.
 * @param pixels Pointer within the source buffer to copy from (not affected by dstX and dstY).
 * @return 1 on success, or 0 on failure.
 */
UInt8 srmBufferWrite(SRMBuffer *buffer,
                     UInt32 stride,
                     UInt32 dstX,
                     UInt32 dstY,
                     UInt32 dstWidth,
                     UInt32 dstHeight,
                     const void *pixels);

/**
 * @brief Prepares an @ref SRMBuffer for writing.
 *
 * The srmBufferWrite2 functions provide the same functionality as srmBufferWrite(),
 * but they allow for multiple writes without requiring immediate internal syncing,
 * yielding better performance.
 *
 * If 1 is returned, calling any function except for srmBufferWrite2Update()
 * or srmBufferWrite2End() will cause a deadlock.
 *
 * If 0 is returned, it means it failed, and neither srmBufferWrite2Update()
 * nor srmBufferWrite2End() should be called.
 *
 * @param buffer Pointer to the @ref SRMBuffer to write data to.
 * @return 1 on success, or 0 on failure.
 */
UInt8 srmBufferWrite2Begin(SRMBuffer *buffer);

/**
 * @brief Writes pixel data to the buffer.
 *
 * This function can only be called if srmBufferWrite2Begin() was previously called and returned 1.
 * The parameters are the same as those defined in srmBufferWrite().
 *
 * @return 1 on success, or 0 on failure.
 */
UInt8 srmBufferWrite2Update(SRMBuffer *buffer,
                            UInt32 stride,
                            UInt32 dstX,
                            UInt32 dstY,
                            UInt32 dstWidth,
                            UInt32 dstHeight,
                            const void *pixels);

/**
 * @brief Finalizes the write operation to the buffer.
 *
 * This function should be called after completing all write operations initiated by srmBufferWrite2Begin().
 * It ensures that any pending writes are completed and the buffer is in a consistent state.
 *
 * @param buffer Pointer to the @ref SRMBuffer that was used for writing.
 * @return 1 on success, or 0 if srmBufferWrite2Begin() wasn't called or failed.
 */
UInt8 srmBufferWrite2End(SRMBuffer *buffer);

/**
 * @brief Reads pixel data from an @ref SRMBuffer.
 *
 * This function reads pixel data from the specified region of an @ref SRMBuffer and copies it to the destination buffer.
 *
 * @note On failure, the texture should still be readable by attaching the texture ID returned by @ref srmBufferGetTextureID()
 *       to a GL framebuffer and using `glReadPixels()`.
 *
 * @param buffer Pointer to the @ref SRMBuffer to read data from.
 * @param srcX X-coordinate of the top-left corner of the source region.
 * @param srcY Y-coordinate of the top-left corner of the source region.
 * @param srcW Width of the source region.
 * @param srcH Height of the source region.
 * @param dstX X-coordinate of the top-left corner of the destination region in the destination buffer.
 * @param dstY Y-coordinate of the top-left corner of the destination region in the destination buffer.
 * @param dstStride Stride (row pitch) of the destination buffer.
 * @param dstBuffer Pointer to the destination buffer where the pixel data will be copied.
 * @return 1 on success, or 0 on failure.
 */
UInt8 srmBufferRead(SRMBuffer *buffer,
                    Int32 srcX, Int32 srcY, Int32 srcW, Int32 srcH,
                    Int32 dstX, Int32 dstY, Int32 dstStride, UInt8 *dstBuffer);

/**
 * @brief Retrieves the width of an @ref SRMBuffer.
 *
 * This function retrieves the width of the specified @ref SRMBuffer.
 * 
 * @param buffer Pointer to the @ref SRMBuffer.
 * @return The width of the @ref SRMBuffer.
 */
UInt32 srmBufferGetWidth(SRMBuffer *buffer);

/**
 * @brief Retrieves the height of an @ref SRMBuffer.
 *
 * This function retrieves the height of the specified @ref SRMBuffer.
 * 
 * @param buffer Pointer to the @ref SRMBuffer.
 * @return The height of the @ref SRMBuffer.
 */
UInt32 srmBufferGetHeight(SRMBuffer *buffer);

/**
 * @brief Retrieves the format of an @ref SRMBuffer.
 *
 * This function retrieves the pixel format of the specified @ref SRMBuffer.
 * 
 * @param buffer Pointer to the @ref SRMBuffer.
 * @return The @ref SRM_BUFFER_FORMAT representing the pixel format of the @ref SRMBuffer.
 */
SRM_BUFFER_FORMAT srmBufferGetFormat(SRMBuffer *buffer);

/**
 * @brief Retrieves the allocator device associated with an @ref SRMBuffer.
 *
 * This function retrieves the @ref SRMDevice responsible for memory allocation associated
 * with the specified @ref SRMBuffer.
 * 
 * @param buffer Pointer to the @ref SRMBuffer.
 * @return Pointer to the @ref SRMDevice responsible for memory allocation, or `NULL` on failure.
 */
SRMDevice *srmBufferGetAllocatorDevice(SRMBuffer *buffer);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif // SRMBUFFER_H
