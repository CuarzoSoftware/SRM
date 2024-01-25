#ifndef SRMBUFFERPRIVATE_H
#define SRMBUFFERPRIVATE_H

#include <SRMBuffer.h>
#include <EGL/egl.h>
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

struct SRMBufferStruct
{
    // Common
    SRMDevice *allocator;
    pthread_mutex_t mutex;
    enum SRM_BUFFER_SRC src;

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
    enum gbm_bo_flags flags;
    void *mapData;

    // DMA
    UInt32 planesCount;
    UInt64 modifiers[SRM_MAX_PLANES];
    Int32 fds[SRM_MAX_PLANES];
    UInt32 strides[SRM_MAX_PLANES];
    UInt32 offsets[SRM_MAX_PLANES];
    void *map;
    struct dma_buf_sync sync;

    // Gles
    GLenum target; // GL_TEXTURE_2D is mutable and GL_TEXTURE_EXTERNAL_OES immutable
    GLint glInternalFormat;
    GLint glFormat;
    GLint glType;
};

SRMBuffer *srmBufferCreate(SRMCore *core, SRMDevice *allocator);
Int32 srmBufferGetDMAFDFromBO(SRMDevice *device, struct gbm_bo *bo);

#ifdef __cplusplus
}
#endif

#endif // SRMBUFFERPRIVATE_H
