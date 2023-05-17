#ifndef SRMBUFFER_H
#define SRMBUFFER_H

#include "SRMTypes.h"
#include <GLES2/gl2.h>

#ifdef __cplusplus
extern "C" {
#endif

SRMBuffer *srmBufferCreateFromCPU(SRMCore *core,
                                  UInt32 width,
                                  UInt32 height,
                                  UInt8 *pixels,
                                  SRM_BUFFER_FORMAT format);

GLuint srmBufferGetTextureID(SRMDevice *device, SRMBuffer *buffer);
void srmBufferDestroy(SRMBuffer *buffer);


#ifdef __cplusplus
}
#endif

#endif // SRMBUFFER_H
