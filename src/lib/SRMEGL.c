#include <private/SRMDevicePrivate.h>
#include <SRMLog.h>
#include <SRMEGL.h>
#include <string.h>

const char *srmEGLGetErrorString(EGLint error)
{
    switch (error)
    {
        case EGL_SUCCESS:
            return "EGL_SUCCESS";
        case EGL_NOT_INITIALIZED:
            return "EGL_NOT_INITIALIZED";
        case EGL_BAD_ACCESS:
            return "EGL_BAD_ACCESS";
        case EGL_BAD_ALLOC:
            return "EGL_BAD_ALLOC";
        case EGL_BAD_ATTRIBUTE:
            return "EGL_BAD_ATTRIBUTE";
        case EGL_BAD_CONTEXT:
            return "EGL_BAD_CONTEXT";
        case EGL_BAD_CONFIG:
            return "EGL_BAD_CONFIG";
        case EGL_BAD_CURRENT_SURFACE:
            return "EGL_BAD_CURRENT_SURFACE";
        case EGL_BAD_DISPLAY:
            return "EGL_BAD_DISPLAY";
        case EGL_BAD_DEVICE_EXT:
            return "EGL_BAD_DEVICE_EXT";
        case EGL_BAD_SURFACE:
            return "EGL_BAD_SURFACE";
        case EGL_BAD_MATCH:
            return "EGL_BAD_MATCH";
        case EGL_BAD_PARAMETER:
            return "EGL_BAD_PARAMETER";
        case EGL_BAD_NATIVE_PIXMAP:
            return "EGL_BAD_NATIVE_PIXMAP";
        case EGL_BAD_NATIVE_WINDOW:
            return "EGL_BAD_NATIVE_WINDOW";
        case EGL_CONTEXT_LOST:
            return "EGL_CONTEXT_LOST";
    }
    return "unknown error";
}

UInt8 srmEGLHasExtension(const char *extensions, const char *extension)
{
    size_t extlen = strlen(extension);
    const char *end = extensions + strlen(extensions);

    while (extensions < end)
    {
        if (*extensions == ' ')
        {
            extensions++;
            continue;
        }

        size_t n = strcspn(extensions, " ");

        if (n == extlen && strncmp(extension, extensions, n) == 0)
            return 1;

        extensions += n;
    }

    return 0;
}

const char *srmEGLGetContextPriorityString(EGLint priority)
{
    switch (priority)
    {
    case EGL_CONTEXT_PRIORITY_HIGH_IMG:
        return "HIGH";
    case EGL_CONTEXT_PRIORITY_MEDIUM_IMG:
        return "MEDIUM";
    case EGL_CONTEXT_PRIORITY_LOW_IMG:
        return "LOW";
    default:
        return "UNKNOWN";
    }
}

EGLImage srmEGLCreateImageFromDMA(SRMDevice *device, const SRMBufferDMAData *dma)
{
    if (!device->eglExtensions.EXT_image_dma_buf_import)
    {
        SRMError("[%s] srmEGLCreateImageFromDMA: EXT_image_dma_buf_import not supported.", device->shortName);
        return EGL_NO_IMAGE;
    }

    if (!device->eglExtensions.EXT_image_dma_buf_import_modifiers)
    {
        for (UInt32 i = 0; i < dma->num_fds; i++)
        {
            if (dma->modifiers[i] != DRM_FORMAT_MOD_INVALID)
            {
                SRMWarning("[%s] srmEGLCreateImageFromDMA: Explicit modifier passed but EXT_image_dma_buf_import_modifiers is not supported.", device->shortName);
                break;
            }
        }
    }

    static EGLint ATTRIBS_FD[] =
    {
        EGL_DMA_BUF_PLANE0_FD_EXT,
        EGL_DMA_BUF_PLANE1_FD_EXT,
        EGL_DMA_BUF_PLANE2_FD_EXT,
        EGL_DMA_BUF_PLANE3_FD_EXT
    };

    static EGLint ATTRIBS_OFFSET[] =
    {
        EGL_DMA_BUF_PLANE0_OFFSET_EXT,
        EGL_DMA_BUF_PLANE1_OFFSET_EXT,
        EGL_DMA_BUF_PLANE2_OFFSET_EXT,
        EGL_DMA_BUF_PLANE3_OFFSET_EXT
    };

    static EGLint ATTRIBS_PITCH[] =
    {
        EGL_DMA_BUF_PLANE0_PITCH_EXT,
        EGL_DMA_BUF_PLANE1_PITCH_EXT,
        EGL_DMA_BUF_PLANE2_PITCH_EXT,
        EGL_DMA_BUF_PLANE3_PITCH_EXT
    };

    static EGLint ATTRIBS_MOD_LO[] =
    {
        EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT,
        EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT,
        EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT,
        EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT,
    };

    static EGLint ATTRIBS_MOD_HI[] =
    {
        EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT,
        EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT,
        EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT,
        EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT,
    };

    UInt32 index = 0;
    EGLint attribs[9 + 10 * dma->num_fds];
    attribs[index++] = EGL_WIDTH;
    attribs[index++] = dma->width;
    attribs[index++] = EGL_HEIGHT;
    attribs[index++] = dma->height;
    attribs[index++] = EGL_LINUX_DRM_FOURCC_EXT;
    attribs[index++] = dma->format;

    for (UInt32 i = 0; i < dma->num_fds; i++)
    {
        attribs[index++] = ATTRIBS_FD[i];
        attribs[index++] = dma->fds[i];
        attribs[index++] = ATTRIBS_OFFSET[i];
        attribs[index++] = dma->offsets[i];
        attribs[index++] = ATTRIBS_PITCH[i];
        attribs[index++] = dma->strides[i];

        if (device->eglExtensions.EXT_image_dma_buf_import_modifiers && dma->modifiers[i] != DRM_FORMAT_MOD_INVALID)
        {
            attribs[index++] = ATTRIBS_MOD_LO[i];
            attribs[index++] = dma->modifiers[i] & 0xffffffff;
            attribs[index++] = ATTRIBS_MOD_HI[i];
            attribs[index++] = dma->modifiers[i] >> 32;
        }
    }

    attribs[index++] = EGL_IMAGE_PRESERVED_KHR;
    attribs[index++] = EGL_TRUE;
    attribs[index++] = EGL_NONE;

    EGLImage image = device->eglFunctions.eglCreateImageKHR(device->eglDisplay, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attribs);

    if (image == EGL_NO_IMAGE)
    {
        SRMError("[%s] srmEGLCreateImageFromDMA: eglCreateImageKHR failed.", device->shortName);
        return EGL_NO_IMAGE;
    }

    return image;
}
