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
static std::shared_ptr<RImage> imgNorm;
static std::shared_ptr<RImage> img90;
static std::shared_ptr<RImage> img180;
static std::shared_ptr<RImage> img270;


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
        surface->setGeometry(
            SkRect::MakeWH(image->size().width(), image->size().height()),
            SkRect::MakeWH(image->size().width()/2, image->size().height()/2),
            CZTransform::Normal);

        SkRegion clip {  };

        conn->setCursorPos(SkIPoint::Make(phase * 2000, phase * 2000));

        for (int y = 0; y < 20; y++)
            for (int x = 0; x < 20; x++)
                clip.op(SkIRect::MakeXYWH(x * 100, y * 100, 50, 50), SkRegion::kUnion_Op);

        if (dx > 10.f)
        {
            /*
            auto pass { surface->beginSKPass(image->allocator()) };
            assert(pass.isValid());
            pass()->save();
            SkPaint p;
            p.setBlendMode(SkBlendMode::kSrcOver);
            pass()->clear(SK_ColorWHITE);
            pass()->clipRegion(clip);
            auto skImage { svg->skImage(image->allocator()) };

            pass()->drawImage(skImage.get(), 1000 * phase + 250, 250, SkSamplingOptions(SkFilterMode::kLinear), &p);
            pass()->restore();*/
        }
        else
        {
            auto pass { surface->beginPass(image->allocator()) };
            pass()->save();
            pass()->setColor(SkColorSetARGB(255, 255, 255, 200));
            pass()->clearSurface();
            pass()->setBlendMode(RBlendMode::SrcOver);

            RDrawImageInfo info {};
            info.image = imgNorm;
            info.srcScale = 1.f;
            info.srcTransform = CZTransform::Normal;
            info.src = SkRect::MakeWH(info.image->size().width(), info.image->size().height());
            info.dst = SkIRect::MakeXYWH(100, 100, 800, 800);

            RDrawImageInfo mask {};
            mask.image = img90;
            mask.srcScale = 1.f;
            mask.srcTransform = CZTransform::Rotated90;
            mask.src = SkRect::MakeWH(mask.image->size().width(), mask.image->size().height());
            mask.dst = SkIRect::MakeXYWH(200, 200, 500, 500);

            pass()->drawImage(info, nullptr, &mask);

            /*
            info.image = imgNorm;
            info.srcTransform = CZTransform::Normal;
            info.src = SkRect::MakeXYWH(0, 0, 1024, 1024);
            info.dst = SkIRect::MakeXYWH(1800, 100, 1024, 1024);
            pass()->drawImage(info);

            info.image = img90;
            info.srcTransform = CZTransform::Rotated90;
            info.src = SkRect::MakeXYWH(100, 200, 40, 50);
            info.dst = SkIRect::MakeXYWH(700, 100, 512, 512);
            pass()->drawImage(info);

            info.image = img180;
            info.srcTransform = CZTransform::Rotated180;
            info.src = SkRect::MakeXYWH(100, 200, 40, 50);
            info.dst = SkIRect::MakeXYWH(100, 700, 512, 512);
            pass()->drawImage(info);

            info.image = img270;
            info.srcTransform = CZTransform::Rotated270;
            info.src = SkRect::MakeXYWH(100, 200, 40, 50);
            info.dst = SkIRect::MakeXYWH(700, 700, 512, 512);
            pass()->drawImage(info);
            */

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
    setenv("CZ_REAM_GAPI", "RS", 0);

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
    imgNorm = RImage::LoadFile("/home/eduardo/Escritorio/atlas.png", fmt);
    img90 = RImage::LoadFile("/usr/share/icons/Adwaita/scalable/devices/input-gaming.svg", fmt);
    img180 = RImage::LoadFile("/home/eduardo/Escritorio/atlas180.png", fmt);
    img270 = RImage::LoadFile("/home/eduardo/Escritorio/atlas270.png", fmt);

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

    usleep(4000000);

    for (auto *dev : srm->devices())
        for (auto *conn : dev->connectors())
                conn->uninitialize();

    imgNorm.reset();
    img90.reset();
    img180.reset();
    img270.reset();

    return 0;
}
