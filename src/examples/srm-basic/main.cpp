#include <SRMLog.h>
#include <SRMCore.h>
#include <fcntl.h>

extern "C" {
#include <libseat.h>
}

using namespace CZ;

static libseat *seat {};

static int openRestricted(const char *path, int flags, void *userData)
{
    CZ_UNUSED(userData);

    //int fd;
    //libseat_open_device(seat, path, &fd);
    return open(path, flags);
}

static void closeRestricted(int fd, void *userData)
{
    CZ_UNUSED(userData);
    close(fd);
}

static const SRMInterface iface
{
    .openRestricted = &openRestricted,
    .closeRestricted = &closeRestricted
};

static void enableSeat(libseat *seat, void *userdata)
{

}

static void disableSeat(libseat *seat, void *userdata)
{
    libseat_disable_seat(seat);
}

static libseat_seat_listener libseatIface
{
    .enable_seat = &enableSeat,
    .disable_seat = &disableSeat
};

int main(void)
{
    setenv("SRM_DEBUG", "4", 0);
    setenv("REAM_DEBUG", "4", 0);

    seat = libseat_open_seat(&libseatIface, NULL);

    auto srm { SRMCore::Make(&iface, nullptr) };

    if (!srm)
    {
        SRMFatal("[srm-basic] Failed to initialize SRM core.");
        return 1;
    }

    return 0;
}
