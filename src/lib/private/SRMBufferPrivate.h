#ifndef SRMBUFFERPRIVATE_H
#define SRMBUFFERPRIVATE_H

#include <SRMFormat.h>
#include <SRMBuffer.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <pthread.h>
#include <gbm.h>
#include <linux/dma-buf.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EGL_DMA_PLANE_DEF(planes_n) \
if (buffer->planesCount > planes_n) \
{ \
    imageAttribs[index++] = EGL_DMA_BUF_PLANE ## planes_n ## _FD_EXT; \
    imageAttribs[index++] = buffer->fds[planes_n]; \
    imageAttribs[index++] = EGL_DMA_BUF_PLANE ## planes_n ## _OFFSET_EXT; \
    imageAttribs[index++] = buffer->offsets[planes_n]; \
    imageAttribs[index++] = EGL_DMA_BUF_PLANE ## planes_n ## _PITCH_EXT; \
    imageAttribs[index++] = buffer->strides[planes_n]; \
    imageAttribs[index++] = EGL_DMA_BUF_PLANE ## planes_n ## _MODIFIER_LO_EXT; \
    imageAttribs[index++] = buffer->modifiers[planes_n] & 0xffffffff; \
    imageAttribs[index++] = EGL_DMA_BUF_PLANE ## planes_n ## _MODIFIER_HI_EXT; \
    imageAttribs[index++] = buffer->modifiers[planes_n] >> 32; \
} \

struct SRMBufferTexture
{
    SRMDevice *device;
    EGLImage image;
    GLuint texture;
    UInt8 updated;
};

typedef enum SRM_BUFFER_WRITE_MODE_ENUM
{
    SRM_BUFFER_WRITE_MODE_NONE,
    SRM_BUFFER_WRITE_MODE_PRIME,
    SRM_BUFFER_WRITE_MODE_GBM,
    SRM_BUFFER_WRITE_MODE_GLES
} SRM_BUFFER_WRITE_MODE;

struct SRMBufferStruct
{
    // Common
    SRMDevice *allocator;
    pthread_mutex_t mutex;
    enum SRM_BUFFER_SRC src;
    SRM_BUFFER_WRITE_MODE writeMode;
    UInt32 refCount;
    EGLSyncKHR eglSync;

    UInt32 width;
    UInt32 height;
    UInt32 format;
    UInt32 caps;

    UInt32 bpp;
    UInt32 pixelSize;
    SRMCore *core;
    SRMList *textures;

    // GBM
    struct gbm_bo *bo;
    void *mapData;

    // Scanout
    struct {
        UInt32 fb;
        struct gbm_bo *bo; // Can be NULL
        SRMFormat fmt;
    } scanout;

    // DMA
    UInt32 planesCount;
    UInt64 modifiers[SRM_MAX_PLANES];
    Int32 fds[SRM_MAX_PLANES];
    UInt32 strides[SRM_MAX_PLANES];
    UInt32 offsets[SRM_MAX_PLANES];
    void *map;
    struct dma_buf_sync sync;

    // GL
    GLenum target;
    GLint glInternalFormat;
    GLint glFormat;
    GLint glType;
};

SRMBuffer *srmBufferCreate(SRMCore *core, SRMDevice *allocator);

/* Increases the ref count by 1 */
SRMBuffer *srmBufferGetRef(SRMBuffer *buffer);
Int32 srmBufferGetDMAFDFromBO(SRMDevice *device, struct gbm_bo *bo);
void *srmBufferMapFD(Int32 fd, size_t len, UInt32 *caps);
struct gbm_bo *srmBufferCreateLinearBO(struct gbm_device *dev, UInt32 width, UInt32 height, UInt32 format);
struct gbm_surface *srmBufferCreateGBMSurface(struct gbm_device *dev, UInt32 width, UInt32 height, UInt32 format, UInt64 modifier, UInt32 flags);
struct gbm_bo *srmBufferCreateGBMBo(struct gbm_device *dev, UInt32 width, UInt32 height, UInt32 format, UInt64 modifier, UInt32 flags);
UInt8 srmBufferUpdateSync(SRMBuffer *buffer);
void srmBufferWaitSync(SRMBuffer *buffer);

#ifdef __cplusplus
}
#endif

#endif // SRMBUFFERPRIVATE_H
