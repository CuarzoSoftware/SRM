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

SRMBuffer *srmBufferCreateFromDMA(SRMCore *core, SRMBufferDMAData *dmaData)
{
    if (dmaData->num_fds < 1 || dmaData->num_fds > 4)
    {
        SRMError("[%s] Can not import DMA buffer with %d fds.",
                 core->allocatorDevice->name, dmaData->num_fds);
        return NULL;
    }

    if (dmaData->width == 0 || dmaData->height == 0)
    {
        SRMError("[%s] Can not import DMA buffer with size %dx%dpx.",
                 core->allocatorDevice->name, dmaData->width, dmaData->height);
        return NULL;
    }

    SRMBuffer *buffer = srmBufferCreate(core);
    buffer->src = SRM_BUFFER_SRC_DMA;
    buffer->format = dmaData->format;
    buffer->width = dmaData->width;
    buffer->height = dmaData->height;
    buffer->planesCount = dmaData->num_fds;

    for (UInt32 i = 0; i < buffer->planesCount; i++)
    {
        buffer->fds[i] = dmaData->fds[i];
        buffer->offsets[i] = dmaData->offsets[i];
        buffer->strides[i] = dmaData->strides[i];
        buffer->modifiers[i] = dmaData->modifiers[i];

    }

    return buffer;
}

SRMBuffer *srmBufferCreateFromCPU(SRMCore *core, UInt32 width, UInt32 height, UInt32 stride, const void *pixels, SRM_BUFFER_FORMAT format)
{
    if (width == 0 || height == 0)
    {
        SRMError("[%s] Can not create CPU buffer with size %dx%dpx.",
                 core->allocatorDevice->name, width, height);
        return NULL;
    }

    SRMBuffer *buffer = srmBufferCreate(core);
    buffer->src = SRM_BUFFER_SRC_CPU;
    buffer->planesCount = 1;
    buffer->width = width;
    buffer->height = height;
    buffer->format = format;
    const SRMGLFormat *glFmt;

    pthread_mutex_lock(&buffer->mutex);

    buffer->modifiers[0] = DRM_FORMAT_MOD_LINEAR;
    buffer->bo = gbm_bo_create_with_modifiers(core->allocatorDevice->gbm,
                                              width,
                                              height,
                                              format,
                                              &buffer->modifiers[0],
                                              1);

    if (!buffer->bo)
    {
        SRMWarning("gbm_bo_create_with_modifiers failed.");

        // Try to use linear so that can be mapped
        buffer->flags = GBM_BO_USE_RENDERING | GBM_BO_USE_LINEAR | GBM_BO_USE_SCANOUT;
        buffer->bo = gbm_bo_create(core->allocatorDevice->gbm, width, height, format, buffer->flags);
    }

    if (!buffer->bo)
    {
        SRMWarning("GBM_BO_USE_RENDERING | GBM_BO_USE_LINEAR | GBM_BO_USE_SCANOUT failed.");
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

    buffer->modifiers[0] = gbm_bo_get_modifier(buffer->bo);
    buffer->strides[0] = gbm_bo_get_stride(buffer->bo);
    buffer->pixelSize = buffer->bpp/8;

    buffer->fds[0] = srmBufferGetDMAFDFromBO(core->allocatorDevice, buffer->bo);

    if (buffer->fds[0] >= 0)
    {
        // Map the DMA buffer into user space
        buffer->map = mmap(NULL, height * buffer->strides[0], PROT_READ | PROT_WRITE, MAP_SHARED, buffer->fds[0], 0);

        if (buffer->map == MAP_FAILED)
        {
            buffer->map = mmap(NULL, height * buffer->strides[0], PROT_WRITE, MAP_SHARED, buffer->fds[0], 0);

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
    pthread_mutex_unlock(&buffer->mutex);
    return buffer;

    gbmMap:

    buffer->map = gbm_bo_map(buffer->bo, 0, 0, width, height, GBM_BO_TRANSFER_READ, &buffer->strides[0], &buffer->mapData);

    if (!buffer->map)
    {
        SRMWarning("[%s] Failed to map DMA FD. Tying gbm_bo_write instead.", core->allocatorDevice->name);
        goto gbmWrite;
    }

    SRMDebug("[%s] Buffer mapped with gbm_bo_map().", core->allocatorDevice->name);

    mapWrite:

    buffer->offsets[0] = gbm_bo_get_offset(buffer->bo, 0);

    buffer->caps |= SRM_BUFFER_CAP_READ | SRM_BUFFER_CAP_WRITE | SRM_BUFFER_CAP_MAP;

    pthread_mutex_unlock(&buffer->mutex);
    srmBufferWrite(buffer, stride, 0, 0, width, height, pixels);
    pthread_mutex_lock(&buffer->mutex);

    SRMDebug("[%s] CPU buffer created using mapping.", core->allocatorDevice->name);
    pthread_mutex_unlock(&buffer->mutex);

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
        buffer->strides[0] = buffer->pixelSize*width;
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

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
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

    pthread_mutex_unlock(&buffer->mutex);
    return buffer;

    fail:
    SRMError("[%s] Failed to create CPU buffer.", core->allocatorDevice->name);
    pthread_mutex_unlock(&buffer->mutex);
    srmBufferDestroy(buffer);
    return NULL;
}

SRMBuffer *srmBufferCreateFromWaylandDRM(SRMCore *core, void *wlBuffer)
{
    SRMBuffer *buffer = srmBufferCreate(core);

    pthread_mutex_lock(&buffer->mutex);
    buffer->src = SRM_BUFFER_SRC_WL_DRM;

    buffer->bo = gbm_bo_import(core->allocatorDevice->gbm, GBM_BO_IMPORT_WL_BUFFER, wlBuffer, GBM_BO_USE_RENDERING);

    if (!buffer->bo)
    {
        SRMDebug("[%s] Failed to create buffer from wl_drm.", core->allocatorDevice->name);
        goto fail;
    }

    buffer->planesCount = gbm_bo_get_plane_count(buffer->bo);
    buffer->bpp = gbm_bo_get_bpp(buffer->bo);
    buffer->pixelSize = buffer->bpp/8;
    buffer->format = gbm_bo_get_format(buffer->bo);
    buffer->width = gbm_bo_get_width(buffer->bo);
    buffer->height = gbm_bo_get_height(buffer->bo);

    for (UInt32 i = 0; i < buffer->planesCount; i++)
    {
        buffer->strides[i] = gbm_bo_get_stride_for_plane(buffer->bo, i);
        buffer->offsets[i] = gbm_bo_get_offset(buffer->bo, i);
    }

    pthread_mutex_unlock(&buffer->mutex);
    return buffer;

    fail:
    pthread_mutex_unlock(&buffer->mutex);
    free(buffer);
    return NULL;
}

GLuint srmBufferGetTextureID(SRMDevice *device, SRMBuffer *buffer)
{
    if (buffer->src == SRM_BUFFER_SRC_WL_DRM && device != buffer->core->allocatorDevice)
    {
        SRMError("[%s] wl_drm buffers can only be accessed from allocator device.");
        return 0;
    }

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

                srmCoreSendDeallocatorMessage(device->core,
                                              SRM_DEALLOCATOR_MSG_DESTROY_BUFFER,
                                              device,
                                              texture->texture,
                                              0,
                                              texture->image);

                free(texture);
                srmListRemoveItem(buffer->textures, item);
                break;
            }

            return texture->texture;
        }
    }

    pthread_mutex_lock(&buffer->mutex);

    // Creates the texture
    texture = calloc(1, sizeof(struct SRMBufferTexture));
    texture->device = device;
    texture->image = EGL_NO_IMAGE;

    UInt32 index = 0;
    EGLAttrib imageAttribs[sizeof(EGLAttrib)*(6 + 10*buffer->planesCount) + 1];

    if (device == buffer->core->allocatorDevice && buffer->bo)
    {
        texture->image = eglCreateImage(device->eglDisplay, device->eglSharedContext, EGL_NATIVE_PIXMAP_KHR, buffer->bo, NULL);

        if (texture->image != EGL_NO_IMAGE)
            goto skipDMA;
    }

    if (!buffer->core->allocatorDevice->capPrimeExport)
    {
        SRMError("srmBufferGetTextureID failed. Allocator device (%s) has not the PRIME export cap.", buffer->core->allocatorDevice->name);
        goto skipDMA;
    }

    if (!device->capPrimeImport)
    {
        SRMError("srmBufferGetTextureID failed. Target device (%s) has not the PRIME import cap.", buffer->core->allocatorDevice->name);
        goto skipDMA;
    }

    if (buffer->fds[0] == -1)
    {
        buffer->fds[0] = srmBufferGetDMAFDFromBO(buffer->core->allocatorDevice, buffer->bo);

        for (UInt32 i = 1; i < buffer->planesCount; i++)
            buffer->fds[i] = buffer->fds[0];
    }

    if (buffer->fds[0] == -1)
        goto skipDMA;

    imageAttribs[index++] = EGL_WIDTH;
    imageAttribs[index++] = buffer->width;
    imageAttribs[index++] = EGL_HEIGHT;
    imageAttribs[index++] = buffer->height;
    imageAttribs[index++] = EGL_LINUX_DRM_FOURCC_EXT;
    imageAttribs[index++] = buffer->format;

    EGL_DMA_PLANE_DEF(0)
    EGL_DMA_PLANE_DEF(1)
    EGL_DMA_PLANE_DEF(2)
    EGL_DMA_PLANE_DEF(3)

    imageAttribs[index++] = EGL_NONE;

    texture->image = eglCreateImage(device->eglDisplay, NULL, EGL_LINUX_DMA_BUF_EXT, NULL, imageAttribs);

    skipDMA:

    if (texture->image == EGL_NO_IMAGE)
    {
        SRMError("srmBufferGetTextureID error. Failed to create EGL image.");
        free(texture);
        pthread_mutex_unlock(&buffer->mutex);
        return 0;
    }

    glGenTextures(1, &texture->texture);
    glBindTexture(GL_TEXTURE_2D, texture->texture);
    device->eglFunctions.glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, texture->image);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    srmListAppendData(buffer->textures, texture);
    pthread_mutex_unlock(&buffer->mutex);
    return texture->texture;
}

void srmBufferDestroy(SRMBuffer *buffer)
{
    pthread_mutex_lock(&buffer->mutex);

    if (buffer->framebuffer)
    {
        srmCoreSendDeallocatorMessage(buffer->core,
                                      SRM_DEALLOCATOR_MSG_DESTROY_BUFFER,
                                      buffer->core->allocatorDevice,
                                      0,
                                      buffer->framebuffer,
                                      EGL_NO_IMAGE);
    }

    if (buffer->textures)
    {
        while (!srmListIsEmpty(buffer->textures))
        {
            struct SRMBufferTexture *texture = srmListPopBack(buffer->textures);

            srmCoreSendDeallocatorMessage(buffer->core,
                                          SRM_DEALLOCATOR_MSG_DESTROY_BUFFER,
                                          texture->device,
                                          texture->texture,
                                          0,
                                          texture->image);

            free(texture);
        }

        srmListDestoy(buffer->textures);
    }

    while (!srmListIsEmpty(buffer->core->deallocatorMessages))
        usleep(10);


    for (UInt32 i = 0; i < buffer->planesCount; i++)
    {
        if (buffer->fds[i] != -1)
            close(buffer->fds[i]);
    }

    if (buffer->bo)
    {
        if (buffer->map)
        {
            if (buffer->mapData)
                gbm_bo_unmap(buffer->bo, buffer->mapData);
            else
                munmap(buffer->map, buffer->height * buffer->strides[0]);
        }

        gbm_bo_destroy(buffer->bo);
    }

    pthread_mutex_unlock(&buffer->mutex);
    pthread_mutex_destroy(&buffer->mutex);

    free(buffer);
}

UInt8 srmBufferWrite(SRMBuffer *buffer, UInt32 stride, UInt32 dstX, UInt32 dstY, UInt32 dstWidth, UInt32 dstHeight, const void *pixels)
{
    if (!(buffer->caps & SRM_BUFFER_CAP_WRITE))
        goto fail;

    if (buffer->map)
    {
        const UInt8 *src = pixels;
        UInt8 *dst = &buffer->map[buffer->offsets[0] + dstY*buffer->strides[0] + dstX*buffer->pixelSize];

        pthread_mutex_lock(&buffer->mutex);
        buffer->sync.flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_WRITE;
        ioctl(buffer->fds[0], DMA_BUF_IOCTL_SYNC, &buffer->sync);

        for (UInt32 i = 0; i < dstHeight; i++)
        {
            memcpy(dst,
                   src,
                   buffer->pixelSize * dstWidth);

            dst += buffer->strides[0];
            src += stride;
        }

        buffer->sync.flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_WRITE;
        ioctl(buffer->fds[0], DMA_BUF_IOCTL_SYNC, &buffer->sync);
        pthread_mutex_unlock(&buffer->mutex);


        /* Disable EGL image recreation
        SRMListForeach(texIt, buffer->textures)
        {
            struct SRMBufferTexture *tex = srmListItemGetData(texIt);
            tex->updated = 1;
        }
        */

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

SRM_BUFFER_FORMAT srmBufferGetFormat(SRMBuffer *buffer)
{
    return buffer->format;
}

UInt32 srmBufferGetWidth(SRMBuffer *buffer)
{
    return buffer->width;
}

UInt32 srmBufferGetHeight(SRMBuffer *buffer)
{
    return buffer->height;
}
