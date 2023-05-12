#include <private/SRMBufferPrivate.h>
#include <private/SRMCorePrivate.h>
#include <private/SRMDevicePrivate.h>

#include <SRMList.h>
#include <SRMLog.h>

#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>
#include <gbm.h>
#include <EGL/eglext.h>
#include <GLES2/gl2ext.h>
#include <unistd.h>


SRMBuffer *srmBufferCreateFromCPU(SRMCore *core, UInt32 width, UInt32 height, UInt8 *pixels, SRM_BUFFER_FORMAT format)
{
    SRMBuffer *buffer = calloc(1, sizeof(SRMBuffer));
    buffer->core = core;
    buffer->dmaFD = -1;
    buffer->textures = srmListCreate();
    buffer->allocatorBO = gbm_bo_create(core->allocatorDevice->gbm, width, height, format, GBM_BO_USE_LINEAR | GBM_BO_USE_RENDERING);

    if (!buffer->allocatorBO)
    {
        SRMError("Failed to create buffer from CPU with allocator device %s.", core->allocatorDevice->name);
        goto fail;
    }

    buffer->dmaFD = srmBufferGetDMAFDFromBO(core->allocatorDevice, buffer->allocatorBO);

    if (buffer->dmaFD != -1)
    {
        // Map the DMA buffer into user space
        buffer->dmaMap = mmap(NULL, height*gbm_bo_get_stride(buffer->allocatorBO), PROT_READ | PROT_WRITE, MAP_SHARED, buffer->dmaFD, 0);

        if (buffer == MAP_FAILED)
        {
           SRMWarning("Failed to map dma FD from allocator device %s. Using glTexImage2D instead.", core->allocatorDevice->name);
           close(buffer->dmaFD);
           buffer->dmaFD = -1;
        }
        else
        {
            UInt32 stride = gbm_bo_get_stride(buffer->allocatorBO);
            UInt32 height = gbm_bo_get_height(buffer->allocatorBO);
            UInt32 width = gbm_bo_get_width(buffer->allocatorBO);
            UInt32 bytesPP = gbm_bo_get_bpp(buffer->allocatorBO)/8;

            for (UInt32 i = 0; i < height; i++)
            {
                memcpy(&buffer->dmaMap[i*stride],
                       &pixels[i*width*bytesPP],
                       width*bytesPP);
            }

            return buffer;
        }
    }

    srmBufferGetTextureID(core->allocatorDevice, buffer);
    GLint fmt = srmBufferFormatToGles(format);
    glTexImage2D(GL_TEXTURE_2D, 0, fmt, width, height, 0, fmt, GL_UNSIGNED_BYTE, pixels);
    return buffer;

    fail:
    srmBufferDestroy(buffer);
    return NULL;
}

GLuint srmBufferGetTextureID(SRMDevice *device, SRMBuffer *buffer)
{
    // Check if already created
    struct SRMBufferTexture *texture;
    SRMListForeach(item, buffer->textures)
    {
        texture = srmListItemGetData(item);

        if (texture->device == device)
            return texture->texture;
    }

    // Creates the texture
    texture = calloc(1, sizeof(struct SRMBufferTexture));
    texture->device = device;

    if (device == buffer->core->allocatorDevice)
    {
        texture->image = eglCreateImage(device->eglDisplay, device->eglSharedContext, EGL_NATIVE_PIXMAP_KHR, buffer->allocatorBO, NULL);
    }
    else
    {
        if (!buffer->core->allocatorDevice->capPrimeExport)
        {
            SRMError("srmBufferGetTextureID failed. Allocator device (%s) has not the PRIME export cap.", buffer->core->allocatorDevice->name);
            free(texture);
            return 0;
        }

        if (!device->capPrimeImport)
        {
            SRMError("srmBufferGetTextureID failed. Target device (%s) has not the PRIME import cap.", buffer->core->allocatorDevice->name);
            free(texture);
            return 0;
        }

        if (buffer->dmaFD == -1)
            buffer->dmaFD = srmBufferGetDMAFDFromBO(buffer->core->allocatorDevice, buffer->allocatorBO);

        EGLAttrib image_attribs[] = {
                                   EGL_WIDTH, gbm_bo_get_width(buffer->allocatorBO),
                                   EGL_HEIGHT, gbm_bo_get_height(buffer->allocatorBO),
                                   EGL_LINUX_DRM_FOURCC_EXT, gbm_bo_get_format(buffer->allocatorBO),
                                   EGL_DMA_BUF_PLANE0_FD_EXT, buffer->dmaFD,
                                   EGL_DMA_BUF_PLANE0_OFFSET_EXT, gbm_bo_get_offset(buffer->allocatorBO,0),
                                   EGL_DMA_BUF_PLANE0_PITCH_EXT, gbm_bo_get_stride_for_plane(buffer->allocatorBO, 0),
                                   EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, (EGLAttrib)(gbm_bo_get_modifier(buffer->allocatorBO) & 0xffffffff),
                                   EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, (EGLAttrib)(gbm_bo_get_modifier(buffer->allocatorBO) >> 32),
                                   EGL_NONE };

        texture->image = eglCreateImage(device->eglDisplay, NULL, EGL_LINUX_DMA_BUF_EXT, NULL, image_attribs);

    }

    if (texture->image == EGL_NO_IMAGE)
    {
        SRMError("srmBufferGetTextureID error. Failed to create EGL image.");
        free(texture);
        return 0;
    }

    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &texture->texture);
    glBindTexture(GL_TEXTURE_2D, texture->texture);
    device->eglFunctions.glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, texture->image);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    srmListAppendData(buffer->textures, texture);

    return texture->texture;
}

void srmBufferDestroy(SRMBuffer *buffer)
{
    if (buffer->textures)
    {
        srmListDestoy(buffer->textures);
    }

    if (buffer->allocatorBO)
    {
        if (buffer->dmaMap)
            munmap(buffer->dmaMap, gbm_bo_get_height(buffer->allocatorBO)*gbm_bo_get_stride(buffer->allocatorBO));

        if (buffer->dmaFD)
            close(buffer->dmaFD);

        gbm_bo_destroy(buffer->allocatorBO);
    }

    free(buffer);
}

GLint srmBufferFormatToGles(SRM_BUFFER_FORMAT format)
{
    switch(format)
    {
        case SRM_BUFFER_FORMAT_ARGB8888: return GL_BGRA_EXT;
        case SRM_BUFFER_FORMAT_XRGB8888: return GL_BGRA_EXT;
        case SRM_BUFFER_FORMAT_ABGR8888: return GL_RGBA;
        case SRM_BUFFER_FORMAT_XBGR8888: return GL_RGBA;
    }
}
