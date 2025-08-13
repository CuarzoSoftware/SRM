#ifndef SRMRENDERER_H
#define SRMRENDERER_H

#include <CZ/CZLogger.h>
#include <CZ/CZSpFd.h>
#include <CZ/skia/core/SkPoint.h>
#include <CZ/SRM/SRMPropertyBlob.h>
#include <CZ/Ream/RPresentationTime.h>

#include <future>
#include <mutex>
#include <memory>
#include <semaphore>
#include <thread>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>

class CZ::SRMRenderer final : SRMObject
{
public:
    enum AtomicChange
    {
        CHCursorVisibility = 1 << 0,
        CHCursorPosition   = 1 << 1,
        CHCursorBuffer     = 1 << 2,
        CHGammaLUT         = 1 << 3,
        CHContentType      = 1 << 4
    };

    enum Strategy
    {
        Self,
        Prime,
        Dumb,
        Copy,
        Raster
    };

    enum class CursorAPI
    {
        None,
        Atomic,
        Legacy
    };

    struct Commit
    {
        bool vSync;
    };

    static std::string_view StrategyString(Strategy strategy) noexcept
    {
        static const std::array<std::string_view, 5> str { "Self", "Prime", "Dumb", "Copy", "Raster" };
        return str[strategy];
    }

    static std::unique_ptr<SRMRenderer> Make(SRMConnector *connector, const SRMConnectorInterface *iface, void *ifaceData) noexcept;
    SRMRenderer(SRMConnector *connector, const SRMConnectorInterface *iface, void *ifaceData) noexcept;
    ~SRMRenderer() noexcept;

    bool init() noexcept;
    void unit() noexcept;
    void initContentType() noexcept;
    void initGamma() noexcept;
    void initCursor() noexcept;
    bool applyCrtcMode() noexcept;

    bool startRenderThread() noexcept;

    bool initSwapchain() noexcept;
    bool initSwapchainSelf() noexcept;
    bool initSwapchainPrime() noexcept;
    bool initSwapchainDumb() noexcept;

    bool flipPage() noexcept;
    bool flipPageSelf() noexcept;
    bool flipPagePrime() noexcept;
    bool flipPageDumb() noexcept;

    void waitForRepaintRequest() noexcept;
    bool waitPendingPageFlip(int iterLimit) noexcept;
    void commit(std::shared_ptr<RDRMFramebuffer> fb) noexcept;

    bool rendRender() noexcept;
    bool rendUpdateMode() noexcept;
    bool rendSuspend() noexcept;
    bool rendResume() noexcept;

    // Appends AtomicChange flags to req
    void atomicReqAppendChanges(std::shared_ptr<SRMAtomicRequest> req, std::shared_ptr<RDRMFramebuffer> fb) noexcept;
    void atomicReqAppendPrimaryPlane(std::shared_ptr<SRMAtomicRequest> req, std::shared_ptr<RDRMFramebuffer> fb) noexcept;

    void logInfo() noexcept;

    SRMDevice *device() const noexcept;
    std::thread::id threadId;

    SRMConnector *conn;
    CZBitset<AtomicChange> atomicChanges;

    CZLogger log;
    CZLogger logAtomic;
    CZLogger logLegacy;

    Strategy strategy;

    struct Cursor
    {
        std::shared_ptr<RImage> image;
        std::shared_ptr<RDRMFramebuffer> fb;
    } cursor[2] {};
    Int32 cursorI { 1 };
    SkIPoint cursorPos {};
    CursorAPI cursorAPI { CursorAPI::None };
    bool cursorVisible { false };

    SRMCrtc *crtc {};
    SRMEncoder *encoder {};
    SRMPlane *primaryPlane {};
    SRMPlane *cursorPlane {};

    bool currentVSync { true };
    bool pendingVSync { true };
    bool firstPageFlip { true };
    bool pendingPageFlip { false };
    bool pendingRepaint { false };
    bool rendering { false };
    bool isDead { false };

    CZSpFd inFence {};

    std::shared_ptr<const RGammaLUT> gammaLUT;
    std::shared_ptr<SRMPropertyBlob> gammaBlob;

    drmEventContext drmEventCtx {};
    std::binary_semaphore repaintSemaphore { 0 };
    std::recursive_mutex propsMutex; // Protect stuff like cursor and gamma updates
    std::unique_ptr<SkRegion> m_damage;
    RFormat m_currentFormat {};
    RPresentationTime presentationTime {};
    Int32 tearingLimit { -1 };
    std::shared_ptr<RImage> m_userScanoutBuffers[2] {};

    std::shared_ptr<RDRMFramebuffer> currentFb; // Currently being presented

    const SRMConnectorInterface *iface;
    void *ifaceData;

    struct
    {
        UInt32 i {};
        UInt32 age {};
        UInt32 n { 2 };

        void advanceAge() noexcept
        {
            if (++i == n) i = 0;
            if (age < n) age++;
        }

        void resetAge() noexcept
        {
            i = age = 0;
        }

        auto fb() const noexcept { return fbs[i]; }
        auto image() const noexcept { return images[i]; }
        auto surface() const noexcept { return surfaces[i]; }
        auto primeImage() const noexcept { return primeImages[i]; }
        auto primeSurface() const noexcept { return primeSurfaces[i]; }
        auto dumbBuffer() const noexcept { return dumbBuffers[i]; }

        std::vector<std::shared_ptr<RDRMFramebuffer>> fbs;
        std::vector<std::shared_ptr<RImage>> images;
        std::vector<std::shared_ptr<RSurface>> surfaces;
        std::vector<std::shared_ptr<RImage>> primeImages;
        std::vector<std::shared_ptr<RSurface>> primeSurfaces;
        std::vector<std::shared_ptr<RDumbBuffer>> dumbBuffers;
    } swapchain;

    std::optional<std::promise<bool>> unitPromise;
    std::promise<int> setModePromise;
};

#endif // SRMRENDERER_H
