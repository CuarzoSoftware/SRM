#include <CZ/SRM/SRMCore.h>
#include <CZ/SRM/SRMCrtc.h>
#include <CZ/SRM/SRMPlane.h>
#include <CZ/SRM/SRMEncoder.h>
#include <CZ/SRM/SRMDevice.h>
#include <CZ/SRM/SRMLog.h>
#include <CZ/SRM/SRMConnector.h>
#include <CZ/SRM/SRMConnectorMode.h>
#include <CZ/Utils/CZVectorUtils.h>
#include <CZ/Ream/GBM/RGBMBo.h>
#include <CZ/Ream/RImage.h>
#include <CZ/Ream/RGammaLUT.h>
#include <xf86drmMode.h>
#include <xf86drm.h>
#include <cstring>
#include <format>

extern "C" {
#include <libdisplay-info/info.h>
}

using namespace CZ;

SRMConnector *SRMConnector::Make(UInt32 id, SRMDevice *device) noexcept
{
    drmModeConnectorPtr res { drmModeGetConnector(device->fd(), id) };

    if (!res)
    {
        device->log(CZError, CZLN, "Failed to get drmModeConnectorPtr for SRMConnector {}", id);
        return {};
    }

    std::unique_ptr<SRMConnector> obj { new SRMConnector(id, device) };
    obj->m_name = std::format("{}-{}", TypeString(res->connector_type), res->connector_type_id);
    obj->log = device->log.newWithContext(obj->name());

    obj->updateProperties(res);
    obj->updateNames(res);
    obj->updateEncoders(res);
    obj->updateModes(res);
    obj->setContentType(RContentType::Graphics);
    drmModeFreeConnector(res);
    return obj.release();

    drmModeFreeConnector(res);
    device->log(CZError, CZLN, "Failed to create SRMConnector {}", id);
    return {};
}

bool SRMConnector::updateProperties(drmModeConnectorPtr res) noexcept
{
    m_subPixel = (RSubPixel)res->subpixel;
    m_mmSize.set(res->mmWidth, res->mmHeight);
    m_isConnected = res->connection == DRM_MODE_CONNECTED;
    m_type = res->connector_type;
    m_nameId = res->connector_type_id;

    memset(&m_propIDs, 0, sizeof(m_propIDs));

    if (!isConnected())
        return false;

    drmModeObjectPropertiesPtr props { drmModeObjectGetProperties(device()->fd(), id(), DRM_MODE_OBJECT_CONNECTOR) };

    if (!props)
    {
        log(CZError, CZLN, "Failed to get drmModeObjectPropertiesPtr");
        return false;
    }

    for (UInt32 i = 0; i < props->count_props; i++)
    {
        drmModePropertyPtr prop { drmModeGetProperty(device()->fd(), props->props[i]) };

        if (!prop)
        {
            log(CZWarning, CZLN, "Failed to get drmModePropertyPtr {}", props->props[i]);
            continue;
        }

        if (strcmp(prop->name, "CRTC_ID") == 0)
            m_propIDs.CRTC_ID = prop->prop_id;
        else if (strcmp(prop->name, "DPMS") == 0)
            m_propIDs.DPMS = prop->prop_id;
        else if (strcmp(prop->name, "EDID") == 0)
            m_propIDs.EDID = prop->prop_id;
        else if (strcmp(prop->name, "PATH") == 0)
            m_propIDs.PATH = prop->prop_id;
        else if (strcmp(prop->name, "link-status") == 0)
            m_propIDs.link_status = prop->prop_id;
        else if (strcmp(prop->name, "non-desktop") == 0)
        {
            m_propIDs.non_desktop = prop->prop_id;
            m_nonDesktop = props->prop_values[i] == 1;
        }
        else if (strcmp(prop->name, "content type") == 0)
            m_propIDs.content_type = prop->prop_id;
        else if (strcmp(prop->name, "panel orientation") == 0)
            m_propIDs.panel_orientation = prop->prop_id;
        else if (strcmp(prop->name, "subconnector") == 0)
            m_propIDs.subconnector = prop->prop_id;
        else if (strcmp(prop->name, "vrr_capable") == 0)
            m_propIDs.vrr_capable = prop->prop_id;

        drmModeFreeProperty(prop);
    }

    drmModeFreeObjectProperties(props);

    return true;
}

bool SRMConnector::updateNames(drmModeConnectorPtr res) noexcept
{
    m_serial.clear();
    m_make = m_model = "Unknown";

    if (!isConnected())
        return false;

    drmModePropertyBlobPtr blob {};

    for (int i = 0; i < res->count_props; i++)
    {
        drmModePropertyPtr prop { drmModeGetProperty(device()->fd(), res->props[i]) };

        if (!prop)
            continue;

        if (strcmp(prop->name, "EDID") == 0)
        {
            blob = drmModeGetPropertyBlob(device()->fd(), res->prop_values[i]);
            drmModeFreeProperty(prop);
            break;
        }

        drmModeFreeProperty(prop);
    }

    if (!blob)
    {
        log(CZWarning, CZLN, "Could not get EDID property blob: {}", strerror(errno));
        return false;
    }

    struct di_info *info = di_info_parse_edid(blob->data, blob->length);

    if (!info)
    {
        log(CZWarning, CZLN, "[%s] Could not parse EDID info: {}", strerror(errno));
        drmModeFreePropertyBlob(blob);
        return false;
    }

    char *str { di_info_get_make(info) };

    if (str)
    {
        m_make = str;
        free(str);
    }

    str = di_info_get_model(info);

    if (str)
    {
        m_model = str;
        free(str);
    }

    str = di_info_get_serial(info);

    if (str)
    {
        m_serial = str;
        free(str);
    }

    di_info_destroy(info);
    drmModeFreePropertyBlob(blob);
    return 1;
}

bool SRMConnector::updateEncoders(drmModeConnectorPtr res) noexcept
{
    m_encoders.clear();

    if (!isConnected())
        return false;

    for (int i = 0; i < res->count_encoders; i++)
    {
        for (SRMEncoder *encoder : device()->encoders())
        {
            if (encoder->id() == res->encoders[i])
            {
                m_encoders.emplace_back(encoder);
                break;
            }
        }
    }

    return true;
}

bool SRMConnector::updateModes(drmModeConnectorPtr res) noexcept
{
    destroyModes();

    if (!isConnected())
        return false;

    for (int i = 0; i < res->count_modes; i++)
        m_modes.emplace_back(new SRMConnectorMode(this, &res->modes[i]));

    m_preferredMode = m_currentMode = findPreferredMode();
    return true;
}

bool SRMConnector::unlockRenderer(bool repaint) noexcept
{
    if (!m_rend)
        return false;

    m_rend->pendingRepaint |= repaint;
    m_rend->repaintSemaphore.release();
    return true;
}

SRMConnectorMode *SRMConnector::findPreferredMode() const noexcept
{
    SRMConnectorMode *preferredMode {};

    Int64 greatestSize { -1 };

    for (SRMConnectorMode *mode : modes())
    {
        if (mode->info().type & DRM_MODE_TYPE_PREFERRED)
        {
            preferredMode = mode;
            break;
        }

        /* If no mode has the preferred flag, look for the one with
         * greatest dimensions */
        const Int64 currentSize { mode->size().area() };

        if (currentSize > greatestSize)
        {
            greatestSize = currentSize;
            preferredMode = mode;
        }
    }

    return preferredMode;
}

bool SRMConnector::findConfiguration(SRMEncoder **bestEncoder, SRMCrtc **bestCrtc, SRMPlane **bestPrimaryPlane, SRMPlane **bestCursorPlane) noexcept
{
    int bestScore { 0 };
    *bestEncoder = nullptr;
    *bestCrtc = nullptr;
    *bestPrimaryPlane = nullptr;
    *bestCursorPlane = nullptr;

    for (auto *encoder : encoders())
    {
        for (auto *crtc : encoder->crtcs())
        {
            if (crtc->currentConnector())
                continue;

            int score { 0 };
            SRMPlane *primaryPlane { nullptr };
            SRMPlane *cursorPlane { nullptr };

            for (auto *plane : device()->planes())
            {
                if (plane->type() == SRMPlane::Overlay)
                    continue;

                for (auto *planeCrtc : plane->crtcs())
                {
                    if (planeCrtc->currentConnector())
                        continue;

                    if (planeCrtc->id() == crtc->id())
                    {
                        if (plane->type() == SRMPlane::Primary)
                        {
                            primaryPlane = plane;
                            break;
                        }
                        else if (plane->type() == SRMPlane::Cursor)
                        {
                            cursorPlane = plane;
                            break;
                        }
                    }
                }
            }

            if (!primaryPlane)
                continue;

            score += 100;

            if (cursorPlane)
                score += 50;

            if (score > bestScore)
            {
                bestScore = score;
                *bestEncoder = encoder;
                *bestCrtc = crtc;
                *bestPrimaryPlane = primaryPlane;
                *bestCursorPlane = cursorPlane;
            }
        }
    }

    return *bestEncoder && *bestCrtc && *bestPrimaryPlane;
}

void SRMConnector::destroyModes() noexcept
{
    CZVectorUtils::DeleteAndPopBackAll(m_modes);
    m_currentMode = m_preferredMode = nullptr;
}

SRMConnector::~SRMConnector() noexcept
{
    uninitialize();
    destroyModes();
}

std::string_view SRMConnector::TypeString(UInt32 type) noexcept
{
    switch (type)
    {
    case DRM_MODE_CONNECTOR_Unknown:     return "unknown";
    case DRM_MODE_CONNECTOR_VGA:         return "VGA";
    case DRM_MODE_CONNECTOR_DVII:        return "DVI-I";
    case DRM_MODE_CONNECTOR_DVID:        return "DVI-D";
    case DRM_MODE_CONNECTOR_DVIA:        return "DVI-A";
    case DRM_MODE_CONNECTOR_Composite:   return "composite";
    case DRM_MODE_CONNECTOR_SVIDEO:      return "S-VIDEO";
    case DRM_MODE_CONNECTOR_LVDS:        return "LVDS";
    case DRM_MODE_CONNECTOR_Component:   return "component";
    case DRM_MODE_CONNECTOR_9PinDIN:     return "DIN";
    case DRM_MODE_CONNECTOR_DisplayPort: return "DisplayPort";
    case DRM_MODE_CONNECTOR_HDMIA:       return "HDMI-A";
    case DRM_MODE_CONNECTOR_HDMIB:       return "HDMI-B";
    case DRM_MODE_CONNECTOR_TV:          return "TV";
    case DRM_MODE_CONNECTOR_eDP:         return "eDP";
    case DRM_MODE_CONNECTOR_VIRTUAL:     return "virtual";
    case DRM_MODE_CONNECTOR_DSI:         return "DSI";
    case DRM_MODE_CONNECTOR_DPI:         return "DPI";
    case DRM_MODE_CONNECTOR_WRITEBACK:   return "writeback";
    default:                             return "unknown";
    }
}

SRMEncoder *SRMConnector::currentEncoder() const noexcept
{
    return m_rend ? m_rend->encoder : nullptr;
}

int SRMConnector::setMode(SRMConnectorMode *mode) noexcept
{
    if (!mode)
    {
        log(CZError, CZLN, "Invalid connector mode (nullptr)");
        return 0;
    }

    if (mode->connector() != this)
    {
        log(CZError, CZLN, "Invalid connector mode {} (belongs to another connector)", *mode);
        return 0;
    }

    if (mode == currentMode())
    {
        log(CZTrace, CZLN, "Mode {} already assigned", *mode);
        return 1;
    }

    if (!m_rend)
    {
        log(CZTrace, CZLN, "Mode {} successfully applied (while the connector is uninitialized)", *mode);
        m_currentMode = mode;
        return 1;
    }

    if (m_rend->isDead)
    {
        log(CZError, CZLN, "Failed to set mode {} (the connector is in a dead state and can only be uninitialized)", *mode);
        return -1;
    }

    if (std::this_thread::get_id() == m_rend->threadId)
    {
        log(CZError, CZLN, "Changing a connector's mode from its render thread is not allowed");
        return 0;
    }

    m_rend->setModePromise = std::promise<int>();
    auto future { m_rend->setModePromise.get_future() };
    m_rend->pendingMode = mode;
    unlockRenderer(false);
    const auto ret { future.get() };

    if (ret == 1)
        log(CZTrace, CZLN, "Mode {} successfully applied", *mode);
    else
        log(CZError, CZLN, "Failed to set mode {}", *mode);

    return ret;
}

SRMCrtc *SRMConnector::currentCrtc() const noexcept
{
    return m_rend ? m_rend->crtc : nullptr;
}

SRMPlane *SRMConnector::currentPrimaryPlane() const noexcept
{
    return m_rend ? m_rend->primaryPlane : nullptr;
}

SRMPlane *SRMConnector::currentCursorPlane() const noexcept
{
    return m_rend ? m_rend->cursorPlane : nullptr;
}

bool SRMConnector::hasCursor() const noexcept
{
    if (m_rend)
        return m_rend->cursorAPI != SRMRenderer::CursorAPI::None;

    return false;
}

bool SRMConnector::setCursor(UInt8 *pixels) noexcept
{
    if (!hasCursor())
        return false;

    std::lock_guard<std::recursive_mutex> lock { m_rend->propsMutex };

    if (pixels)
    {
        const auto i { 1 - m_rend->cursorI };
        auto bo { m_rend->cursor[i].bo };

        if (gbm_bo_write(bo->bo(), pixels, 64 * 64 * 4) != 0)
            return false;

        m_rend->cursorVisible = true;

        if (m_rend->cursorAPI == SRMRenderer::CursorAPI::Atomic)
        {
            m_rend->atomicChanges.add(SRMRenderer::CHCursorVisibility | SRMRenderer::CHCursorBuffer);
            unlockRenderer(false);
        }
        else
        {
            drmModeSetCursor(device()->fd(), m_rend->crtc->id(), bo->planeHandle(0).u32, bo->dmaInfo().width, bo->dmaInfo().height);
            m_rend->cursorI = i;
        }
    }
    else
    {
        if (!m_rend->cursorVisible)
            return true;

        m_rend->cursorVisible = false;

        if (m_rend->cursorAPI == SRMRenderer::CursorAPI::Atomic)
        {
            m_rend->atomicChanges.add(SRMRenderer::CHCursorVisibility);
            unlockRenderer(false);
        }
        else
            drmModeSetCursor(device()->fd(), m_rend->crtc->id(), 0, 0, 0);
    }

    return true;
}

bool SRMConnector::setCursorPos(SkIPoint pos) noexcept
{
    if (!hasCursor())
        return false;

    if (m_rend->cursorPos.x() == pos.x() && m_rend->cursorPos.y() == pos.y())
        return true;

    std::lock_guard<std::recursive_mutex> lock { m_rend->propsMutex };

    m_rend->cursorPos = pos;

    if (m_rend->cursorAPI == SRMRenderer::CursorAPI::Atomic)
    {
        m_rend->atomicChanges.add(SRMRenderer::CHCursorPosition);
        unlockRenderer(false);
    }
    else
        drmModeMoveCursor(device()->fd(), m_rend->crtc->id(), pos.x(), pos.y());

    return true;
}

bool SRMConnector::initialize(const SRMConnectorInterface *iface, void *data) noexcept
{
    if (isInitialized() || !isConnected())
        return false;

    m_rend = SRMRenderer::Make(this, iface, data);

    if (!m_rend)
        return false;

    const bool ret  { m_rend->startRenderThread() };
    if (!ret) m_rend.reset();
    return ret;
}

bool SRMConnector::repaint() noexcept
{
    return unlockRenderer(true);
}

bool SRMConnector::uninitialize() noexcept
{
    if (!isInitialized())
        return false;

    if (std::this_thread::get_id() == m_rend->threadId)
    {
        log(CZError, CZLN, "Calling uninitialize() from the connector's rendering thread is not allowed");
        return false;
    }

    m_rend->unitPromise = std::promise<bool>();
    auto future { m_rend->unitPromise->get_future() };
    unlockRenderer(false);
    assert(future.get());
    m_rend.reset();
    return true;
}

const std::vector<std::shared_ptr<RImage>> &SRMConnector::images() const noexcept
{
    static const std::vector<std::shared_ptr<RImage>> dummy;
    return m_rend ? m_rend->swapchain.images : dummy;
}

UInt32 SRMConnector::imageIndex() const noexcept
{
    return m_rend ? m_rend->swapchain.i : 0;
}

UInt32 SRMConnector::imageAge() const noexcept
{
    return m_rend ? m_rend->swapchain.age : 0;
}

std::shared_ptr<RImage> SRMConnector::currentImage() const noexcept
{
    return m_rend ? m_rend->swapchain.image() : nullptr;
}

UInt64 SRMConnector::gammaSize() const noexcept
{
    return m_rend ? m_rend->crtc->gammaSize() : 0;
}

std::shared_ptr<const RGammaLUT> SRMConnector::gammaLUT() const noexcept
{
    return m_rend ? m_rend->gammaLUT : nullptr;
}

bool SRMConnector::canDisableVSync() const noexcept
{
    if (device()->clientCaps().Atomic)
        return device()->caps().AtomicAsyncPageFlip;
    return device()->caps().AsyncPageFlip;
}

bool SRMConnector::enableVSync(bool enabled) noexcept
{
    if (enabled)
    {
        m_vsync = true;
        return true;
    }

    if (!canDisableVSync())
        return false;

    m_vsync = false;
    return true;
}

bool SRMConnector::setGammaLUT(std::shared_ptr<const RGammaLUT> gammaLUT) noexcept
{
    if (!m_rend)
    {
        log(CZError, CZLN, "Failed to set gamma LUT (uninitialized connector)");
        return false;
    }

    if (gammaSize() == 0)
    {
        log(CZError, CZLN, "Failed to set gamma LUT (unsupported)");
        return false;
    }

    if (gammaLUT)
    {
        if (gammaLUT->size() != gammaSize())
        {
            log(CZError, CZLN, "Invalid gamma LUT size {} != {}", gammaLUT->size(), gammaSize());
            return false;
        }
    }
    else
        gammaLUT = RGammaLUT::MakeFilled(gammaSize(), 1, 1, 1);


    if (device()->clientCaps().Atomic)
    {
        std::lock_guard<std::recursive_mutex> lock { m_rend->propsMutex };

        const auto R { gammaLUT->red() };
        const auto G { gammaLUT->green() };
        const auto B { gammaLUT->blue() };

        std::vector<drm_color_lut> table;
        table.resize(gammaLUT->size());

        for (size_t i = 0; i < gammaLUT->size(); i++)
        {
            table[i].red = R[i];
            table[i].green = G[i];
            table[i].blue = B[i];
        }

        m_rend->gammaBlob = SRMPropertyBlob::Make(device(), table.data(), table.size() * sizeof(*table.data()));

        if (!m_rend->gammaBlob)
        {
            log(CZError, CZLN, "Failed to create property blob for gamma LUT");
            return false;
        }

        m_rend->atomicChanges |= SRMRenderer::CHGammaLUT;
        m_rend->gammaLUT = gammaLUT;
        unlockRenderer(false);
    }
    else
    {
        if (drmModeCrtcSetGamma(device()->fd(), m_rend->crtc->id(), gammaLUT->size(), gammaLUT->red().data(), gammaLUT->green().data(), gammaLUT->blue().data()))
        {
            log(CZError, CZLN, "Failed to set gamma LUT (drmModeCrtcSetGamma)");
            return false;
        }

        m_rend->gammaLUT = gammaLUT;
    }

    return true;
}

void SRMConnector::setContentType(RContentType type, bool force) noexcept
{
    if (!m_propIDs.content_type || !m_rend)
    {
        m_contentType = type;
        return;
    }

    if (!force && m_contentType == type)
        return;

    std::lock_guard<std::recursive_mutex> lock { m_rend->propsMutex };

    if (device()->clientCaps().Atomic)
    {
        m_rend->atomicChanges |= SRMRenderer::CHContentType;
        unlockRenderer(false);
    }
    else
        drmModeConnectorSetProperty(device()->fd(), id(), m_propIDs.content_type, static_cast<UInt64>(type));
}


#if 1 == 2

UInt8 srmConnectorHasBufferDamageSupport(SRMConnector *connector)
{
    if (connector->currentPrimaryPlane && connector->currentPrimaryPlane->propIDs.FB_DAMAGE_CLIPS)
        return 1;

    SRM_RENDER_MODE renderMode = srmDeviceGetRenderMode(connector->device);

    if (renderMode == SRM_RENDER_MODE_ITSELF)
        return 0;

    // PRIME, DUMB and CPU modes always support damage
    return 1;
}

UInt8 srmConnectorSetBufferDamage(SRMConnector *connector, SRMRect *rects, Int32 n)
{
    if (!connector->currentPrimaryPlane || !srmConnectorHasBufferDamageSupport(connector))
        return 0;

    if (connector->damageBoxes)
    {
        free(connector->damageBoxes);
        connector->damageBoxes = NULL;
        connector->damageBoxesCount = 0;
    }

    if (n == 0)
        return 1;

    if (n < 0)
        return 0;

    ssize_t size = sizeof(SRMBox)*n;
    connector->damageBoxes = malloc(size);

    for (Int32 i = 0; i < n; i++)
    {
        connector->damageBoxes[i].x1 = rects[i].x;
        connector->damageBoxes[i].y1 = rects[i].y;
        connector->damageBoxes[i].x2 = rects[i].x + rects[i].width;
        connector->damageBoxes[i].y2 = rects[i].y + rects[i].height;
    }
    connector->damageBoxesCount = n;
    return 1;
}

UInt8 srmConnectorSetBufferDamageBoxes(SRMConnector *connector, SRMBox *boxes, Int32 n)
{
    if (!connector->currentPrimaryPlane || !srmConnectorHasBufferDamageSupport(connector))
        return 0;

    if (connector->damageBoxes)
    {
        free(connector->damageBoxes);
        connector->damageBoxes = NULL;
        connector->damageBoxesCount = 0;
    }

    if (n == 0)
        return 1;

    if (n < 0)
        return 0;

    ssize_t size = sizeof(SRMBox)*n;
    connector->damageBoxes = malloc(size);
    memcpy(connector->damageBoxes, boxes, size);
    connector->damageBoxesCount = n;
    return 1;
}

SRM_CONNECTOR_SUBPIXEL srmConnectorGetSubPixel(SRMConnector *connector)
{
    return connector->subpixel;
}

UInt64 srmConnectorGetGammaSize(SRMConnector *connector)
{
    if (connector->currentCrtc)
        return srmCrtcGetGammaSize(connector->currentCrtc);

    return 0;
}

UInt8 srmConnectorSetGamma(SRMConnector *connector, UInt16 *table)
{
    if (!connector->currentCrtc)
    {
        SRMError("Failed to set gamma for connector %d. Gamma cannot be set on an uninitialized connector.",
            connector->id);
        return 0;
    }

    UInt64 gammaSize = srmCrtcGetGammaSize(connector->currentCrtc);

    if (gammaSize == 0)
    {
        SRMError("Failed to set gamma for connector %d. Gamma size is 0, indicating that the driver does not support gamma correction.",
            connector->id);
        return 0;
    }

    if (connector->device->clientCapAtomic && connector->currentCrtc->propIDs.GAMMA_LUT_SIZE)
    {
        pthread_mutex_lock(&connector->propsMutex);
        UInt16 *R = table;
        UInt16 *G = table + gammaSize;
        UInt16 *B = G + gammaSize;

        for (UInt64 i = 0; i < gammaSize; i++)
        {
            connector->gamma[i].red = R[i];
            connector->gamma[i].green = G[i];
            connector->gamma[i].blue = B[i];
        }
        
        connector->atomicChanges |= SRM_ATOMIC_CHANGE_GAMMA_LUT;
        pthread_mutex_unlock(&connector->propsMutex);
        pthread_cond_signal(&connector->repaintCond);
        return 1;
    }
    else
    {
        memcpy(connector->gamma, table, sizeof(UInt16) * gammaSize * 3);
        if (drmModeCrtcSetGamma(connector->device->fd,
            connector->currentCrtc->id,
            (UInt32)gammaSize,
            table,
            table + gammaSize,
            table + gammaSize + gammaSize))
        {
            SRMError("Failed to set gamma for connector %d using legacy API drmModeCrtcSetGamma().",
            connector->id);
            return 0;
        }
    }

    return 1;
}

UInt8 srmConnectorHasVSyncControlSupport(SRMConnector *connector)
{
    return (connector->device->capAsyncPageFlip && !connector->device->clientCapAtomic) ||
           (connector->device->capAtomicAsyncPageFlip && connector->device->clientCapAtomic);
}

UInt8 srmConnectorIsVSyncEnabled(SRMConnector *connector)
{
    return connector->pendingVSync;
}

UInt8 srmConnectorEnableVSync(SRMConnector *connector, UInt8 enabled)
{
    if (!enabled && !srmConnectorHasVSyncControlSupport(connector))
        return 0;

    connector->pendingVSync = enabled;
    return 1;
}

void srmConnectorSetRefreshRateLimit(SRMConnector *connector, Int32 hz)
{
    connector->maxRefreshRate = hz;
}

Int32 srmConnectorGetRefreshRateLimit(SRMConnector *connector)
{
    return connector->maxRefreshRate;
}

clockid_t srmConnectorGetPresentationClock(SRMConnector *connector)
{
    return connector->device->clock;
}

const SRMPresentationTime *srmConnectorGetPresentationTime(SRMConnector *connector)
{
    return &connector->presentationTime;
}

void srmConnectorSetContentType(SRMConnector *connector, SRM_CONNECTOR_CONTENT_TYPE contentType)
{
    if (!connector->propIDs.content_type)
    {
        connector->contentType = contentType;
        return;
    }

    pthread_mutex_lock(&connector->propsMutex);

    if (connector->contentType == contentType)
    {
        pthread_mutex_unlock(&connector->propsMutex);
        return;
    }

    connector->contentType = contentType;

    if (connector->device->clientCapAtomic)
    {
        connector->atomicChanges |= SRM_ATOMIC_CHANGE_CONTENT_TYPE;

        pthread_mutex_unlock(&connector->propsMutex);
        pthread_cond_signal(&connector->repaintCond);
    }
    else
    {
        drmModeConnectorSetProperty(connector->device->fd,
                                    connector->id,
                                    connector->propIDs.content_type,
                                    connector->contentType);
        pthread_mutex_unlock(&connector->propsMutex);
    }
}

SRM_CONNECTOR_CONTENT_TYPE srmConnectorGetContentType(SRMConnector *connector)
{
    return connector->contentType;
}

UInt8 srmConnectorSetCustomScanoutBuffer(SRMConnector *connector, SRMBuffer *buffer)
{
    if (!connector->inPaintGL || connector->device->core->customBufferScanoutIsDisabled || connector->pendingModeSetting)
        return 0;

    if (buffer == connector->userScanoutBufferRef[0])
    {
        SRMDebug("[%s] [%s] Custom scanout buffer succesfully set.",
                 connector->device->shortName,
                 connector->name);
        return 1;
    }

    srmConnectorReleaseUserScanoutBuffer(connector, 0);

    if (!buffer)
    {
        SRMDebug("[%s] [%s] Custom scanout buffer succesfully unset.",
                 connector->device->shortName,
                 connector->name);
        return 0;
    }

    if (connector->device != buffer->allocator)
    {
        SRMError("[%s] [%s] Failed to set custom scanout buffer. The buffer allocator must match the connector's device.",
                 connector->device->shortName,
                 connector->name);
        return 0;
    }

    SRMConnectorMode *mode = srmConnectorGetCurrentMode(connector);

    if (srmConnectorModeGetWidth(mode) != srmBufferGetWidth(buffer) || srmConnectorModeGetHeight(mode) != srmBufferGetHeight(buffer))
    {
        SRMError("[%s] [%s] Failed to set custom scanout buffer. The buffer dimensions must match the connector's mode size.",
                 connector->device->shortName,
                 connector->name);
        return 0;
    }

    if (srmBufferGetTexture(connector->device, buffer).id == 0)
    {
        SRMError("[%s] [%s] Failed to set custom scanout buffer. The buffer is not supported by the connector's device.",
                 connector->device->shortName,
                 connector->name);
        return 0;
    }

    /* Already created */

    if (buffer->scanout.fb)
    {
        if (!srmFormatIsInList(connector->currentPrimaryPlane->inFormats, buffer->scanout.fmt.format, buffer->scanout.fmt.modifier)
            || srmFormatIsInList(connector->currentPrimaryPlane->inFormatsBlacklist, buffer->scanout.fmt.format, buffer->scanout.fmt.modifier))
        {
            if (buffer->scanout.usingAlphaSubstitute)
            {
                SRMError("[%s] [%s] Failed to set custom scanout buffer. Format not supported by the primary plane.",
                         connector->device->shortName,
                         connector->name);

                return 0;
            }
            else
            {
                buffer->scanout.fmt.format = srmFormatGetAlphaSubstitute(buffer->scanout.fmt.format);
                buffer->scanout.usingAlphaSubstitute = 1;

                if (!srmFormatIsInList(connector->currentPrimaryPlane->inFormats, buffer->scanout.fmt.format, buffer->scanout.fmt.modifier)
                    || srmFormatIsInList(connector->currentPrimaryPlane->inFormatsBlacklist, buffer->scanout.fmt.format, buffer->scanout.fmt.modifier))
                {
                    SRMError("[%s] [%s] Failed to set custom scanout buffer. Unsupported format/modifier: %s - %s.",
                             connector->device->shortName,
                             connector->name,
                             drmGetFormatName(buffer->scanout.fmt.format),
                             drmGetFormatModifierName(buffer->scanout.fmt.modifier));
                    return 0;
                }
            }
        }

        connector->userScanoutBufferRef[0] = srmBufferGetRef(buffer);
        return 1;
    }

    struct gbm_bo *bo = NULL;

    /* If the buffer already has a bo*/
    if (buffer->bo)
        bo = buffer->bo;
    else
    {
        struct SRMBufferTexture *texture;
        SRMListForeach(item, buffer->textures)
        {
            texture = srmListItemGetData(item);

            if (texture->device == connector->device)
            {
                /* If already contains an EGL Image */
                if (texture->image != EGL_NO_IMAGE)
                {
                    bo = gbm_bo_import(connector->device->gbm, GBM_BO_IMPORT_EGL_IMAGE, texture->image, GBM_BO_USE_SCANOUT);

                    if (!bo)
                        break;

                    buffer->scanout.bo = bo;
                }

                break;
            }
        }
    }

    if (!bo)
    {
        SRMError("[%s] [%s] Failed to set custom scanout buffer. Could not get a GBM bo.",
                 connector->device->shortName,
                 connector->name);
        return 0;
    }

    connector->userScanoutBufferRef[0] = srmBufferGetRef(buffer);

    Int32 ret = 1;
    UInt32 handles[4] = {0};
    UInt32 pitches[4] = {0};
    UInt32 offsets[4] = {0};
    UInt64 modifiers[4] = {0};

    Int32 planesCount = gbm_bo_get_plane_count(bo);
    buffer->scanout.fmt.format = gbm_bo_get_format(bo);
    buffer->scanout.fmt.modifier = gbm_bo_get_modifier(bo);

    if (!srmFormatIsInList(connector->currentPrimaryPlane->inFormats, buffer->scanout.fmt.format, buffer->scanout.fmt.modifier)
        || srmFormatIsInList(connector->currentPrimaryPlane->inFormatsBlacklist, buffer->scanout.fmt.format, buffer->scanout.fmt.modifier))
    {
        UInt32 oldFmt = buffer->scanout.fmt.format;
        buffer->scanout.fmt.format = srmFormatGetAlphaSubstitute(buffer->scanout.fmt.format);
        buffer->scanout.usingAlphaSubstitute = 1;

        SRMError("[%s] [%s] Failed to set custom scanout buffer. Format %s not supported by primary plane. Trying alpha substitute format %s",
                 connector->device->shortName,
                 connector->name,
                 drmGetFormatName(oldFmt),
                 drmGetFormatName(buffer->scanout.fmt.format));

        if (!srmFormatIsInList(connector->currentPrimaryPlane->inFormats, buffer->scanout.fmt.format, buffer->scanout.fmt.modifier)
            || srmFormatIsInList(connector->currentPrimaryPlane->inFormatsBlacklist, buffer->scanout.fmt.format, buffer->scanout.fmt.modifier))
        {
            SRMError("[%s] [%s] Failed to set custom scanout buffer. Unsupported format/modifier: %s - %s.",
                     connector->device->shortName,
                     connector->name,
                     drmGetFormatName(buffer->scanout.fmt.format),
                     drmGetFormatModifierName(buffer->scanout.fmt.modifier));
            goto releaseAndFail;
        }
    }

    for (Int32 i = 0; i < planesCount; i++)
    {
        handles[i] = gbm_bo_get_handle_for_plane(bo, i).u32;
        pitches[i] = gbm_bo_get_stride_for_plane(bo, i);
        offsets[i] = gbm_bo_get_offset(bo, i);
        modifiers[i] = gbm_bo_get_modifier(bo);
    }

    if (connector->device->capAddFb2Modifiers && buffer->scanout.fmt.modifier != DRM_FORMAT_MOD_INVALID)
    {
        ret = drmModeAddFB2WithModifiers(
            connector->device->fd,
            buffer->dma.width, buffer->dma.height,
            buffer->scanout.fmt.format, handles, pitches, offsets, modifiers,
            &buffer->scanout.fb, DRM_MODE_FB_MODIFIERS);
    }

    if (ret)
    {
        SRMError("[%s] [%s] Failed to set custom scanout buffer using drmModeAddFB2WithModifiers(), trying drmModeAddFB2().",
                 connector->device->shortName,
                 connector->name);

        if (buffer->scanout.fmt.modifier != DRM_FORMAT_MOD_INVALID && buffer->scanout.fmt.modifier != DRM_FORMAT_MOD_LINEAR)
        {
            SRMError("[%s] [%s] Failed to set custom scanout buffer. drmModeAddFB2() and drmModeAddFB() do not support explicit modifiers.",
                     connector->device->shortName,
                     connector->name);
            goto releaseAndFail;
        }

        ret = drmModeAddFB2(
            connector->device->fd,
            buffer->dma.width, buffer->dma.height,
            buffer->scanout.fmt.format, handles, pitches, offsets,
            &buffer->scanout.fb, DRM_MODE_FB_MODIFIERS);
    }

    if (ret && planesCount == 1 && offsets[0] == 0)
    {
        SRMError("[%s] [%s] Failed to set custom scanout buffer using drmModeAddFB2(), trying drmModeAddFB().",
                 connector->device->shortName,
                 connector->name);

        UInt32 depth, bpp;
        srmFormatGetDepthBpp(buffer->scanout.fmt.format, &depth, &bpp);

        if (depth == 0 || bpp == 0)
        {
            SRMError("[%s] [%s] Failed to set custom scanout buffer using drmModeAddFB(), could not get depth and bpp for format %s.",
                     connector->device->shortName,
                     connector->name,
                     drmGetFormatName(buffer->scanout.fmt.format));
            goto releaseAndFail;
        }

        ret = drmModeAddFB(
            connector->device->fd,
            buffer->dma.width, buffer->dma.height,
            depth, bpp, pitches[0], handles[0],
            &buffer->scanout.fb);
    }

    if (ret)
    {
        SRMError("[%s] [%s] Failed to set custom scanout buffer using drmModeAddFB().",
                 connector->device->shortName,
                 connector->name);
        goto releaseAndFail;
    }

    return 1;

releaseAndFail:
    srmConnectorReleaseUserScanoutBuffer(connector, 0);
    return 0;
}

void srmConnectorSetCurrentBufferLocked(SRMConnector *connector, UInt8 locked)
{
    connector->lockCurrentBuffer = locked;
}

UInt8 srmConnectorIsCurrentBufferLocked(SRMConnector *connector)
{
    return connector->lockCurrentBuffer;
}
#endif
