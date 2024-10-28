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

struct SRMBufferTexture
{
    SRMDevice *device;
    EGLImage image;
    GLuint texture;
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
    // DMA
    SRMBufferDMAData dma;
    void *map;
    struct dma_buf_sync sync;

    // Common
    SRMDevice *allocator;
    pthread_mutex_t mutex;
    enum SRM_BUFFER_SRC src;
    SRM_BUFFER_WRITE_MODE writeMode;
    UInt32 refCount;
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

    // GL
    EGLSyncKHR fence;
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
void srmBufferFillParamsFromBO(SRMBuffer *buffer, struct gbm_bo *bo);
void srmBufferSetTargetFromFormat(SRMBuffer *buffer);
void srmBufferCreateSync(SRMBuffer *buffer);
void srmBufferWaitSync(SRMBuffer *buffer);
UInt8 srmBufferCreateRBFromBO(SRMCore *core, struct gbm_bo *bo, GLuint *outFB, GLuint *outRB, SRMBuffer **outWrapper);

#ifdef __cplusplus
}
#endif

#endif // SRMBUFFERPRIVATE_H
