#include <SRMCore.h>
#include <SRMLog.h>
#include <SRMBuffer.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <malloc.h>
#include <time.h>

static int openRestricted(const char *path, int flags, void *userData)
{
    SRM_UNUSED(userData);
    SRMLog("OPEN DRM.");
    return open(path, flags);
}

static void closeRestricted(int fd, void *userData)
{
    SRM_UNUSED(userData);
    SRMLog("CLOSE DRM.");
    close(fd);
}

static SRMInterface srmInterface =
{
    .openRestricted = &openRestricted,
    .closeRestricted = &closeRestricted
};

int main(void)
{
    setenv("SRM_DEBUG", "0", 1);

    SRMCore *core = srmCoreCreate(&srmInterface, NULL);

    UInt8 *pixels = malloc(8048 * 8048 * 4);

    SRMBuffer *buffer = srmBufferCreateFromCPU(core, NULL, 8048, 8048, 8048 * 4, pixels, DRM_FORMAT_ABGR8888);

    clock_t start_time, end_time;
    double elapsed_time_ms;

    start_time = clock();

    for (Int32 i = 0; i < 500; i++)
    {
        srmBufferWrite(buffer, 8048 * 4, 0, 0, 8048, 8048, pixels);
        srmBufferWrite(buffer, 8048 * 4, 0, 0, 1024, 8048, pixels);
        srmBufferWrite(buffer, 8048 * 4, 0, 0, 8024, 1024, pixels);
    }

    end_time = clock();

    elapsed_time_ms = ((double)(end_time - start_time) * 1000.0) / CLOCKS_PER_SEC;

    SRMLog("Time elapsed: %f ms\n", elapsed_time_ms);

    free(pixels);
    srmBufferDestroy(buffer);
    srmCoreDestroy(core);
    return 0;
}
