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
    SRMCore *core;
    struct gbm_bo *allocatorBO;
    Int32 dmaFD;
    UInt8 *dmaMap;
    void *dmaMapData; // Only used by gbm_bo_map
    SRMList *textures;

    enum gbm_bo_flags flags;

};

Int32 srmBufferGetDMAFDFromBO(SRMDevice *device, struct gbm_bo *bo);

#endif // SRMBUFFERPRIVATE_H
