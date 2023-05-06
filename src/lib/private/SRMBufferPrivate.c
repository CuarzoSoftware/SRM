#include <private/SRMBufferPrivate.h>
#include <private/SRMDevicePrivate.h>

#include <SRMLog.h>

#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <unistd.h>

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

    return prime_handle.fd;

    fail:
    SRMError("Error: Failed to get file descriptor for handle %u: %s", prime_handle.handle, strerror(errno));
    return -1;
}
