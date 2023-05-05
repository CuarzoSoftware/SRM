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
    UInt32 allocatorBOStride;
    void *allocatorBOMapData;
    UInt8 *allocatorBOMap;
    SRMList *textures;
};

#endif // SRMBUFFERPRIVATE_H
