#ifndef SRMBUFFER_H
#define SRMBUFFER_H

#include <SRMTypes.h>
#include <GLES2/gl2.h>

SRMBuffer *srmBufferCreateFromCPU(SRMCore *core,
                                  UInt32 width,
                                  UInt32 height,
                                  UInt8 *pixels,
                                  SRM_BUFFER_FORMAT format);

GLuint srmBufferGetTextureID(SRMDevice *device, SRMBuffer *buffer);

GLint srmBufferFormatToGles(SRM_BUFFER_FORMAT format);

void srmBufferDestroy(SRMBuffer *buffer);


#endif // SRMBUFFER_H
