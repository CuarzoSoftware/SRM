#include <GLES2/gl2.h>
#include <SRMLog.h>
#include <SRMCore.h>
#include <SRMDevice.h>
#include <SRMConnector.h>

#include <RImage.h>
#include <RSurface.h>
#include <RPainter.h>
#include <RPass.h>

#include <SK/RSKPass.h>

#include <fcntl.h>
#include <thread>
#include <drm_fourcc.h>

extern "C" {
#include <libseat.h>
}

using namespace CZ;

static libseat *seat {};
static std::shared_ptr<RImage> svg;

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

static std::mutex mtx;

static const SRMConnectorInterface connIface
{
    .initializeGL = [](SRMConnector *conn, void *) {

        std::vector<UInt32> cursor;
        cursor.resize(64*64);
        for (size_t i = 0; i < cursor.size(); i++)
            cursor[i] = 0xFFFF00FF;
        conn->setCursor((UInt8*)cursor.data());
        conn->setCursorPos({200, 200});
        conn->repaint();
    },
    .paintGL = [](SRMConnector *conn, void *)
    {
        std::lock_guard g {mtx};
        static float dx { 0.f }; dx += 0.01f;
        float phase { 0.5f * (std::sin(dx) + 1.f) };
        auto image { conn->currentImage() };
        auto surface { RSurface::WrapImage(image) };
        SkRegion clip { SkIRect::MakeSize(image->size()) };

        conn->setCursorPos(SkIPoint::Make(phase * 2000, phase * 2000));
        /*
        for (int y = 0; y < 20; y++)
            for (int x = 0; x < 20; x++)
                clip.op(SkIRect::MakeXYWH(x * 400, y * 400, 300, 300), SkRegion::kUnion_Op);*/

        if (dx > 10.f)
        {
            auto pass { surface->beginSKPass(image->allocator()) };
            assert(pass.isValid());
            pass()->save();
            SkPaint p;
            p.setBlendMode(SkBlendMode::kSrcOver);
            pass()->clear(SK_ColorWHITE);
            pass()->clipRegion(clip);
            auto skImage { svg->skImage(image->allocator()) };
            for (int y = 0; y < 10; y++)
                for (int x = 0; x < 10; x++)
                    pass()->drawImage(skImage.get(), 1000 * phase + 250 * x, 250 * y, SkSamplingOptions(SkFilterMode::kLinear), &p);
            pass()->restore();
        }
        else
        {
            auto pass { surface->beginPass(image->allocator()) };
            pass()->save();
            pass()->setColor(SkColorSetARGB(255, 255, 255, 200));
            pass()->clearSurface();
            pass()->setBlendMode(RBlendMode::SrcOver);

            RDrawImageInfo info {};
            info.image = svg;
            info.src = SkRect::Make(info.image->size());
            info.dst = SkIRect::MakeSize(info.image->size());
            for (int y = 0; y < 10; y++)
            {
                for (int x = 0; x < 10; x++)
                {
                    info.dst.offsetTo(1000 * phase + 250 * x, 250 * y);
                    pass()->drawImage(info, &clip);
                }
            }
            pass()->restore();
        }

        conn->repaint();
    },
    .pageFlipped = [](SRMConnector *, void *) {},
    .resizeGL = [](SRMConnector *, void *) {},
    .uninitializeGL = [](SRMConnector *, void *)
    {
    }
};

int main(void)
{
    setenv("CZ_SRM_LOG_LEVEL", "4", 0);
    setenv("CZ_REAM_LOG_LEVEL", "4", 0);
    setenv("CZ_REAM_EGL_LOG_LEVEL", "4", 0);
    setenv("CZ_REAM_GAPI", "GL", 0);

    /*
    std::thread([]{
        usleep(10000000);
        abort();
    }).detach();*/

    seat = libseat_open_seat(&libseatIface, NULL);

    auto srm { SRMCore::Make(&iface, nullptr) };

    if (!srm)
    {
        SRMLog(CZFatal, "[srm-basic] Failed to initialize SRM core.");
        return 1;
    }

    RDRMFormat fmt {DRM_FORMAT_ARGB8888 , { DRM_FORMAT_MOD_LINEAR }};
    svg = RImage::LoadFile("/home/eduardo/Descargas/svgviewer-output.svg", fmt, {256, 256});

    for (auto *dev : srm->devices())
    {
        //if (dev->isBootVGA())
        //    continue;

        for (auto *conn : dev->connectors())
        {
            if (conn->isConnected())
            {
                conn->initialize(&connIface, nullptr);
            }
        }
    }

    //while (srm->dispatch(-1) >= 0) { };

    usleep(10000000);

    for (auto *dev : srm->devices())
        for (auto *conn : dev->connectors())
                conn->uninitialize();

    svg.reset();

    return 0;
}
