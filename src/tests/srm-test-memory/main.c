#include <SRMCore.h>
#include <SRMLog.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <malloc.h>

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

static unsigned long prev = 0;

static void printUsedMemory(const char *prefix)
{
    struct mallinfo2 info = mallinfo2();
    SRMLog("Memory used %s: %zu bytes. DIFF: %d", prefix, info.uordblks, abs((int)info.uordblks - (int)prev));
    prev = info.uordblks;
}

int main(void)
{
    setenv("SRM_DEBUG", "0", 1);

    SRMLogInit();

    SRMCore *core;
    core = srmCoreCreate(&srmInterface, NULL);
    printUsedMemory("before");
    srmCoreDestroy(core);
    printUsedMemory("after");

    return 0;
}
