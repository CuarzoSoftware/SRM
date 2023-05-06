#ifndef SRMBUFFERPRIVATE_H
#define SRMBUFFERPRIVATE_H

#include <SRMBuffer.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

struct SRMBufferTexture
{
    SRMDevice *device;
    EGLImage image;
    GLuint texture;
};

struct SRMBufferStruct
{
    SRMCore *core;
    struct gbm_bo *allocatorBO;
    Int32 dmaFD;
    UInt8 *dmaMap;
    SRMList *textures;
};

Int32 srmBufferGetDMAFDFromBO(SRMDevice *device, struct gbm_bo *bo);

#endif // SRMBUFFERPRIVATE_H
