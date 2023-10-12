#include <SRMFormat.h>
#include <SRMList.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <stdlib.h>

static const SRMGLFormat glFormats[] =
{
    {
        .drmFormat = DRM_FORMAT_ARGB8888,
        .glInternalFormat = GL_BGRA_EXT,
        .glFormat = GL_BGRA_EXT,
        .glType = GL_UNSIGNED_BYTE,
        .hasAlpha = 1,
    },
    {
        .drmFormat = DRM_FORMAT_XRGB8888,
        .glInternalFormat = GL_BGRA_EXT,
        .glFormat = GL_BGRA_EXT,
        .glType = GL_UNSIGNED_BYTE,
        .hasAlpha = 0,
    },
    {
        .drmFormat = DRM_FORMAT_XBGR8888,
        .glInternalFormat = GL_RGBA,
        .glFormat = GL_RGBA,
        .glType = GL_UNSIGNED_BYTE,
        .hasAlpha = 0,
    },
    {
        .drmFormat = DRM_FORMAT_ABGR8888,
        .glInternalFormat = GL_RGBA,
        .glFormat = GL_RGBA,
        .glType = GL_UNSIGNED_BYTE,
        .hasAlpha = 1,
    },
    {
        .drmFormat = DRM_FORMAT_BGR888,
        .glInternalFormat = GL_RGB,
        .glFormat = GL_RGB,
        .glType = GL_UNSIGNED_BYTE,
        .hasAlpha = 0,
    },
#if SRM_LITTLE_ENDIAN
    {
        .drmFormat = DRM_FORMAT_RGBX4444,
        .glInternalFormat = GL_RGBA,
        .glFormat = GL_RGBA,
        .glType = GL_UNSIGNED_SHORT_4_4_4_4,
        .hasAlpha = 0,
    },
    {
        .drmFormat = DRM_FORMAT_RGBA4444,
        .glInternalFormat = GL_RGBA,
        .glFormat = GL_RGBA,
        .glType = GL_UNSIGNED_SHORT_4_4_4_4,
        .hasAlpha = 1,
    },
    {
        .drmFormat = DRM_FORMAT_RGBX5551,
        .glInternalFormat = GL_RGBA,
        .glFormat = GL_RGBA,
        .glType = GL_UNSIGNED_SHORT_5_5_5_1,
        .hasAlpha = 0,
    },
    {
        .drmFormat = DRM_FORMAT_RGBA5551,
        .glInternalFormat = GL_RGBA,
        .glFormat = GL_RGBA,
        .glType = GL_UNSIGNED_SHORT_5_5_5_1,
        .hasAlpha = 1,
    },
    {
        .drmFormat = DRM_FORMAT_RGB565,
        .glInternalFormat = GL_RGB,
        .glFormat = GL_RGB,
        .glType = GL_UNSIGNED_SHORT_5_6_5,
        .hasAlpha = 0,
    },
    {
        .drmFormat = DRM_FORMAT_XBGR2101010,
        .glInternalFormat = GL_RGBA,
        .glFormat = GL_RGBA,
        .glType = GL_UNSIGNED_INT_2_10_10_10_REV_EXT,
        .hasAlpha = 0,
    },
    {
        .drmFormat = DRM_FORMAT_ABGR2101010,
        .glInternalFormat = GL_RGBA,
        .glFormat = GL_RGBA,
        .glType = GL_UNSIGNED_INT_2_10_10_10_REV_EXT,
        .hasAlpha = 1,
    },
    {
        .drmFormat = DRM_FORMAT_XBGR16161616F,
        .glInternalFormat = GL_RGBA,
        .glFormat = GL_RGBA,
        .glType = GL_HALF_FLOAT_OES,
        .hasAlpha = 0,
    },
    {
        .drmFormat = DRM_FORMAT_ABGR16161616F,
        .glInternalFormat = GL_RGBA,
        .glFormat = GL_RGBA,
        .glType = GL_HALF_FLOAT_OES,
        .hasAlpha = 1,
    },
    {
        .drmFormat = DRM_FORMAT_XBGR16161616,
        .glInternalFormat = GL_RGBA16_EXT,
        .glFormat = GL_RGBA,
        .glType = GL_UNSIGNED_SHORT,
        .hasAlpha = 0,
    },
    {
        .drmFormat = DRM_FORMAT_ABGR16161616,
        .glInternalFormat = GL_RGBA16_EXT,
        .glFormat = GL_RGBA,
        .glType = GL_UNSIGNED_SHORT,
        .hasAlpha = 1,
    },
#endif
};

SRMListItem *srmFormatsListAddFormat(SRMList *formatsList, UInt32 format, UInt64 modifier)
{
    SRMFormat *fmt = malloc(sizeof(SRMFormat));
    fmt->format = format;
    fmt->modifier = modifier;
    return srmListAppendData(formatsList, fmt);
}

UInt8 srmFormatIsInList(SRMList *formatsList, UInt32 format, UInt64 modifier)
{
    SRMFormat *fmt;
    SRMListForeach(fmtIt, formatsList)
    {
        fmt = srmListItemGetData(fmtIt);

        if (fmt->format == format && fmt->modifier == modifier)
            return 1;
    }

    return 0;
}

void srmFormatsListDestroy(SRMList **formatsList)
{
    if (*formatsList)
    {
        while (!srmListIsEmpty(*formatsList))
            free(srmListPopBack(*formatsList));

        srmListDestroy(*formatsList);
        *formatsList = NULL;
    }
}

SRMList *srmFormatsListCopy(SRMList *formatsList)
{
    SRMList *newList = srmListCreate();

    SRMListForeach(fmtIt, formatsList)
    {
        SRMFormat *fmt = srmListItemGetData(fmtIt);
        srmFormatsListAddFormat(newList, fmt->format, fmt->modifier);
    }

    return newList;
}

const SRMGLFormat *srmFormatDRMToGL(SRM_BUFFER_FORMAT format)
{
    UInt32 n = sizeof(glFormats)/sizeof(glFormats[0]);

    for (UInt32 i = 0; i<n; i++)
        if (format == glFormats[i].drmFormat)
            return &glFormats[i];

    return NULL;
}

SRMFormat *srmFormatListFirstMatchFormat(SRMList *formatsList, UInt32 format)
{
    SRMFormat *fmt;
    SRMListForeach(fmtIt, formatsList)
    {
        fmt = srmListItemGetData(fmtIt);

        if (fmt->format == format)
            return fmt;
    }

    return NULL;
}

UInt8 srmFormatGetDepthBpp(SRM_BUFFER_FORMAT format, UInt32 *depth, UInt32 *bpp)
{
    switch (format) {
    case DRM_FORMAT_C8:
    case DRM_FORMAT_RGB332:
    case DRM_FORMAT_BGR233:
        *depth = 8;
        *bpp = 8;
        break;
    case DRM_FORMAT_XRGB1555:
    case DRM_FORMAT_XBGR1555:
    case DRM_FORMAT_RGBX5551:
    case DRM_FORMAT_BGRX5551:
    case DRM_FORMAT_ARGB1555:
    case DRM_FORMAT_ABGR1555:
    case DRM_FORMAT_RGBA5551:
    case DRM_FORMAT_BGRA5551:
        *depth = 15;
        *bpp = 16;
        break;
    case DRM_FORMAT_RGB565:
    case DRM_FORMAT_BGR565:
        *depth = 16;
        *bpp = 16;
        break;
    case DRM_FORMAT_RGB888:
    case DRM_FORMAT_BGR888:
        *depth = 24;
        *bpp = 24;
        break;
    case DRM_FORMAT_XRGB8888:
    case DRM_FORMAT_XBGR8888:
    case DRM_FORMAT_RGBX8888:
    case DRM_FORMAT_BGRX8888:
        *depth = 24;
        *bpp = 32;
        break;
    case DRM_FORMAT_XRGB2101010:
    case DRM_FORMAT_XBGR2101010:
    case DRM_FORMAT_RGBX1010102:
    case DRM_FORMAT_BGRX1010102:
    case DRM_FORMAT_ARGB2101010:
    case DRM_FORMAT_ABGR2101010:
    case DRM_FORMAT_RGBA1010102:
    case DRM_FORMAT_BGRA1010102:
        *depth = 30;
        *bpp = 32;
        break;
    case DRM_FORMAT_ARGB8888:
    case DRM_FORMAT_ABGR8888:
    case DRM_FORMAT_RGBA8888:
    case DRM_FORMAT_BGRA8888:
        *depth = 32;
        *bpp = 32;
        break;
    default:
        *depth = 0;
        *bpp = 0;
        return 0;
    }

    return 1;
}
