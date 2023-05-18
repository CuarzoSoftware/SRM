#ifndef SRMBUFFERPRIVATE_H
#define SRMBUFFERPRIVATE_H

#include "../SRMBuffer.h"
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <gbm.h>

#ifdef __cplusplus
extern "C" {
#endif

struct SRMBufferTexture
{
    SRMDevice *device;
    EGLImage image;
    GLuint texture;
};

struct SRMBufferStruct
{
    // Common
    enum SRM_BUFFER_SRC src;
    UInt32 format, width, height, bpp, stride;
    UInt64 modifier;
    SRMCore *core;
    SRMList *textures;

    // GBM
    struct gbm_bo *bo;
    enum gbm_bo_flags flags;
    void *mapData;

    // EGL
    EGLImage image;

    // Gles
    GLuint textureID;

    // DMA
    Int32 fd;
    void *map;
};

SRMBuffer *srmBufferCreate(SRMCore *core);
Int32 srmBufferGetDMAFDFromBO(SRMDevice *device, struct gbm_bo *bo);

#ifdef __cplusplus
}
#endif

#endif // SRMBUFFERPRIVATE_H
