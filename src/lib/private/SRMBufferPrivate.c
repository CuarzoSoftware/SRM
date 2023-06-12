#include <private/SRMBufferPrivate.h>
#include <private/SRMDevicePrivate.h>

#include <SRMCore.h>
#include <SRMList.h>
#include <SRMLog.h>

#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>


SRMBuffer *srmBufferCreate(SRMCore *core, SRMDevice *allocator)
{
    SRMBuffer *buffer = calloc(1, sizeof(SRMBuffer));
    pthread_mutex_init(&buffer->mutex, NULL);
    buffer->core = core;

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
    memset(&prime_handle, 0, sizeof(prime_handle));
    prime_handle.handle = gbm_bo_get_handle(bo).u32;
    prime_handle.flags = DRM_CLOEXEC | DRM_RDWR;
    prime_handle.fd = -1;

    if (ioctl(device->fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &prime_handle) != 0)
        goto fail;

    if (prime_handle.fd < 0)
        goto fail;

    // Set read and write permissions on the file descriptor
    if (fcntl(prime_handle.fd, F_SETFL, fcntl(prime_handle.fd, F_GETFL) | O_RDWR) == -1)
    {
        close(prime_handle.fd);
        goto fail;
    }

    SRMDebug("[%s] Got buffer DMA fd using DRM_IOCTL_PRIME_HANDLE_TO_FD.", device->name);
    return prime_handle.fd;

    fail:

    prime_handle.fd = gbm_bo_get_fd(bo);

    if (prime_handle.fd >= 0)
    {
        SRMDebug("[%s] Got buffer DMA fd using gbm_bo_get_fd().", device->name);
        return prime_handle.fd;
    }

    SRMError("Error: Failed to get file descriptor for handle %u: %s", prime_handle.handle, strerror(errno));
    return -1;
}
