#ifndef SRMRENDERER_H
#define SRMRENDERER_H

#include <CZ/CZLogger.h>
#include <CZ/skia/core/SkPoint.h>

#include <CZ/SRM/SRMObject.h>

#include <CZ/Ream/RPresentationTime.h>

#include <mutex>
#include <memory>
#include <semaphore>
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
        /*
        Dumb,
        Texture,
        Raster*/
    };

    static std::unique_ptr<SRMRenderer> Make(SRMConnector *connector, const SRMConnectorInterface *iface, void *ifaceData) noexcept;
    SRMRenderer(SRMConnector *connector, const SRMConnectorInterface *iface, void *ifaceData) noexcept;
    ~SRMRenderer() noexcept;
    void initGamma() noexcept;
    bool initRenderThread() noexcept;

    bool rendInit() noexcept;
    bool rendInitCrtc() noexcept;

    bool initSwapchain() noexcept;
    bool initSwapchainSelf() noexcept;
    bool initSwapchainPrime() noexcept;

    bool flipPage() noexcept;
    bool flipPageSelf() noexcept;
    bool flipPagePrime() noexcept;


    bool rendWaitForRepaint() noexcept;
    bool rendWaitPageFlip(int iterLimit) noexcept;
    void presentFrame(UInt32 fb) noexcept;

    bool rendRender() noexcept;
    bool rendUpdateMode() noexcept;
    bool rendSuspend() noexcept;
    bool rendResume() noexcept;
    void rendUnit() noexcept;

    void updateAgeAndIndex() noexcept;

    int rendAtomicCommit(drmModeAtomicReqPtr req, UInt32 flags, void *data, bool forceRetry) noexcept;
    void rendAppendAtomicChanges(drmModeAtomicReqPtr req, bool clearFlags) noexcept;
    void atomicAppendPlane(drmModeAtomicReqPtr req) noexcept;

    SRMDevice *device() const noexcept;
    SRMConnector *conn;
    CZBitset<AtomicChange> atomicChanges;

    CZLogger log;
    CZLogger logAtomic;
    CZLogger logLegacy;

    Strategy strategy;

    struct AtomicCursor
    {
        gbm_bo *bo {};
        UInt32 fb {};
    } m_cursor[2] {};

    Int32 cursorI;
    SkIPoint cursorPos {};
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

    std::vector<drm_color_lut> m_gamma;
    UInt32 m_gammaBlobId {};
    drmEventContext drmEventCtx {};
    UInt32 lastFb {};
    UInt32 imageAge {};
    UInt32 imageI {};
    int fenceFd { -1 };
    std::binary_semaphore semaphore { 0 };
    std::mutex propsMutex; // Protect stuff like cursor and gamma updates
    std::unique_ptr<SkRegion> m_damage;
    RFormat m_currentFormat {};
    RPresentationTime presentationTime {};
    Int32 tearingLimit { -1 };
    std::shared_ptr<RImage> m_userScanoutBuffers[2] {};


    const SRMConnectorInterface *iface;
    void *ifaceData;
    std::vector<std::shared_ptr<RImage>> images;
    std::vector<std::shared_ptr<RSurface>> surfaces;
    std::vector<std::shared_ptr<RDRMFramebuffer>> fbs;

    std::vector<std::shared_ptr<RImage>> primeImages;
    std::vector<std::shared_ptr<RSurface>> primeSurfaces;
};

#endif // SRMRENDERER_H
