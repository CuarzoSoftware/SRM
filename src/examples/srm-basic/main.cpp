#include <GLES2/gl2.h>
#include <SRMLog.h>
#include <SRMCore.h>
#include <SRMDevice.h>
#include <SRMConnector.h>

#include <RImage.h>
#include <RSurface.h>
#include <RPainter.h>

#include <fcntl.h>
#include <thread>
#include <drm_fourcc.h>

extern "C" {
#include <libseat.h>
}

using namespace CZ;

static libseat *seat {};
static std::shared_ptr<RImage> squaresPre, squaresUnpre, mask;

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
        printf("--- initializeGL");
        conn->repaint();
    },
    .paintGL = [](SRMConnector *conn, void *)
    {
        static float dx { 0.f }; dx += 0.5f;
        auto image { conn->currentImage() };
        auto surface { RSurface::WrapImage(image) };
        auto *pass { surface->beginPass() };

        pass->setColor(SK_ColorWHITE);
        pass->clearSurface();
        pass->setBlendMode(RBlendMode::SrcOver);

        pass->setOpacity(0.5f);
        pass->setFactor({0.f, 1.f, 0.f, 1.f});

        RDrawImageInfo info {};
        info.image = squaresPre;
        info.magFilter = RImageFilter::Linear;
        info.minFilter = RImageFilter::Linear;
        info.src = SkRect::MakeWH(squaresPre->size().width(), squaresPre->size().height());
        info.dst = SkIRect::MakeWH(image->size().width(), image->size().height());
        auto clip { SkRegion(SkIRect::MakeXYWH(0, 0, 100 + dx * 10.f, 100)) };
        pass->drawImage(info, &clip);

        info.image = squaresUnpre;
        info.src = SkRect::MakeWH(squaresUnpre->size().width(), squaresUnpre->size().height());
        info.dst = SkIRect::MakeWH(image->size().width(), image->size().height());
        clip = SkRegion(SkIRect::MakeXYWH(0, 110, 100 + dx * 10.f, 100));
        pass->drawImage(info, &clip);

        info.image = squaresPre;
        info.src = SkRect::MakeWH(squaresPre->size().width(), squaresPre->size().height());
        info.dst = SkIRect::MakeXYWH(300, 300, 512, 512);

        RDrawImageInfo maskInfo { info };
        maskInfo.image = mask;
        maskInfo.src = SkRect::MakeWH(mask->size().width(), mask->size().height());
        maskInfo.dst = info.dst.makeInset(32, 32).makeOffset(dx, 0);
        pass->drawImage(info, nullptr, &maskInfo);

        pass->endPass();

        conn->repaint();
    },
    .pageFlipped = [](SRMConnector *, void *) {},
    .resizeGL = [](SRMConnector *, void *) {},
    .uninitializeGL = [](SRMConnector *, void *)
    {
        printf("--- uninitializeGL");
    }
};

int main(void)
{
    setenv("CZ_SRM_LOG_LEVEL", "5", 0);
    setenv("CZ_REAM_LOG_LEVEL", "6", 0);
    setenv("CZ_REAM_EGL_LOG_LEVEL", "5", 0);
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
    squaresUnpre = RImage::MakeFromPixels(info);

    for (size_t i = 0; i < pixels.size(); i++)
        pixels[i] = 0x88880000;
    info.alphaType = kPremul_SkAlphaType;
    squaresPre = RImage::MakeFromPixels(info);

    squaresPre = RImage::LoadFile("/home/eduardo/.local/share/icons/WhiteSur/apps/scalable/accessories-image-viewer.svg", {256, 256});
    mask = RImage::LoadFile("/home/eduardo/.local/share/icons/WhiteSur/actions@2x/symbolic/color-tag-symbolic.svg", {512, 512});

    for (auto *dev : srm->devices())
    {
        //SRMDebug("Dev %s connectors %zu", dev->nodeName().c_str(), dev->connectors().size());

        for (auto *conn : dev->connectors())
        {
            if (conn->isConnected())
            {
                //SRMDebug("Conn %s", conn->name().c_str());
                conn->initialize(&connIface, nullptr);
                goto skip;
            }
        }
    }
skip:
    while (srm->dispatch(-1) >= 0) { };

    return 0;
}
