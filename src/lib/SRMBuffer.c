#include <GL/gl.h>
#include <assert.h>
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

#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>

SRMBuffer *srmBufferCreateFromDMA(SRMCore *core, SRMDevice *allocator, SRMBufferDMAData *dmaData)
{
    if (dmaData->num_fds < 1 || dmaData->num_fds > 4)
    {
        SRMError("srmBufferCreateFromDMA: Invalid number of planes provided.");
        return NULL;
    }

    if (dmaData->width == 0 || dmaData->height == 0)
    {
        SRMError("srmBufferCreateFromDMA: Invalid buffer dimensions %dx%dpx.", dmaData->width, dmaData->height);
        return NULL;
    }

    SRMBuffer *buffer = srmBufferCreate(core, allocator);
    buffer->src = SRM_BUFFER_SRC_DMA;    
    memcpy(&buffer->dma, dmaData, sizeof(*dmaData));
    srmBufferSetTargetFromFormat(buffer);

    if (buffer->target == GL_NONE)
        goto fail;

    return buffer;
fail:
    SRMError("srmBufferCreateFromDMA: Failed to import DMA buffer.");
    srmBufferDestroy(buffer);
    return NULL;
}

SRMBuffer *srmBufferCreateFromCPU(SRMCore *core, SRMDevice *allocator,
                                  UInt32 width, UInt32 height, UInt32 stride,
                                  const void *pixels, SRM_BUFFER_FORMAT format)
{
    if (width == 0 || height == 0)
    {
        SRMError("srmBufferCreateFromCPU: Invalid buffer dimensions %dx%dpx.", width, height);
        return NULL;
    }

    SRMBuffer *buffer = srmBufferCreate(core, allocator);
    buffer->src = SRM_BUFFER_SRC_CPU;
    buffer->dma.num_fds = 1;
    buffer->dma.width = width;
    buffer->dma.height = height;
    buffer->dma.format = format;
    buffer->dma.modifiers[0] = DRM_FORMAT_MOD_LINEAR;

    const SRMGLFormat *glFmt;

    if (buffer->allocator->cpuBufferWriteMode == SRM_BUFFER_WRITE_MODE_GLES)
        goto glesOnly;

    srmBufferSetTargetFromFormat(buffer);

    if (buffer->target == GL_NONE)
        goto glesOnly;

    buffer->bo = srmBufferCreateLinearBO(buffer->allocator->gbm, width, height, format);

    if (!buffer->bo)
        goto glesOnly;

    buffer->bpp = gbm_bo_get_bpp(buffer->bo);

    if (buffer->bpp % 8 != 0)
    {
        SRMWarning("[%s] Buffer bpp must be a multiple of 8.", buffer->allocator->name);
        goto glesOnly;
    }

    buffer->dma.modifiers[0] = gbm_bo_get_modifier(buffer->bo);
    buffer->dma.strides[0] = gbm_bo_get_stride(buffer->bo);
    buffer->pixelSize = buffer->bpp/8;

    if (buffer->allocator->cpuBufferWriteMode == SRM_BUFFER_WRITE_MODE_PRIME)
    {
        buffer->dma.fds[0] = srmBufferGetDMAFDFromBO(buffer->allocator, buffer->bo);

        if (buffer->dma.fds[0] >= 0)
        {
            const size_t len = height * buffer->dma.strides[0];
            buffer->map = srmBufferMapFD(buffer->dma.fds[0], len, &buffer->caps);

            if (buffer->map)
            {
                buffer->writeMode = SRM_BUFFER_WRITE_MODE_PRIME;
                goto mapWrite;
            }
        }
    }

    /* Test if gbm_bo_map works */

    UInt32 testStride;
    buffer->map = gbm_bo_map(buffer->bo, 0, 0, width, height, GBM_BO_TRANSFER_READ_WRITE, &testStride, &buffer->mapData);

    if (!buffer->map)
    {
        buffer->mapData = NULL;
        buffer->map = gbm_bo_map(buffer->bo, 0, 0, width, height, GBM_BO_TRANSFER_WRITE, &testStride, &buffer->mapData);

        if (!buffer->map)
        {
            buffer->mapData = NULL;
            buffer->map = NULL;
            goto glesOnly;
        }

        buffer->caps |= SRM_BUFFER_CAP_WRITE;
    }
    else
        buffer->caps |= SRM_BUFFER_CAP_READ | SRM_BUFFER_CAP_WRITE;

    gbm_bo_unmap(buffer->bo, buffer->mapData);
    buffer->map = NULL;
    buffer->mapData = NULL;
    buffer->writeMode = SRM_BUFFER_WRITE_MODE_GBM;

    mapWrite:

    buffer->dma.offsets[0] = gbm_bo_get_offset(buffer->bo, 0);
    buffer->caps |= SRM_BUFFER_CAP_MAP;
    srmBufferWrite(buffer, stride, 0, 0, width, height, pixels);
    return buffer;

    glesOnly:

    buffer->target = GL_TEXTURE_2D;
    buffer->writeMode = SRM_BUFFER_WRITE_MODE_GLES;
    glFmt = srmFormatDRMToGL(format);

    if (!glFmt)
    {
        SRMError("[%s] Could not find the equivalent GL format and type from DRM format %s.",
                 buffer->allocator->name,
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
        buffer->dma.strides[0] = buffer->pixelSize*width;
    }

    struct SRMBufferTexture *texture = calloc(1, sizeof(struct SRMBufferTexture));
    texture->device = buffer->allocator;
    texture->image = EGL_NO_IMAGE;

    srmSaveContext();
    srmDeviceMakeCurrent(buffer->allocator);
    glGenTextures(1, &texture->texture);

    if (!texture->texture)
    {
        free(texture);
        srmRestoreContext();
        goto fail;
    }

    glBindTexture(GL_TEXTURE_2D, texture->texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    srmListAppendData(buffer->textures, texture);

    if (pixels)
    {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, stride / buffer->pixelSize);
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
    }
    else
    {
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     glFmt->glInternalFormat,
                     width,
                     height,
                     0,
                     glFmt->glFormat,
                     glFmt->glType,
                     NULL);
    }

    if (buffer->allocator->eglExtensions.KHR_gl_texture_2D_image)
    {
        static const EGLint attribs[3] =
        {
            EGL_IMAGE_PRESERVED_KHR,
            EGL_TRUE,
            EGL_NONE
        };

        /* This is used to later get a gbm bo if the buffer is used for scanout */
        texture->image = buffer->allocator->eglFunctions.eglCreateImageKHR(
            eglGetCurrentDisplay(),
            eglGetCurrentContext(),
            EGL_GL_TEXTURE_2D_KHR,
            (EGLClientBuffer)(UInt64)texture->texture,
            attribs);
    }

    srmBufferCreateSync(buffer);
    srmRestoreContext();

    /* TODO: Add read cap */

    buffer->caps |= SRM_BUFFER_CAP_WRITE;
    return buffer;

fail:
    SRMError("[%s] Failed to create CPU buffer.", buffer->allocator->name);
    srmBufferDestroy(buffer);
    return NULL;
}

SRMBuffer *srmBufferCreateFromWaylandDRM(SRMCore *core, void *wlBuffer)
{
    SRMBuffer *buffer = srmBufferCreate(core, NULL);
    buffer->src = SRM_BUFFER_SRC_WL_DRM;
    buffer->bo = gbm_bo_import(buffer->allocator->gbm, GBM_BO_IMPORT_WL_BUFFER, wlBuffer, GBM_BO_USE_RENDERING);

    if (!buffer->bo)
    {
        SRMDebug("[%s] srmBufferCreateFromWaylandDRM: Failed to create gbm_bo from wl_drm.", buffer->allocator->name);
        goto fail;
    }

    srmBufferFillParamsFromBO(buffer, buffer->bo);
    srmBufferSetTargetFromFormat(buffer);

    /* Just in case DMA formats are not supported but KHR_image_pixmap is */
    if (buffer->target == GL_NONE)
        buffer->target = GL_TEXTURE_2D;

    if (!srmBufferGetTextureID(buffer->allocator, buffer))
        goto fail;

    return buffer;

fail:
    SRMDebug("[%s] srmBufferCreateFromWaylandDRM: Failed to get texture.", buffer->allocator->name);
    free(buffer);
    return NULL;
}

GLuint srmBufferGetTextureID(SRMDevice *device, SRMBuffer *buffer)
{
    if (!device || !buffer)
    {
        SRMError("[SRMBuffer] srmBufferGetTextureID: Invalid device or buffer.");
        return 0;
    }

    if ((buffer->src == SRM_BUFFER_SRC_WL_DRM || buffer->src == SRM_BUFFER_SRC_GL) && device != buffer->allocator)
    {
        SRMError("[%s] srmBufferGetTextureID: wl_drm buffers and GL wrappers can only be accessed from allocator device.", device->shortName);
        return 0;
    }

    // Check if already created
    struct SRMBufferTexture *texture;
    SRMListForeach(item, buffer->textures)
    {
        texture = srmListItemGetData(item);

        if (texture->device == device)
        {
            srmBufferWaitSync(buffer);
            return texture->texture;
        }
    }

    if (buffer->target == GL_TEXTURE_2D && !device->glExtensions.OES_EGL_image)
    {
        SRMError("[%s] Failed to get texture id from EGL image, OES_EGL_image extension not available.", device->shortName);
        return 0;
    }

    if (buffer->target == GL_TEXTURE_EXTERNAL_OES && !device->glExtensions.OES_EGL_image_external)
    {
        SRMError("[%s] Failed to get texture id from EGL image, OES_EGL_image_external extension not available.", device->shortName);
        return 0;
    }

    pthread_mutex_lock(&buffer->mutex);

    texture = calloc(1, sizeof(struct SRMBufferTexture));
    texture->device = device;
    texture->image = EGL_NO_IMAGE;

    if (device->eglExtensions.KHR_image_pixmap && device == buffer->allocator && buffer->bo && device)
    {
        static const EGLint attribs[3] =
        {
            EGL_IMAGE_PRESERVED_KHR,
            EGL_TRUE,
            EGL_NONE
        };

        texture->image = device->eglFunctions.eglCreateImageKHR(device->eglDisplay, EGL_NO_CONTEXT, EGL_NATIVE_PIXMAP_KHR, buffer->bo, attribs);

        if (texture->image != EGL_NO_IMAGE)
            goto skipDMA;
    }
    
    if (buffer->dma.fds[0] == -1 && buffer->bo)
    {
        buffer->dma.fds[0] = srmBufferGetDMAFDFromBO(buffer->allocator, buffer->bo);

        for (UInt32 i = 1; i < buffer->dma.num_fds; i++)
            buffer->dma.fds[i] = buffer->dma.fds[0];
    }

    if (buffer->dma.fds[0] == -1 || !device->eglExtensions.EXT_image_dma_buf_import)
        goto skipDMA;

    texture->image = srmEGLCreateImageFromDMA(device, &buffer->dma);

    skipDMA:

    if (texture->image == EGL_NO_IMAGE)
    {
        SRMError("srmBufferGetTextureID error. Failed to create EGL image.");
        free(texture);
        pthread_mutex_unlock(&buffer->mutex);
        return 0;
    }

    srmSaveContext();
    srmDeviceMakeCurrent(device);
    glGenTextures(1, &texture->texture);
    glBindTexture(buffer->target, texture->texture);
    device->eglFunctions.glEGLImageTargetTexture2DOES(buffer->target, texture->image);

    glTexParameteri(buffer->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(buffer->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(buffer->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(buffer->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    srmRestoreContext();
    srmListAppendData(buffer->textures, texture);
    pthread_mutex_unlock(&buffer->mutex);
    return texture->texture;
}

void srmBufferDestroy(SRMBuffer *buffer)
{
    pthread_mutex_lock(&buffer->mutex);

    buffer->refCount--;

    if (buffer->refCount > 0)
    {
        pthread_mutex_unlock(&buffer->mutex);
        return;
    }

    if (buffer->scanout.fb != 0)
    {
        drmModeRmFB(buffer->allocator->fd, buffer->scanout.fb);
        buffer->scanout.fb = 0;
    }

    if (buffer->scanout.bo)
    {
        gbm_bo_destroy(buffer->scanout.bo);
        buffer->scanout.bo = NULL;
    }

    if (buffer->textures)
    {
        srmSaveContext();

        while (!srmListIsEmpty(buffer->textures))
        {
            struct SRMBufferTexture *texture = srmListPopBack(buffer->textures);

            srmDeviceMakeCurrent(texture->device);

            if (!buffer->keepTexturesAlive && texture->texture != 0)
                glDeleteTextures(1, &texture->texture);

            if (texture->image != EGL_NO_IMAGE)
                eglDestroyImage(texture->device->eglDisplay, texture->image);

            free(texture);
        }

        srmListDestroy(buffer->textures);
        srmRestoreContext();
    }

    for (UInt32 i = 0; i < buffer->dma.num_fds; i++)
        if (buffer->dma.fds[i] != -1)
            close(buffer->dma.fds[i]);

    if (buffer->bo)
    {
        if (buffer->map)
            munmap(buffer->map, buffer->dma.height * buffer->dma.strides[0]);

        // Do not destroy the user's bo
        if (buffer->src != SRM_BUFFER_SRC_GBM)
            gbm_bo_destroy(buffer->bo);
    }

    pthread_mutex_unlock(&buffer->mutex);
    pthread_mutex_destroy(&buffer->mutex);
    free(buffer);
}

UInt8 srmBufferWrite(SRMBuffer *buffer, UInt32 stride, UInt32 dstX, UInt32 dstY, UInt32 dstWidth, UInt32 dstHeight, const void *pixels)
{
    if (!pixels)
        return 0;

    if (!(buffer->caps & SRM_BUFFER_CAP_WRITE))
        goto fail;

    if (buffer->src != SRM_BUFFER_SRC_CPU && buffer->target == GL_TEXTURE_EXTERNAL_OES)
    {
        SRMError("[%s] srmBufferWrite() failed. Buffers with the GL_TEXTURE_EXTERNAL_OES target are immutable.", buffer->allocator->name);
        return 0;
    }

    if (buffer->writeMode == SRM_BUFFER_WRITE_MODE_PRIME)
    {
        assert(buffer->map != NULL);

        const UInt8 *src = pixels;
        UInt8 *dst = buffer->map;
        dst = &dst[buffer->dma.offsets[0] + dstY*buffer->dma.strides[0] + dstX*buffer->pixelSize];

        pthread_mutex_lock(&buffer->mutex);
        buffer->sync.flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_WRITE;
        ioctl(buffer->dma.fds[0], DMA_BUF_IOCTL_SYNC, &buffer->sync);

        if (dstX == 0 && dstWidth == buffer->dma.width && stride == buffer->dma.strides[0])
        {
            memcpy(dst,
                   src,
                   stride * dstHeight);
        }
        else
        {
            for (UInt32 i = 0; i < dstHeight; i++)
            {
                memcpy(dst,
                       src,
                       buffer->pixelSize * dstWidth);

                dst += buffer->dma.strides[0];
                src += stride;
            }
        }

        buffer->sync.flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_WRITE;
        ioctl(buffer->dma.fds[0], DMA_BUF_IOCTL_SYNC, &buffer->sync);
        pthread_mutex_unlock(&buffer->mutex);
        return 1;
    }
    else if (buffer->writeMode == SRM_BUFFER_WRITE_MODE_GBM)
    {
        assert(buffer->bo != NULL);

        UInt32 mapStride;
        void *mapData = NULL;

        pthread_mutex_lock(&buffer->mutex);

        UInt8 *dst = gbm_bo_map(buffer->bo, dstX, dstY, dstWidth, dstHeight, GBM_BO_TRANSFER_WRITE, &mapStride, &mapData);

        if (dst == NULL)
        {
            pthread_mutex_unlock(&buffer->mutex);
            return 0;
        }

        const UInt8 *src = pixels;

        if (dstX == 0 && dstWidth == buffer->dma.width && stride == mapStride)
        {
            memcpy(dst,
                   src,
                   stride * dstHeight);
        }
        else
        {
            for (UInt32 i = 0; i < dstHeight; i++)
            {
                memcpy(dst,
                       src,
                       buffer->pixelSize * dstWidth);

                dst += mapStride;
                src += stride;
            }
        }

        gbm_bo_unmap(buffer->bo, mapData);
        pthread_mutex_unlock(&buffer->mutex);
        return 1;
    }
    else if (buffer->writeMode == SRM_BUFFER_WRITE_MODE_GLES)
    {
        srmSaveContext();
        srmDeviceMakeCurrent(buffer->allocator);

        glBindTexture(GL_TEXTURE_2D, srmBufferGetTextureID(buffer->allocator, buffer));
        glPixelStorei(GL_UNPACK_ALIGNMENT, buffer->pixelSize);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, stride / buffer->pixelSize);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

        glTexSubImage2D(GL_TEXTURE_2D, 0, dstX, dstY, dstWidth, dstHeight,
                        buffer->glFormat, buffer->glType, pixels);

        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        srmBufferCreateSync(buffer);
        srmRestoreContext();
        return 1;
    }

    fail:
    SRMError("[%s] Buffer can not be written.", buffer->allocator->name);
    return 0;
}

SRM_BUFFER_FORMAT srmBufferGetFormat(SRMBuffer *buffer)
{
    return buffer->dma.format;
}

UInt32 srmBufferGetWidth(SRMBuffer *buffer)
{
    return buffer->dma.width;
}

UInt32 srmBufferGetHeight(SRMBuffer *buffer)
{
    return buffer->dma.height;
}

SRMBuffer *srmBufferCreateFromGBM(SRMCore *core, struct gbm_bo *bo)
{
    SRMDevice *allocDev = NULL;

    SRMListForeach(devIt, core->devices)
    {
        SRMDevice *dev = srmListItemGetData(devIt);
        if (dev->gbm == gbm_bo_get_device(bo))
        {
            allocDev = dev;
            break;
        }
    }

    if (!allocDev)
    {
        SRMError("srmBufferCreateFromGBM: Failed to find SRMDevice for gbm_device.");
        return NULL;
    }

    if (!allocDev->eglExtensions.KHR_image_pixmap && !allocDev->eglExtensions.EXT_image_dma_buf_import)
    {
        SRMError("srmBufferCreateFromGBM: KHR_image_pixmap and EXT_image_dma_buf_import_modifiers extensions not available.");
        return NULL;
    }

    SRMBuffer *buffer = srmBufferCreate(core, allocDev);
    buffer->src = SRM_BUFFER_SRC_GBM;
    srmBufferFillParamsFromBO(buffer, bo);
    buffer->bo = bo;
    srmBufferSetTargetFromFormat(buffer);

    /* Just in case DMA formats are not supported but KHR_image_pixmap is */
    if (buffer->target == GL_NONE)
        buffer->target = GL_TEXTURE_2D;

    if (buffer->dma.modifiers[0] != DRM_FORMAT_MOD_LINEAR || buffer->dma.num_fds != 1)
        goto skipMap;

    buffer->dma.fds[0] = srmBufferGetDMAFDFromBO(buffer->allocator, buffer->bo);

    if (buffer->dma.fds[0] >= 0)
    {
        buffer->map = mmap(NULL, buffer->dma.height * buffer->dma.strides[0], PROT_READ | PROT_WRITE, MAP_SHARED, buffer->dma.fds[0], 0);

        if (buffer->map == MAP_FAILED)
        {
            buffer->map = mmap(NULL, buffer->dma.height * buffer->dma.strides[0], PROT_WRITE, MAP_SHARED, buffer->dma.fds[0], 0);

            if (buffer->map == MAP_FAILED)
            {
                SRMWarning("[%s] Directly mapping buffer DMA fd failed. Trying gbm_bo_map.", buffer->allocator->name);
            }
        }
    }

    if (buffer->map != MAP_FAILED)
    {
        buffer->caps |= SRM_BUFFER_CAP_WRITE;
        goto skipMap;
    }

    buffer->map = gbm_bo_map(buffer->bo, 0, 0, buffer->dma.width, buffer->dma.height, GBM_BO_TRANSFER_READ, &buffer->dma.strides[0], &buffer->mapData);

    if (!buffer->map)
        buffer->mapData = NULL;
    else
    {
        buffer->caps |= SRM_BUFFER_CAP_WRITE;
        SRMDebug("[%s] Buffer mapped with gbm_bo_map().", buffer->allocator->name);
    }

skipMap:
    return buffer;
}

SRMDevice *srmBufferGetAllocatorDevice(SRMBuffer *buffer)
{
    return buffer->allocator;
}

UInt8 srmBufferRead(SRMBuffer *buffer, Int32 srcX, Int32 srcY, Int32 srcW, Int32 srcH, Int32 dstX, Int32 dstY, Int32 dstStride, UInt8 *dstBuffer)
{
    /* TODO: Use READ cap and add GL support */

    if (buffer->map && buffer->dma.modifiers[0] == DRM_FORMAT_MOD_LINEAR)
    {
        pthread_mutex_lock(&buffer->mutex);
        buffer->sync.flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_READ;
        ioctl(buffer->dma.fds[0], DMA_BUF_IOCTL_SYNC, &buffer->sync);

        UInt8 *src = buffer->map;

        for (Int32 i = 0; i < srcH; i++)
        {
            memcpy(&dstBuffer[(i + dstY)*dstStride + buffer->pixelSize*dstX],
                   &src[(i + srcY)*buffer->dma.strides[0] + buffer->pixelSize*srcX],
                   srcW*buffer->pixelSize);
        }

        buffer->sync.flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_READ;
        ioctl(buffer->dma.fds[0], DMA_BUF_IOCTL_SYNC, &buffer->sync);
        pthread_mutex_unlock(&buffer->mutex);
        return 1;
    }

    return 0;
}

GLenum srmBufferGetTextureTarget(SRMBuffer *buffer)
{
    return buffer->target;
}

EGLImage srmBufferGetEGLImage(SRMDevice *device, SRMBuffer *buffer)
{
    if (!srmBufferGetTextureID(device, buffer))
        return EGL_NO_IMAGE;

    struct SRMBufferTexture *texture;
    SRMListForeach(item, buffer->textures)
    {
        texture = srmListItemGetData(item);

        if (texture->device == device)
            return texture->image;
    }

    return EGL_NO_IMAGE;
}

SRMBuffer *srmBufferCreateGLTextureWrapper(SRMDevice *device, GLuint id, GLenum target, SRM_BUFFER_FORMAT format, UInt32 width, UInt32 height, UInt8 transferOwnership)
{
    if (!device)
    {
        SRMError("srmBufferCreateGLTextureWrapper: Invalid SRMDevice (NULL).");
        return NULL;
    }

    if (width == 0 || height == 0)
    {
        SRMError("srmBufferCreateGLTextureWrapper: Invalid buffer dimensions %dx%dpx.", width, height);
        return NULL;
    }

    if (id == 0)
    {
        SRMError("srmBufferCreateGLTextureWrapper: Invalid texture ID (0).");
        return NULL;
    }

    const SRMGLFormat *glFmt = srmFormatDRMToGL(format);

    if (!glFmt)
    {
        SRMError("srmBufferCreateGLTextureWrapper: Could not find the equivalent GL format and type from DRM format %s.", drmGetFormatName(format));
        return NULL;
    }

    UInt32 depth, bpp;

    if (!srmFormatGetDepthBpp(format, &depth, &bpp))
    {
        SRMError("srmBufferCreateGLTextureWrapper: Failed to get depth and bpp for DRM format %s.", drmGetFormatName(format));
        return NULL;
    }

    SRMBuffer *buffer = srmBufferCreate(device->core, device);
    buffer->src = SRM_BUFFER_SRC_GBM;
    buffer->writeMode = SRM_BUFFER_WRITE_MODE_GLES;
    buffer->caps |= SRM_BUFFER_CAP_WRITE;
    buffer->keepTexturesAlive = !transferOwnership;
    buffer->target = target;
    buffer->allocator = device;
    buffer->glType = glFmt->glType;
    buffer->glFormat = glFmt->glFormat;
    buffer->glInternalFormat = glFmt->glInternalFormat;
    buffer->dma.num_fds = 1;
    buffer->dma.format = format;
    buffer->dma.width = width;
    buffer->dma.height = height;
    buffer->bpp = bpp;
    buffer->pixelSize = buffer->bpp/8;
    buffer->dma.strides[0] = buffer->pixelSize*width;

    struct SRMBufferTexture *texture = calloc(1, sizeof(struct SRMBufferTexture));
    texture->device = buffer->allocator;
    texture->texture = id;
    texture->image = EGL_NO_IMAGE;
    srmListAppendData(buffer->textures, texture);

    if (buffer->allocator->eglExtensions.KHR_gl_texture_2D_image)
    {
        static const EGLint attribs[3] =
        {
            EGL_IMAGE_PRESERVED_KHR,
            EGL_TRUE,
            EGL_NONE
        };

        srmSaveContext();
        srmDeviceMakeCurrent(buffer->allocator);

        /* This is used to later get a gbm bo if the buffer is used for scanout */
        texture->image = buffer->allocator->eglFunctions.eglCreateImageKHR(
            eglGetCurrentDisplay(),
            eglGetCurrentContext(),
            EGL_GL_TEXTURE_2D_KHR,
            (EGLClientBuffer)(UInt64)texture->texture,
            attribs);

        srmRestoreContext();
    }

    return buffer;
}
