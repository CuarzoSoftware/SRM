#ifndef SRMFORMAT_H
#define SRMFORMAT_H

#include <SRMTypes.h>
#include <GLES2/gl2.h>

struct SRMFormatStruct
{
    UInt32 format;
    UInt64 modifier;
};

struct SRMGLFormatStruct
{
    UInt32 drmFormat;
    GLint glInternalFormat;
    GLint glFormat, glType;
    UInt8 hasAlpha;
};

const SRMGLFormat *srmFormatDRMToGL(SRM_BUFFER_FORMAT format);
SRMListItem *srmFormatsListAddFormat(SRMList *formatsList, UInt32 format, UInt64 modifier);
SRMFormat *srmFormatListFirstMatchFormat(SRMList *formatsList, UInt32 format);
UInt8 srmFormatGetDepthBpp(SRM_BUFFER_FORMAT format, UInt32 *depth, UInt32 *bpp);
SRMList *srmFormatsListCopy(SRMList *formatsList);
void srmFormatsListDestroy(SRMList **formatsList);

#endif // SRMFORMAT_H
