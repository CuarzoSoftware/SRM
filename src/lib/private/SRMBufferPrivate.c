#include <private/SRMBufferPrivate.h>
#include <private/SRMDevicePrivate.h>

#include <SRMCore.h>
#include <SRMList.h>
#include <SRMLog.h>

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <xf86drm.h>

SRMBuffer *srmBufferCreate(SRMCore *core, SRMDevice *allocator)
{
    SRMBuffer *buffer = calloc(1, sizeof(SRMBuffer));
    pthread_mutex_init(&buffer->mutex, NULL);
    buffer->core = core;
    buffer->refCount = 1;
    buffer->fence = EGL_NO_SYNC_KHR;

    for (int i = 0; i < SRM_MAX_PLANES; i++)
        buffer->dma.fds[i] = -1;

    buffer->textures = srmListCreate();
    buffer->dma.modifiers[0] = DRM_FORMAT_MOD_INVALID;

    if (allocator)
        buffer->allocator = allocator;
    else
        buffer->allocator = srmCoreGetAllocatorDevice(core);

    return buffer;
}

Int32 srmBufferGetDMAFDFromBO(SRMDevice *device, struct gbm_bo *bo)
{
    struct drm_prime_handle prime_handle;
    prime_handle.handle = gbm_bo_get_handle(bo).u32;
    prime_handle.flags = DRM_CLOEXEC | DRM_RDWR;
    prime_handle.fd = -1;

    if (ioctl(device->fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &prime_handle) != 0)
        goto fail;

    if (prime_handle.fd < 0)
        goto fail;

    if (fcntl(prime_handle.fd, F_SETFL, fcntl(prime_handle.fd, F_GETFL) | O_RDWR) == -1)
    {
        close(prime_handle.fd);
        goto fail;
    }

    return prime_handle.fd;

    fail:

    prime_handle.fd = gbm_bo_get_fd(bo);

    if (prime_handle.fd >= 0)
        return prime_handle.fd;

    SRMError("Error: Failed to get file descriptor for handle %u: %s", prime_handle.handle, strerror(errno));
    return -1;
}

void *srmBufferMapFD(Int32 fd, size_t len, UInt32 *caps)
{
    void *map = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (map != MAP_FAILED)
    {
        *caps = SRM_BUFFER_CAP_READ | SRM_BUFFER_CAP_WRITE;
        return map;
    }

    map = mmap(NULL, len, PROT_WRITE, MAP_SHARED, fd, 0);

    if (map == MAP_FAILED)
    {
        *caps = 0;
        return NULL;
    }

    *caps = SRM_BUFFER_CAP_WRITE;
    return map;
}

struct gbm_bo *srmBufferCreateLinearBO(struct gbm_device *dev, UInt32 width, UInt32 height, UInt32 format)
{
    UInt64 mod = DRM_FORMAT_MOD_LINEAR;
    struct gbm_bo *bo;

    bo = gbm_bo_create_with_modifiers(dev, width, height, format, &mod, 1);

    if (bo)
        return bo;

    bo = gbm_bo_create(dev, width, height, format, GBM_BO_USE_RENDERING | GBM_BO_USE_LINEAR | GBM_BO_USE_SCANOUT);

    if (bo)
        return bo;

    return gbm_bo_create(dev, width, height, format, GBM_BO_USE_RENDERING | GBM_BO_USE_LINEAR);
}

SRMBuffer *srmBufferGetRef(SRMBuffer *buffer)
{
    pthread_mutex_lock(&buffer->mutex);
    buffer->refCount++;
    pthread_mutex_unlock(&buffer->mutex);
    return buffer;
}

struct gbm_surface *srmBufferCreateGBMSurface(struct gbm_device *dev, UInt32 width, UInt32 height, UInt32 format, UInt64 modifier, UInt32 flags)
{
    struct gbm_surface *surface = NULL;

    if (modifier == DRM_FORMAT_MOD_INVALID)
    {
        return gbm_surface_create(dev, width, height, format, flags);
    }
    else if (modifier == DRM_FORMAT_MOD_LINEAR)
    {
        surface = gbm_surface_create(dev, width, height, format, flags | GBM_BO_USE_LINEAR);

        if (surface) return surface;
    }

    surface = gbm_surface_create_with_modifiers2(
        dev, width, height, format, &modifier, 1, flags);

    if (surface) return surface;

    surface = gbm_surface_create_with_modifiers(
        dev, width, height, format, &modifier, 1);

    return surface;
}

struct gbm_bo *srmBufferCreateGBMBo(struct gbm_device *dev, UInt32 width, UInt32 height, UInt32 format, UInt64 modifier, UInt32 flags)
{
    struct gbm_bo *bo = NULL;

    if (modifier == DRM_FORMAT_MOD_INVALID)
        return gbm_bo_create(dev, width, height, format, flags);

    bo = gbm_bo_create_with_modifiers2(
        dev, width, height, format, &modifier, 1, flags);

    if (bo) return bo;

    bo = gbm_bo_create_with_modifiers(
        dev, width, height, format, &modifier, 1);

    if (bo) return bo;

    if (modifier == DRM_FORMAT_MOD_LINEAR)
        bo = gbm_bo_create(dev, width, height, format, flags | GBM_BO_USE_LINEAR);

    return bo;
}

void srmBufferFillParamsFromBO(SRMBuffer *buffer, struct gbm_bo *bo)
{
    buffer->dma.num_fds = gbm_bo_get_plane_count(bo);
    buffer->bpp = gbm_bo_get_bpp(bo);
    buffer->pixelSize = buffer->bpp/8;
    buffer->dma.format = gbm_bo_get_format(bo);
    buffer->dma.width = gbm_bo_get_width(bo);
    buffer->dma.height = gbm_bo_get_height(bo);

    for (UInt32 i = 0; i < buffer->dma.num_fds; i++)
    {
        buffer->dma.modifiers[i] = gbm_bo_get_modifier(bo);
        buffer->dma.strides[i] = gbm_bo_get_stride_for_plane(bo, i);
        buffer->dma.offsets[i] = gbm_bo_get_offset(bo, i);
    }
}

GLenum srmBufferGetTargetFromFormat(SRMDevice *device, UInt64 mod, UInt32 fmt)
{
    if (srmFormatIsInList(device->dmaExternalFormats, fmt, mod))
    {
        if (!device->glExtensions.OES_EGL_image_external)
        {
            SRMError("Buffer has GL_TEXTURE_EXTERNAL_OES target but OES_EGL_image_external is not available.");
            return GL_NONE;
        }

        return GL_TEXTURE_EXTERNAL_OES;
    }
    else if (srmFormatIsInList(device->dmaTextureFormats, fmt, mod))
    {
        if (!device->glExtensions.OES_EGL_image)
        {
            SRMError("Buffer has GL_TEXTURE_2D target but OES_EGL_image is not available.");
            return GL_NONE;
        }

        return GL_TEXTURE_2D;
    }

    return GL_NONE;
}

// A valid EGLDisplay and EGLContext must be current before calling this function
void srmBufferCreateSync(SRMBuffer *buffer)
{
    SRMEGLDeviceFunctions *f = &buffer->allocator->eglFunctions;

    if (!f->eglDupNativeFenceFDANDROID)
        goto fallback;

    if (buffer->fence != EGL_NO_SYNC_KHR)
    {
        f->eglDestroySyncKHR(buffer->allocator->eglDisplay, buffer->fence);
        buffer->fence = EGL_NO_SYNC_KHR;
    }

    static const EGLint attribs[] =
    {
        EGL_SYNC_NATIVE_FENCE_FD_ANDROID, EGL_NO_NATIVE_FENCE_FD_ANDROID,
        EGL_NONE,
    };

    buffer->fence = f->eglCreateSyncKHR(buffer->allocator->eglDisplay, EGL_SYNC_NATIVE_FENCE_ANDROID, attribs);

fallback:
    glFlush();
}

void srmBufferWaitSync(SRMBuffer *buffer)
{
    if (buffer->fence == EGL_NO_SYNC_KHR)
        return;

    if (buffer->allocator->eglFunctions.eglWaitSyncKHR(buffer->allocator->eglDisplay, buffer->fence, 0) != EGL_TRUE)
        SRMWarning("[%s] eglWaitSyncKHR failed.", buffer->allocator->shortName);

    buffer->allocator->eglFunctions.eglDestroySyncKHR(buffer->allocator->eglDisplay, buffer->fence);
    buffer->fence = EGL_NO_SYNC_KHR;
}

UInt8 srmBufferCreateRBFromBO(SRMCore *core, struct gbm_bo *bo, GLuint *outFB, GLuint *outRB, SRMBuffer **outWrapper)
{
    if (!bo)
    {
        SRMError("[SRMBuffer] srmBufferCreateRBFromBO: Invalid gbm_bo.");
        return 0;
    }

    *outWrapper = srmBufferCreateFromGBM(core, bo);

    if (!outWrapper)
    {
        SRMError("[SRMBuffer] srmBufferCreateRBFromBO: Failed to create SRMBuffer wrapper for gbm_bo.");
        return 0;
    }

    if (!(*outWrapper)->allocator->eglFunctions.glEGLImageTargetRenderbufferStorageOES)
    {
        SRMError("[SRMBuffer] srmBufferCreateRBFromBO: glEGLImageTargetRenderbufferStorageOES is not available.");
        goto failWrapper;
    }

    EGLImage image = srmBufferGetEGLImage((*outWrapper)->allocator, *outWrapper);

    if (image == EGL_NO_IMAGE)
    {
        SRMError("[SRMBuffer] srmBufferCreateRBFromBO: Failed to get EGLImage.");
        goto failWrapper;
    }

    glGenRenderbuffers(1, outRB);

    if (*outRB == 0)
    {
        SRMError("[SRMBuffer] srmBufferCreateRBFromBO: Failed to generate GL renderbuffer.");
        goto failWrapper;
    }

    glBindRenderbuffer(GL_RENDERBUFFER, *outRB);
    (*outWrapper)->allocator->eglFunctions.glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, image);
    glGenFramebuffers(1, outFB);

    if (*outFB == 0)
    {
        SRMError("[SRMBuffer] srmBufferCreateRBFromBO: Failed to generate GL framebuffer.");
        goto failGLRB;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, *outFB);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, *outRB);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        SRMError("[SRMBuffer] srmBufferCreateRBFromBO: Incomplete GL framebuffer.");
        goto failGLFB;
    }

    return 1;

failGLFB:
    glDeleteFramebuffers(1, outFB);
    *outFB = 0;
failGLRB:
    glDeleteRenderbuffers(1, outRB);
    *outRB = 0;
failWrapper:
    srmBufferDestroy(*outWrapper);
    *outWrapper = NULL;
    return 0;
}
