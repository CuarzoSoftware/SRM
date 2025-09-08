#include <GLES2/gl2.h>
#include <SRMLog.h>
#include <SRMCore.h>
#include <SRMDevice.h>
#include <SRMConnector.h>

#include <RImage.h>
#include <RSurface.h>
#include <RPainter.h>
#include <RPass.h>

#include <fcntl.h>
#include <thread>
#include <drm_fourcc.h>

extern "C" {
#include <libseat.h>
}

using namespace CZ;

static libseat *seat {};

static std::shared_ptr<RImage> imgNorm;
/*
static std::shared_ptr<RImage> img90;
static std::shared_ptr<RImage> img180;
static std::shared_ptr<RImage> img270;*/

static std::shared_ptr<RSurface> tmpSurf;


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

        tmpSurf = RSurface::Make({128, 128}, 2, true);
        SRMLog(CZInfo, "Realloc {}", tmpSurf->resize({98, 98}, 1));
        assert(tmpSurf);
        std::vector<UInt32> cursor;
        cursor.resize(64*64);
        for (size_t i = 0; i < cursor.size(); i++)
            cursor[i] = 0xFFFF00FF;
        conn->setCursor((UInt8*)cursor.data());
        conn->setCursorPos({200, 200});
        conn->enableVSync(false);
        conn->repaint();
    },
    .paintGL = [](SRMConnector *conn, void *)
    {
        std::lock_guard g {mtx};
        static float dx { 0.f }; dx += 0.01f;
        float phase { 0.5f * (std::sin(dx) + 1.f) };
        auto image { conn->currentImage() };
        auto surface { RSurface::WrapImage(image) };
        conn->setCursorPos({Int32(phase*1000), 200});

        surface->setGeometry({
            .viewport = SkRect::MakeWH(image->size().width(), image->size().height()),
            .dst = SkRect::MakeWH(image->size().width()/2, image->size().height()/2),
            .transform = CZTransform::Normal});


        SkRegion clip {  };

        conn->setCursorPos(SkIPoint::Make(phase * 2000, phase * 2000));

        auto pass { surface->beginPass() };
        auto *p { pass->getPainter() };

        RDrawImageInfo d {};
        d.image = imgNorm;
        d.dst.setXYWH(phase * 600, 600, 800, 800);
        d.src = SkRect::Make(imgNorm->size());
        p->drawImage(d);
        conn->repaint();

        SRMLog(CZInfo, "Paint event id: {}", conn->paintEventId());
    },
    .presented = [](SRMConnector *, const RPresentationTime &info, void *)
    {
        SRMLog(CZInfo, "Frame presented id: {} VSYNC: {}", info.paintEventId, info.flags.has(RPresentationTime::VSync));
    },
    .discarded = [](SRMConnector *, UInt64 paintEventId, void *)
    {
        SRMLog(CZError, "Frame discarded id: {}", paintEventId);
    },
    .resizeGL = [](SRMConnector *, void *) {},
    .uninitializeGL = [](SRMConnector *, void *)
    {
    }
};

int main(void)
{
    setenv("SRM_FORCE_LEGACY_API", "1", 0);
    setenv("CZ_SRM_LOG_LEVEL", "4", 0);
    setenv("CZ_REAM_LOG_LEVEL", "6", 0);
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

    imgNorm = RImage::LoadFile("/home/eduardo/tower.jpg", fmt);
      /*
    img90 = RImage::LoadFile("/usr/share/icons/Adwaita/scalable/devices/input-gaming.svg", fmt);
    img180 = RImage::LoadFile("/home/eduardo/Escritorio/atlas180.png", fmt);
    img270 = RImage::LoadFile("/home/eduardo/Escritorio/atlas270.png", fmt);*/

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
        /*
    img90.reset();
    img180.reset();
    img270.reset();*/
    tmpSurf.reset();

    return 0;
}
