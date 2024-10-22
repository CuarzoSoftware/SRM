#include <assert.h>
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

    for (int i = 0; i < SRM_MAX_PLANES; i++)
        buffer->fds[i] = -1;

    buffer->textures = srmListCreate();
    buffer->modifiers[0] = DRM_FORMAT_MOD_INVALID;

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
    struct gbm_bo *bo = NULL;

    bo = gbm_bo_create_with_modifiers2(
        dev, width, height, format, &mod, 1, GBM_BO_USE_RENDERING | GBM_BO_USE_LINEAR | GBM_BO_USE_SCANOUT);

    if (bo)
        return bo;

    bo = gbm_bo_create_with_modifiers2(
        dev, width, height, format, &mod, 1, GBM_BO_USE_RENDERING | GBM_BO_USE_LINEAR);

    if (bo)
        return bo;

    bo = gbm_bo_create_with_modifiers(
        dev, width, height, format, &mod, 1);

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
    {
        return gbm_bo_create(dev, width, height, format, flags);
    }
    else if (modifier == DRM_FORMAT_MOD_LINEAR)
    {
        bo = gbm_bo_create(dev, width, height, format, flags | GBM_BO_USE_LINEAR);

        if (bo) return bo;
    }

    bo = gbm_bo_create_with_modifiers2(
        dev, width, height, format, &modifier, 1, flags);

    if (bo) return bo;

    bo = gbm_bo_create_with_modifiers(
        dev, width, height, format, &modifier, 1);

    return bo;
}
