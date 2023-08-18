#ifndef SRMBUFFER_H
#define SRMBUFFER_H

#include "SRMTypes.h"
#include <GLES2/gl2.h>
#include <gbm.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup SRMBuffer SRMBuffer
 * @brief Represents an OpenGL ES 2.0 texture shared across GPUs and created from various sources.
 *
 * The SRMBuffer class encapsulates an OpenGL ES 2.0 texture that is shared across multiple GPUs.
 * It can be created from a variety of sources including DMA planes, main memory, GBM bo buffers,
 * and more. This class provides a unified interface to manage and utilize such shared textures.
 *
 * @addtogroup SRMBuffer
 * @{
 */

#define SRM_MAX_PLANES 4

/**
 * @enum SRM_BUFFER_CAP
 * Enumerates the capabilities of an SRMBuffer.
 */
enum SRM_BUFFER_CAP
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
};

/**
 * @enum SRM_BUFFER_SRC
 * Enumerates the possible sources of an SRM buffer.
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
     * The buffer source is a Wayland wl_drm buffer.
     */
    SRM_BUFFER_SRC_WL_DRM,

    /**
     * The buffer source is a bo from the Graphics Buffer Manager (GBM).
     */
    SRM_BUFFER_SRC_GBM
};

/**
 * @struct SRMBufferDMADataStruct
 * Structure containing DMA-related data for an SRM buffer.
 */
struct SRMBufferDMADataStruct
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
};

/**
 * @brief Creates an SRMBuffer from a Graphics Buffer Manager (GBM) gbm_bo.
 *
 * This function creates an SRMBuffer object using the provided GBM buffer object.
 * 
 * @warning The gbm_bo must remain valid during the buffer lifetime. It is not destroyed when the buffer is destroyed and must be destroyed manually.
 *
 * @param core Pointer to the SRMCore instance.
 * @param bo Pointer to the GBM buffer object.
 * @return A pointer to the created SRMBuffer, or NULL on failure.
 */
SRMBuffer *srmBufferCreateFromGBM(SRMCore *core, struct gbm_bo *bo);

/**
 * @brief Creates an SRMBuffer from Direct Memory Access (DMA) planes.
 *
 * This function creates an SRMBuffer object using the provided DMA planes data, which includes information
 * about width, height, format, memory planes, and more.
 * 
 * @param core Pointer to the SRMCore instance.
 * @param allocator Pointer to the SRMDevice responsible for memory allocation. Use NULL to make the texture shareable between all devices (GPUs).
 * @param dmaPlanes Pointer to the SRMBufferDMAData containing DMA planes data.
 * @return A pointer to the created SRMBuffer, or NULL on failure.
 */
SRMBuffer *srmBufferCreateFromDMA(SRMCore *core, SRMDevice *allocator, SRMBufferDMAData *dmaPlanes);

/**
 * @brief Creates an SRMBuffer from main memory buffer.
 *
 * This function creates an SRMBuffer object using a main memory buffer, allowing you to define
 * the buffer's width, height, stride, pixel data, and format.
 * 
 * @param core Pointer to the SRMCore instance.
 * @param allocator Pointer to the SRMDevice responsible for memory allocation. Use NULL to make the texture shareable between all devices (GPUs).
 * @param width Width of the src buffer.
 * @param height Height of the src buffer.
 * @param stride Stride (row pitch) of the src buffer in bytes.
 * @param pixels Pointer to the pixel data.
 * @param format Format of the pixel data.
 * @return A pointer to the created SRMBuffer, or NULL on failure.
 */
SRMBuffer *srmBufferCreateFromCPU(SRMCore *core, SRMDevice *allocator,
                                  UInt32 width, UInt32 height, UInt32 stride,
                                  const void *pixels, SRM_BUFFER_FORMAT format);


/**
 * @brief Creates an SRMBuffer from a Wayland wl_drm buffer.
 *
 * This function creates an SRMBuffer object using the provided Wayland wl_drm buffer.
 * 
 * @param core Pointer to the SRMCore instance.
 * @param wlBuffer Pointer to the Wayland DRM buffer.
 * @return A pointer to the created SRMBuffer, or NULL on failure.
 */
SRMBuffer *srmBufferCreateFromWaylandDRM(SRMCore *core, void *wlBuffer);

/**
 * @brief Destroys an SRMBuffer.
 *
 * This function destroys an SRMBuffer object, freeing associated resources.
 * 
 * @param buffer Pointer to the SRMBuffer to be destroyed.
 */
void srmBufferDestroy(SRMBuffer *buffer);

/**
 * @brief Retrieves an OpenGL texture ID associated with an SRMBuffer for a specific device (GPU).
 *
 * This function retrieves the OpenGL texture ID that corresponds to the given SRMBuffer
 * for the specified device (GPU).
 * 
 * @note You probably will call this function when doing rendering on a connector.
 *       To get the connector's renderer device, use `srmDeviceGetRendererDevice(srmConnectorGetDevice(connector))`.
 *
 * @param device Pointer to the SRMDevice representing the specific device (GPU).
 * @param buffer Pointer to the SRMBuffer for which the texture ID is requested.
 * @return The OpenGL texture ID associated with the SRMBuffer and device, or 0 on failure.
 */
GLuint srmBufferGetTextureID(SRMDevice *device, SRMBuffer *buffer);

/**
 * @brief Writes pixel data to an SRMBuffer.
 *
 * This function writes pixel data to the specified region of an SRMBuffer.
 * 
 * @param buffer Pointer to the SRMBuffer to write data to.
 * @param stride Stride (row pitch) of the source pixel data.
 * @param dstX X-coordinate of the top-left corner of the destination region.
 * @param dstY Y-coordinate of the top-left corner of the destination region.
 * @param dstWidth Width of the destination region.
 * @param dstHeight Height of the destination region.
 * @param pixels Pointer to the source pixel data. Must point to the top-left of the source buffer.
 * @return 1 on success, or 0 on failure.
 */
UInt8 srmBufferWrite(SRMBuffer *buffer,
                     UInt32 stride, // src stride
                     UInt32 dstX,
                     UInt32 dstY,
                     UInt32 dstWidth,
                     UInt32 dstHeight,
                     const void *pixels);

/**
 * @brief Reads pixel data from an SRMBuffer.
 *
 * This function reads pixel data from the specified region of an SRMBuffer and copies it to the destination buffer.
 * 
 * @param buffer Pointer to the SRMBuffer to read data from.
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
 * @brief Retrieves the width of an SRMBuffer.
 *
 * This function retrieves the width of the specified SRMBuffer.
 * 
 * @param buffer Pointer to the SRMBuffer.
 * @return The width of the SRMBuffer.
 */
UInt32 srmBufferGetWidth(SRMBuffer *buffer);

/**
 * @brief Retrieves the height of an SRMBuffer.
 *
 * This function retrieves the height of the specified SRMBuffer.
 * 
 * @param buffer Pointer to the SRMBuffer.
 * @return The height of the SRMBuffer.
 */
UInt32 srmBufferGetHeight(SRMBuffer *buffer);

/**
 * @brief Retrieves the format of an SRMBuffer.
 *
 * This function retrieves the pixel format of the specified SRMBuffer.
 * 
 * @param buffer Pointer to the SRMBuffer.
 * @return The SRM_BUFFER_FORMAT representing the pixel format of the SRMBuffer.
 */
SRM_BUFFER_FORMAT srmBufferGetFormat(SRMBuffer *buffer);

/**
 * @brief Retrieves the allocator device associated with an SRMBuffer.
 *
 * This function retrieves the SRMDevice responsible for memory allocation associated
 * with the specified SRMBuffer.
 * 
 * @param buffer Pointer to the SRMBuffer.
 * @return Pointer to the SRMDevice responsible for memory allocation, or NULL on failure.
 */
SRMDevice *srmBufferGetAllocatorDevice(SRMBuffer *buffer);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif // SRMBUFFER_H