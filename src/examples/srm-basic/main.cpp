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
static std::shared_ptr<RImage> squaresPre, squaresUnpre, mask, writeTest;

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

static const SRMConnectorInterface connIface
{
    .initializeGL = [](SRMConnector *conn, void *) {
        conn->repaint();
    },
    .paintGL = [](SRMConnector *conn, void *)
    {
        static float dx { 0.f }; dx += 0.1f;
        float phase { 0.5f * (std::sin(dx) + 1.f) };
        float phaseC { 0.5f * (std::cos(dx) + 1.f) };
        auto image { conn->currentImage() };
        auto surface { RSurface::WrapImage(image) };

        SkRegion clip;

        for (int y = 0; y < 60; y++)
            for (int x = 0; x < 70; x++)
                clip.op(SkIRect::MakeXYWH(x * 50, y * 50, 25, 25), SkRegion::kUnion_Op);

        if (dx < 10.f)
        {
            auto pass { surface->beginSKPass() };
            assert(pass.isValid());
            pass()->save();
            SkPaint p;
            p.setBlendMode(SkBlendMode::kSrcOver);
            pass()->clear(SK_ColorWHITE);
            pass()->clipRegion(clip);
            auto skImage { squaresPre->skImage() };
            for (int y = 0; y < 20; y++)
                for (int x = 0; x < 25; x++)
                    pass()->drawImage(skImage.get(), 1000 * phase + 150 * x, 150 * y, SkSamplingOptions(SkFilterMode::kLinear), &p);
            pass()->restore();
        }
        else
        {
            auto pass { surface->beginPass() };
            pass()->save();
            pass()->setColor(SkColorSetARGB(255, 200, 250, 250));
            pass()->clearSurface();
            pass()->setBlendMode(RBlendMode::SrcOver);
            RDrawImageInfo info {};
            info.image = squaresPre;
            info.src = SkRect::Make(info.image->size());
            info.dst = SkIRect::MakeSize(info.image->size());
            for (int y = 0; y < 20; y++)
            {
                for (int x = 0; x < 25; x++)
                {
                    info.dst.offsetTo(1000 * phase + 150 * x, 150 * y);
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
    setenv("CZ_SRM_LOG_LEVEL", "3", 0);
    setenv("CZ_REAM_LOG_LEVEL", "6", 0);
    setenv("CZ_REAM_EGL_LOG_LEVEL", "6", 0);
    setenv("CZ_REAM_GAPI", "GL", 0);


    std::thread([]{
        usleep(5000000);
        exit(0);
    }).detach();

    seat = libseat_open_seat(&libseatIface, NULL);

    auto srm { SRMCore::Make(&iface, nullptr) };

    if (!srm)
    {
        SRMLog(CZFatal, "[srm-basic] Failed to initialize SRM core.");
        return 1;
    }

    RDRMFormat fmt { DRM_FORMAT_ABGR8888 , {DRM_FORMAT_MOD_INVALID }};
    writeTest = RImage::Make({256, 256}, fmt);
    writeTest->writePixels({});

    std::vector<UInt32> pixels;
    pixels.resize(256 * 256);
    for (size_t i = 0; i < pixels.size(); i++)
        pixels[i] = 0x88FF0000;

    RPixelBufferInfo info {};
    info.size = {256, 256};
    info.format = DRM_FORMAT_ARGB8888;
    info.stride = 256 * 4;
    info.pixels = (UInt8*)pixels.data();
    info.alphaType = kUnpremul_SkAlphaType;
    squaresUnpre = RImage::MakeFromPixels(info, fmt);

    for (size_t i = 0; i < pixels.size(); i++)
        pixels[i] = 0x88880000;
    info.alphaType = kPremul_SkAlphaType;
    squaresPre = RImage::MakeFromPixels(info, fmt);

    squaresPre = RImage::LoadFile("/home/eduardo/.local/share/icons/WhiteSur/apps/scalable/accessories-image-viewer.svg", fmt, {256, 256});
    mask = RImage::LoadFile("/home/eduardo/.local/share/icons/WhiteSur/actions@2x/symbolic/color-tag-symbolic.svg", fmt, {512, 512});

    for (auto *dev : srm->devices())
    {
        for (auto *conn : dev->connectors())
        {
            if (conn->isConnected())
            {
                conn->initialize(&connIface, nullptr);
                goto skip;
            }
        }
    }
skip:
    //while (srm->dispatch(-1) >= 0) { };

    usleep(1000000);

    for (auto *dev : srm->devices())
        for (auto *conn : dev->connectors())
                conn->uninitialize();

    return 0;
}
