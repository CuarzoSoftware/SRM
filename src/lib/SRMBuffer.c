#include <private/SRMBufferPrivate.h>
#include <private/SRMCorePrivate.h>
#include <private/SRMDevicePrivate.h>

#include <SRMFormat.h>
#include <SRMList.h>
#include <SRMLog.h>

#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>
#include <gbm.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

#include <linux/dma-heap.h>
#include <linux/dma-buf.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>


SRMBuffer *srmBufferCreateFromCPU(SRMCore *core, UInt32 width, UInt32 height, UInt32 stride, void *pixels, SRM_BUFFER_FORMAT format)
{
    SRMBuffer *buffer = srmBufferCreate(core);
    buffer->src = SRM_BUFFER_SRC_CPU;
    buffer->planesCount = 1;
    buffer->width = width;
    buffer->height = height;
    buffer->format = format;
    const SRMGLFormat *glFmt;

    // Try to use linear so that can be mapped
    buffer->flags = GBM_BO_USE_RENDERING | GBM_BO_USE_LINEAR | GBM_BO_USE_WRITE;
    buffer->bo = gbm_bo_create(core->allocatorDevice->gbm, width, height, format, buffer->flags);

    if (!buffer->bo)
    {
        buffer->flags = GBM_BO_USE_RENDERING | GBM_BO_USE_LINEAR;
        buffer->bo = gbm_bo_create(core->allocatorDevice->gbm, width, height, format, buffer->flags);
    }

    if (!buffer->bo)
    {
        SRMWarning("[%s] Failed to create buffer from CPU with GBM. Trying glTexImage2D instead.", core->allocatorDevice->name);
        goto glesOnly;
    }

    buffer->bpp = gbm_bo_get_bpp(buffer->bo);

    if (buffer->bpp % 8 != 0)
    {
        SRMWarning("[%s] Buffer bpp must be a multiple of 8.", core->allocatorDevice->name);
        goto glesOnly;
    }

    buffer->modifier = gbm_bo_get_modifier(buffer->bo);
    buffer->stride = gbm_bo_get_stride(buffer->bo);
    buffer->pixelSize = buffer->bpp/8;

    buffer->fd = srmBufferGetDMAFDFromBO(core->allocatorDevice, buffer->bo);

    if (buffer->fd >= 0)
    {
        // Map the DMA buffer into user space
        buffer->map = mmap(NULL, height * buffer->stride, PROT_READ | PROT_WRITE, MAP_SHARED, buffer->fd, 0);

        if (buffer->map == MAP_FAILED)
        {
            buffer->map = mmap(NULL, height * buffer->stride, PROT_WRITE, MAP_SHARED, buffer->fd, 0);

            if (buffer->map == MAP_FAILED)
            {
                SRMWarning("[%s] Directly mapping buffer DMA fd failed. Trying gbm_bo_map.", core->allocatorDevice->name);
                goto gbmMap;
            }
        }
        goto mapWrite;
    }

    gbmWrite:

    if (gbm_bo_write(buffer->bo, pixels, width * stride * buffer->pixelSize) != 0)
    {
        SRMWarning("[%s] gbm_bo_write failed. Trying glTexImage2D instead.", core->allocatorDevice->name);
        goto glesOnly;
    }

    buffer->caps |= SRM_BUFFER_CAP_WRITE;
    SRMDebug("[%s] CPU buffer created using gbm_bo_write.", core->allocatorDevice->name);
    return buffer;

    gbmMap:

    buffer->map = gbm_bo_map(buffer->bo, 0, 0, width, height, GBM_BO_TRANSFER_READ, &buffer->stride, &buffer->mapData);

    if (!buffer->map)
    {
        SRMWarning("[%s] Failed to map DMA FD. Tying gbm_bo_write instead.", core->allocatorDevice->name);
        goto gbmWrite;
    }

    SRMDebug("[%s] Buffer mapped with gbm_bo_map().", core->allocatorDevice->name);

    /* Only if LINEAR */

    mapWrite:

    buffer->offset = gbm_bo_get_offset(buffer->bo, 0);

    for (UInt32 i = 0; i < height; i++)
    {
        memcpy(&buffer->map[buffer->offset + i*buffer->stride],
               &pixels[i*stride],
               stride*buffer->pixelSize);
    }

    buffer->caps |= SRM_BUFFER_CAP_READ | SRM_BUFFER_CAP_WRITE | SRM_BUFFER_CAP_MAP;
    SRMDebug("[%s] CPU buffer created using mapping.", core->allocatorDevice->name);
    return buffer;

    glesOnly:

    /* In this case the texture is only avaliable for 1 GPU */

    glFmt = srmFormatDRMToGL(format);

    if (!glFmt)
    {
        SRMError("[%s] Could not find the equivalent GL format and type from DRM format %s.",
                 core->allocatorDevice->name,
                 drmGetFormatName(format));
        goto fail;
    }

    buffer->glType = glFmt->glType;
    buffer->glFormat = glFmt->glFormat;
    buffer->glInternalFormat = glFmt->glInternalFormat;

    UInt32 depth;

    if (srmFormatGetDepthBpp(format, &depth, &buffer->bpp))
    {
        buffer->pixelSize = buffer->bpp/8;
        buffer->stride = buffer->pixelSize*width;
    }

    // Creates the texture
    struct SRMBufferTexture *texture = calloc(1, sizeof(struct SRMBufferTexture));
    texture->device = core->allocatorDevice;
    texture->image = EGL_NO_IMAGE;

    EGLDisplay prevDisplay = eglGetCurrentDisplay();
    EGLSurface prevSurfDraw = eglGetCurrentSurface(EGL_DRAW);
    EGLSurface prevSurfRead = eglGetCurrentSurface(EGL_READ);
    EGLContext prevContext = eglGetCurrentContext();

    eglMakeCurrent(core->allocatorDevice->eglDisplay,
                   EGL_NO_SURFACE,
                   EGL_NO_SURFACE,
                   core->allocatorDevice->eglSharedContext);

    glGenFramebuffers(1, &buffer->framebuffer);

    if (buffer->framebuffer)
        glBindFramebuffer(GL_FRAMEBUFFER, buffer->framebuffer);


    glGenTextures(1, &texture->texture);

    if (!texture->texture)
    {
        free(texture);
        eglMakeCurrent(prevDisplay,
                       prevSurfDraw,
                       prevSurfRead,
                       prevContext);
        goto fail;
    }

    glBindTexture(GL_TEXTURE_2D, texture->texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    srmListAppendData(buffer->textures, texture);

    glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, stride / buffer->pixelSize);

    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 glFmt->glInternalFormat,
                 width,
                 height,
                 0,
                 glFmt->glFormat,
                 glFmt->glType,
                 pixels);

    glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, 0);

    if (buffer->framebuffer)
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture->texture, 0);

    SRMDebug("[%s] CPU buffer created using glTexImage2D.", core->allocatorDevice->name);

    glFlush();

    if (buffer->framebuffer && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
        buffer->caps |= SRM_BUFFER_CAP_READ;
    else
    {
        glDeleteFramebuffers(1, &buffer->framebuffer);
        buffer->framebuffer = 0;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    eglMakeCurrent(prevDisplay,
                   prevSurfDraw,
                   prevSurfRead,
                   prevContext);

    buffer->caps |= SRM_BUFFER_CAP_WRITE;

    return buffer;

    fail:
    SRMError("[%s] Failed to create CPU buffer.", core->allocatorDevice->name);
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
        {
            if (texture->updated)
            {
                /* TODO: Some GPUs do not require EGL image recreation when the DMA buf is updated. Check with glRead or
                 * something like that if it needs it */

                glDeleteTextures(1, &texture->texture);

                if (texture->image != EGL_NO_IMAGE)
                    eglDestroyImage(texture->device->eglDisplay, texture->image);

                free(texture);
                srmListRemoveItem(buffer->textures, item);
                break;
            }
            return texture->texture;
        }
    }

    if (!buffer->bo)
    {
        SRMError("srmBufferGetTextureID error. Buffer is not shareable.");
        return 0;
    }

    // Creates the texture
    texture = calloc(1, sizeof(struct SRMBufferTexture));
    texture->device = device;
    texture->image = EGL_NO_IMAGE;

    if (!buffer->core->allocatorDevice->capPrimeExport)
    {
        SRMError("srmBufferGetTextureID failed. Allocator device (%s) has not the PRIME export cap.", buffer->core->allocatorDevice->name);
        goto skipDMA;
        return 0;
    }

    if (!device->capPrimeImport)
    {
        SRMError("srmBufferGetTextureID failed. Target device (%s) has not the PRIME import cap.", buffer->core->allocatorDevice->name);
        goto skipDMA;
    }

    if (buffer->fd == -1)
        buffer->fd = srmBufferGetDMAFDFromBO(buffer->core->allocatorDevice, buffer->bo);

    if (buffer->fd == -1)
        goto skipDMA;

    EGLAttrib image_attribs[] = {
                               EGL_WIDTH, gbm_bo_get_width(buffer->bo),
                               EGL_HEIGHT, gbm_bo_get_height(buffer->bo),
                               EGL_LINUX_DRM_FOURCC_EXT, gbm_bo_get_format(buffer->bo),
                               EGL_DMA_BUF_PLANE0_FD_EXT, buffer->fd,
                               EGL_DMA_BUF_PLANE0_OFFSET_EXT, gbm_bo_get_offset(buffer->bo, 0),
                               EGL_DMA_BUF_PLANE0_PITCH_EXT, gbm_bo_get_stride_for_plane(buffer->bo, 0),
                               EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, (EGLAttrib)(gbm_bo_get_modifier(buffer->bo) & 0xffffffff),
                               EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, (EGLAttrib)(gbm_bo_get_modifier(buffer->bo) >> 32),
                               EGL_NONE };

    texture->image = eglCreateImage(device->eglDisplay, NULL, EGL_LINUX_DMA_BUF_EXT, NULL, image_attribs);

    skipDMA:

    if (texture->image == EGL_NO_IMAGE && device == buffer->core->allocatorDevice)
    {
        texture->image = eglCreateImage(device->eglDisplay, device->eglSharedContext, EGL_NATIVE_PIXMAP_KHR, buffer->bo, NULL);
    }

    if (texture->image == EGL_NO_IMAGE)
    {
        SRMError("srmBufferGetTextureID error. Failed to create EGL image.");
        free(texture);
        return 0;
    }

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

        EGLDisplay prevDisplay = eglGetCurrentDisplay();
        EGLSurface prevSurfDraw = eglGetCurrentSurface(EGL_DRAW);
        EGLSurface prevSurfRead = eglGetCurrentSurface(EGL_READ);
        EGLContext prevContext = eglGetCurrentContext();

        UInt8 first = 1;
        while (!srmListIsEmpty(buffer->textures))
        {
            struct SRMBufferTexture *texture = srmListPopBack(buffer->textures);
            eglMakeCurrent(texture->device->eglDisplay,
                           EGL_NO_SURFACE,
                           EGL_NO_SURFACE,
                           texture->device->eglSharedContext);

            if (first)
            {
                if (buffer->framebuffer)
                    glDeleteFramebuffers(1, &buffer->framebuffer);

                first = 0;
            }

            if (texture->texture)
                glDeleteTextures(1, &texture->texture);

            if (texture->image != EGL_NO_IMAGE)
                eglDestroyImage(texture->device->eglDisplay, texture->image);

            free(texture);
        }

        srmListDestoy(buffer->textures);

        eglMakeCurrent(prevDisplay,
                       prevSurfDraw,
                       prevSurfRead,
                       prevContext);
    }

    if (buffer->bo)
    {
        if (buffer->map)
        {
            if (buffer->mapData)
                gbm_bo_unmap(buffer->bo, buffer->mapData);
            else
                munmap(buffer->map, gbm_bo_get_height(buffer->bo)*gbm_bo_get_stride(buffer->bo));
        }

        if (buffer->fd != -1)
            close(buffer->fd);

        gbm_bo_destroy(buffer->bo);
    }

    free(buffer);
}

UInt32 srmBufferWrite(SRMBuffer *buffer, UInt32 stride, UInt32 dstX, UInt32 dstY, UInt32 dstWidth, UInt32 dstHeight, void *pixels)
{
    if (!(buffer->caps & SRM_BUFFER_CAP_WRITE))
        goto fail;

    if (buffer->map)
    {
        UInt32 dstOffset = buffer->offset + dstY*buffer->stride + dstX*buffer->pixelSize;
        UInt32 srcOffset = 0;

        for (UInt32 i = 0; i < dstHeight; i++)
        {
            memcpy(&buffer->map[dstOffset],
                   &pixels[srcOffset],
                   buffer->pixelSize * dstWidth);

            dstOffset += buffer->stride;
            srcOffset += stride;
        }

        SRMListForeach(texIt, buffer->textures)
        {
            struct SRMBufferTexture *tex = srmListItemGetData(texIt);
            tex->updated = 1;
        }

        /* SRMDebug("[%s] Buffer written using mapping.", buffer->core->allocatorDevice->name); */

        return 1;
    }
    else if (buffer->framebuffer)
    {
        glBindTexture(GL_TEXTURE_2D, srmBufferGetTextureID(buffer->core->allocatorDevice, buffer));
        glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, stride / buffer->pixelSize);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS_EXT, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS_EXT, 0);

        glTexSubImage2D(GL_TEXTURE_2D, 0, dstX, dstY, dstWidth, dstHeight,
                        buffer->glFormat, buffer->glType, pixels);

        glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, 0);

        glFlush();

        /* SRMDebug("[%s] Buffer written using glTexSubImage2D.", buffer->core->allocatorDevice->name); */

        return 1;
    }

    fail:
    SRMError("[%s] Buffer can not be written.", buffer->core->allocatorDevice->name);
    return 0;
}
