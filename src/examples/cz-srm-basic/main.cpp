#include <CZ/SRM/SRMLog.h>
#include <CZ/SRM/SRMCore.h>
#include <CZ/SRM/SRMDevice.h>
#include <CZ/SRM/SRMConnector.h>

#include <CZ/Ream/RImage.h>
#include <CZ/Ream/RSurface.h>
#include <CZ/Ream/RPainter.h>
#include <CZ/Ream/RPass.h>

#include <CZ/Core/CZCore.h>
#include <CZ/Core/CZTimer.h>

#include <fcntl.h>
#include <drm_fourcc.h>

extern "C" {
#include <libseat.h>
}

using namespace CZ;

static bool Running { true };
static std::shared_ptr<CZCore> Core;
static std::shared_ptr<SRMCore> SRM;
static std::shared_ptr<CZEventSource> LibseatSrc;
static libseat *Seat {};
static std::mutex Mutex;
static std::unordered_map<int, int> SeatDevices; // FD => ID

static int OpenRestricted(const char *path, int flags, void *userData)
{
    CZ_UNUSED(userData)
    CZ_UNUSED(flags)

    int fd;
    int id { libseat_open_device(Seat, path, &fd) };

    if (id != -1)
        SeatDevices[fd] = id;

    return fd;
}

static void CloseRestricted(int fd, void *userData)
{
    CZ_UNUSED(userData)

    auto it { SeatDevices.find(fd) };

    if (it != SeatDevices.end())
    {
        libseat_close_device(Seat, it->second);
        SeatDevices.erase(it);
    }
}

static const SRMInterface iface
{
    .openRestricted = &OpenRestricted,
    .closeRestricted = &CloseRestricted
};

static void EnableSeat(libseat *, void *)
{
    if (SRM)
        SRM->resume();
}

static void DisableSeat(libseat *seat, void *)
{
    if (SRM)
        SRM->suspend();

    libseat_disable_seat(seat);
}

static libseat_seat_listener LibseatIface
{
    .enable_seat = &EnableSeat,
    .disable_seat = &DisableSeat
};

static const SRMConnectorInterface ConnIface
{
    .initialized = [](SRMConnector *conn, void *)
    {
        conn->repaint();
    },
    .paint = [](SRMConnector *conn, void *)
    {
        std::lock_guard lock { Mutex };
        auto image { conn->currentImage() };
        auto surface { RSurface::WrapImage(image) };
        auto pass { surface->beginPass() };
        auto *c { pass->getCanvas() };

        static bool colorToggle { true };

        if (conn->paintEventId() % 64 == 0)
            colorToggle = !colorToggle;

        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setBlendMode(SkBlendMode::kSrc);

        const auto center { SkPoint(image->size().width() / 2, image->size().height() / 2) };
        const auto rad { 256.f };
        const SkIRect circleBounds(
            center.x() - rad,
            center.y() - rad,
            center.x() + rad,
            center.y() + rad);

        paint.setColor(SK_ColorBLACK);
        c->drawIRect(circleBounds, paint);

        paint.setColor(colorToggle ? SK_ColorBLUE : SK_ColorRED);
        c->drawCircle(center, rad, paint);

        conn->damage.setRect(circleBounds);
        conn->repaint();
    },
    .presented = [](SRMConnector *, const CZPresentationTime &, void *) {},
    .discarded = [](SRMConnector *, UInt64 paintEventId, void *)
    {
        SRMLog(CZError, "Frame {} discarded", paintEventId);
    },
    .resized = [](SRMConnector *, void *) {},
    .uninitialized = [](SRMConnector *, void *) {}
};

int main(void)
{
    setenv("SRM_FORCE_LEGACY_API", "1", 0);
    setenv("CZ_SRM_LOG_LEVEL", "4", 0);
    setenv("CZ_REAM_LOG_LEVEL", "6", 0);
    setenv("CZ_REAM_EGL_LOG_LEVEL", "4", 0);

    Core = CZCore::GetOrMake();
    assert(Core && "Failed to create CZCore");
    Seat = libseat_open_seat(&LibseatIface, NULL);
    assert(Seat && "Failed to open seat");
    SRM = SRMCore::Make(&iface, nullptr);
    assert(SRM && "Failed to create SRMCore");

    LibseatSrc = CZEventSource::Make(libseat_get_fd(Seat), EPOLLIN, CZOwn::Borrow, [](auto, auto){
        libseat_dispatch(Seat, 0);
    });

    for (auto *dev : SRM->devices())
        for (auto *conn : dev->connectors())
            if (conn->isConnected())
                conn->initialize(&ConnIface, nullptr);

    CZTimer::OneShot(5000, [](auto){
        LibseatSrc.reset();
        SRM.reset();
        libseat_close_seat(Seat);
        Running = false;
    });

    while (Running) { Core->dispatch(-1); };

    return 0;
}
