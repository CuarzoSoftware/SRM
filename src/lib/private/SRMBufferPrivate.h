#ifndef SRMBUFFERPRIVATE_H
#define SRMBUFFERPRIVATE_H

#include <SRMBuffer.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <gbm.h>

struct SRMBufferTexture
{
    SRMDevice *device;
    EGLImage image;
    GLuint texture;
};

struct SRMBufferStruct
{
    // Props
    UInt32 format, width, height, bpp, stride;
    UInt64 modifier;

    SRMCore *core;
    struct gbm_bo *allocatorBO;
    GLuint textureID; // If allocation with GBM fails
    Int32 dmaFD;
    UInt8 *dmaMap;
    void *dmaMapData; // Only used by gbm_bo_map
    SRMList *textures;

    enum gbm_bo_flags flags;

};

Int32 srmBufferGetDMAFDFromBO(SRMDevice *device, struct gbm_bo *bo);

#endif // SRMBUFFERPRIVATE_H
