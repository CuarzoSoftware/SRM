#include <CZ/SRM/SRMEncoder.h>
#include <CZ/SRM/SRMDevice.h>
#include <CZ/SRM/SRMLog.h>
#include <CZ/SRM/SRMConnector.h>
#include <CZ/SRM/SRMConnectorMode.h>
#include <CZ/Utils/CZVectorUtils.h>
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
        SRMError(CZLN, "[%s] Failed to get drmModeConnectorPtr for SRMConnector %d.", device->nodeName().c_str(), id);
        return {};
    }

    std::unique_ptr<SRMConnector> obj { new SRMConnector(id, device) };

    obj->updateProperties(res);
    obj->updateNames(res);
    obj->updateEncoders(res);
    obj->updateModes(res);
    obj->setContentType(RContentType::Graphics);
    drmModeFreeConnector(res);
    return obj.release();

    drmModeFreeConnector(res);
    SRMError(CZLN, "[%s] Failed to create SRMConnector %d.", device->nodeName().c_str(), id);
    return {};
}

bool SRMConnector::updateProperties(drmModeConnectorPtr res) noexcept
{
    m_subPixel = (RSubPixel)res->subpixel;
    m_mmSize.set(res->mmWidth, res->mmHeight);
    m_pf.setFlag(pConnected, res->connection == DRM_MODE_CONNECTED);
    m_type = res->connector_type;
    m_nameId = res->connector_type_id;

    memset(&m_propIDs, 0, sizeof(m_propIDs));

    if (!isConnected())
        return false;

    drmModeObjectPropertiesPtr props { drmModeObjectGetProperties(device()->fd(), id(), DRM_MODE_OBJECT_CONNECTOR) };

    if (!props)
    {
        SRMError(CZLN, "[%s] Failed to get drmModeObjectPropertiesPtr for SRMConnector %d.", device()->nodeName().c_str(), id());
        return false;
    }

    for (UInt32 i = 0; i < props->count_props; i++)
    {
        drmModePropertyPtr prop { drmModeGetProperty(device()->fd(), props->props[i]) };

        if (!prop)
        {
            SRMWarning(CZLN, "[%s] Failed to get property %d for SRMConnector %d.", device()->nodeName().c_str(), props->props[i], id());
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
            m_pf.setFlag(pNonDesktop, props->prop_values[i] == 1);
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
    m_name = std::format("{}-{}", TypeString(type()), m_nameId);

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
        SRMWarning(CZLN, "[%s] Could not get EDID property blob of SRMConnector %d: %s", device()->nodeName().c_str(), id(), strerror(errno));
        return false;
    }

    struct di_info *info = di_info_parse_edid(blob->data, blob->length);

    if (!info)
    {
        SRMWarning(CZLN, "[%s] Could not parse EDID of SRMConnector %d: %s", device()->nodeName().c_str(), id(), strerror(errno));
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
    m_currentEncoder = nullptr;

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

SRMDevice *SRMConnector::rendererDevice() const noexcept
{
    return device()->rendererDevice();
}

bool SRMConnector::setMode(SRMConnectorMode *mode) noexcept
{

}

bool SRMConnector::hasCursor() const noexcept
{
    return m_cursor[0].bo != nullptr;
}

bool SRMConnector::setCursor(UInt8 *pixels) noexcept
{

}

bool SRMConnector::setCursorPos(SkIPoint pos) noexcept
{

}

bool SRMConnector::initialize(SRMConnectorInterface *iface, void *data) noexcept
{

}

bool SRMConnector::repaint() noexcept
{

}

void SRMConnector::uninitialize() noexcept
{

}

bool SRMConnector::suspend() noexcept
{

}

bool SRMConnector::resume() noexcept
{

}

void SRMConnector::setContentType(RContentType type) noexcept
{

}


#if 1 == 2
UInt8 srmConnectorSetCursor(SRMConnector *connector, UInt8 *pixels)
{
    if (!connector->cursor[0].bo)
        return 0;

    if (!pixels && !connector->cursorVisible)
        return 1;

    pthread_mutex_lock(&connector->propsMutex);

    if (connector->currentCursorPlane)
    {
        if (pixels)
        {
            if (!connector->cursorVisible)
            {
                connector->cursorVisible = 1;
                connector->atomicChanges |= SRM_ATOMIC_CHANGE_CURSOR_VISIBILITY;
            }

            /* The index is updated during the atomic commit */
            UInt32 pendingCursorIndex = 1 - connector->cursorIndex;
            gbm_bo_write(connector->cursor[pendingCursorIndex].bo, pixels, 64*64*4);
            connector->atomicChanges |= SRM_ATOMIC_CHANGE_CURSOR_BUFFER;
        }
        else
        {
            connector->cursorVisible = 0;
            connector->atomicChanges |= SRM_ATOMIC_CHANGE_CURSOR_VISIBILITY;
        }

        pthread_mutex_unlock(&connector->propsMutex);
        pthread_cond_signal(&connector->repaintCond);
    }
    else
    {
        if (pixels)
        {
            connector->cursorVisible = 1;
            connector->cursorIndex = 1 - connector->cursorIndex;
            gbm_bo_write(connector->cursor[connector->cursorIndex].bo, pixels, 64*64*4);

            drmModeSetCursor(connector->device->fd,
                             connector->currentCrtc->id,
                             gbm_bo_get_handle(connector->cursor[connector->cursorIndex].bo).u32,
                             64,
                             64);
        }
        else
        {
            drmModeSetCursor(connector->device->fd,
                             connector->currentCrtc->id,
                             0,
                             0,
                             0);
        }

        pthread_mutex_unlock(&connector->propsMutex);
    }

    return 1;
}

UInt8 srmConnectorSetCursorPos(SRMConnector *connector, Int32 x, Int32 y)
{
    if (!connector->cursor[0].bo)
        return 0;

    if (connector->cursorX == x && connector->cursorY == y)
        return 1;

    pthread_mutex_lock(&connector->propsMutex);

    connector->cursorX = x;
    connector->cursorY = y;

    if (connector->currentCursorPlane)
    {
        if (connector->cursorVisible)
            connector->atomicChanges |= SRM_ATOMIC_CHANGE_CURSOR_POSITION;
        pthread_mutex_unlock(&connector->propsMutex);
        pthread_cond_signal(&connector->repaintCond);
    }
    else
    {
        drmModeMoveCursor(connector->device->fd,
                          connector->currentCrtc->id,
                          x,
                          y);

        pthread_mutex_unlock(&connector->propsMutex);
    }

    return 1;
}


UInt8 srmConnectorSetMode(SRMConnector *connector, SRMConnectorMode *mode)
{
    if (connector->currentMode == mode)
        return 1;

    pthread_mutex_lock(&connector->stateMutex);

    if (connector->state == SRM_CONNECTOR_STATE_INITIALIZED)
    {
        SRMConnectorMode *modeBackup = connector->currentMode;

        connector->targetMode = mode;
        connector->state = SRM_CONNECTOR_STATE_CHANGING_MODE;        
        pthread_mutex_unlock(&connector->stateMutex);

        retry:
        srmConnectorUnlockRenderThread(connector, 0);
        pthread_mutex_lock(&connector->stateMutex);

        if (connector->state == SRM_CONNECTOR_STATE_CHANGING_MODE)
        {
            pthread_mutex_unlock(&connector->stateMutex);
            goto retry;
        }

        if (connector->state == SRM_CONNECTOR_STATE_INITIALIZED)
        {
            pthread_mutex_unlock(&connector->stateMutex);
            return 1;
        }
        else // REVERTING MODE CHANGE
        {
            connector->targetMode = modeBackup;
            connector->state = SRM_CONNECTOR_STATE_CHANGING_MODE;
            pthread_mutex_unlock(&connector->stateMutex);
            goto retry;
            return 0;
        }
    }

    else if (connector->state == SRM_CONNECTOR_STATE_UNINITIALIZED)
    {
        pthread_mutex_unlock(&connector->stateMutex);
        connector->currentMode = mode;
        return 1;
    }

    // Wait for intermediate states to finish
    else
    {
        pthread_mutex_unlock(&connector->stateMutex);
        return 0;
    }

    pthread_mutex_unlock(&connector->stateMutex);
    return 1;
}

UInt8 srmConnectorInitialize(SRMConnector *connector, SRMConnectorInterface *interface, void *userData)
{
    if (connector->state != SRM_CONNECTOR_STATE_UNINITIALIZED)
        return 0;

    connector->state = SRM_CONNECTOR_STATE_INITIALIZING;

    // Find best config
    SRMEncoder *bestEncoder;
    SRMCrtc *bestCrtc;
    SRMPlane *bestPrimaryPlane;
    SRMPlane *bestCursorPlane;

    if (!srmConnectorGetBestConfiguration(connector, &bestEncoder, &bestCrtc, &bestPrimaryPlane, &bestCursorPlane))
    {
        // Fails to get a valid encoder, crtc or primary plane
        SRMWarning("[%s] [%s] Could not get a Encoder, Crtc and Primary Plane trio.",
                   connector->device->shortName,
                   connector->name);
        return 0;
    }

    connector->currentEncoder = bestEncoder;
    connector->currentCrtc = bestCrtc;
    connector->currentPrimaryPlane = bestPrimaryPlane;

    bestEncoder->currentConnector = connector;
    bestCrtc->currentConnector = connector;
    bestPrimaryPlane->currentConnector = connector;

    // When using legacy cursor IOCTLs, the plane is choosen by the driver
    if (!connector->device->core->forceLegacyCursor && connector->device->clientCapAtomic)
    {
        connector->currentCursorPlane = bestCursorPlane;

        if (bestCursorPlane)
            bestCursorPlane->currentConnector = connector;
    }
    else
        connector->currentCursorPlane = NULL;

    connector->fenceFD = -1;
    connector->interfaceData = userData;
    connector->interface = interface;

    connector->renderInitResult = 0;
    connector->firstPageFlip = 1;

    srmConnectorInitGamma(connector);

    if (pthread_create(&connector->renderThread, NULL, srmConnectorRenderThread, connector))
    {
        SRMError("[%s] [%s] Could not start rendering thread.", connector->device->shortName, connector->name);
        goto fail;
    }

    while (!connector->renderInitResult) { usleep(1000); }

    // If failed
    if (connector->renderInitResult != 1)
        goto fail;

    connector->state = SRM_CONNECTOR_STATE_INITIALIZED;

    SRMDebug("[%s] [%s] Initialized.", connector->device->shortName, connector->name);

    return 1;

fail:
    connector->state = SRM_CONNECTOR_STATE_UNINITIALIZED;

    if (bestCursorPlane)
        bestCursorPlane->currentConnector = NULL;

    srmConnectorRenderThreadCleanUp(connector);

    return 0;
}

UInt8 srmConnectorRepaint(SRMConnector *connector)
{
    if (connector->lockCurrentBuffer)
        return 0;

    if (connector->state == SRM_CONNECTOR_STATE_INITIALIZING ||
        connector->state == SRM_CONNECTOR_STATE_INITIALIZED ||
        connector->state == SRM_CONNECTOR_STATE_CHANGING_MODE)
    {
        srmConnectorUnlockRenderThread(connector, 1);
        return 1;
    }

    return 0;
}

void srmConnectorUninitialize(SRMConnector *connector)
{
    // Wait for those states to change
    while (connector->state == SRM_CONNECTOR_STATE_CHANGING_MODE ||
           connector->state == SRM_CONNECTOR_STATE_INITIALIZING)
    {
        usleep(20000);
    }

    // Nothing to do
    if (connector->state == SRM_CONNECTOR_STATE_UNINITIALIZED ||
        connector->state == SRM_CONNECTOR_STATE_UNINITIALIZING)
    {
        return;
    }

    // Unitialize
    connector->state = SRM_CONNECTOR_STATE_UNINITIALIZING;

    while (connector->state != SRM_CONNECTOR_STATE_UNINITIALIZED)
    {
        srmConnectorUnlockRenderThread(connector, 0);
        usleep(1000);
    }

    srmConnectorRenderThreadCleanUp(connector);

    SRMDebug("[%s] [%s] Uninitialized.", connector->device->shortName, connector->name);
}

UInt32 srmConnectorGetCurrentBufferAge(SRMConnector *connector)
{
    if (connector->state != SRM_CONNECTOR_STATE_INITIALIZED)
        return 0;

    UInt32 num = srmConnectorGetBuffersCount(connector);

    if (connector->bufferAgeFrame >= num)
        return num;

    return 0;
}

UInt32 srmConnectorGetCurrentBufferIndex(SRMConnector *connector)
{
    if (connector->state != SRM_CONNECTOR_STATE_INITIALIZED)
        return 0;

    return connector->renderInterface.getCurrentBufferIndex(connector);
}

UInt8 srmConnectorSuspend(SRMConnector *connector)
{
    if (connector->state == SRM_CONNECTOR_STATE_UNINITIALIZED)
        return 0;

    pthread_mutex_lock(&connector->stateMutex);

    switch (connector->state)
    {
        case SRM_CONNECTOR_STATE_SUSPENDED:
        {
            pthread_mutex_unlock(&connector->stateMutex);
            return 1;
        }
        case SRM_CONNECTOR_STATE_UNINITIALIZED:
        case SRM_CONNECTOR_STATE_UNINITIALIZING:
        {
            pthread_mutex_unlock(&connector->stateMutex);
            return 0;
        }
        case SRM_CONNECTOR_STATE_INITIALIZED:
        {
            connector->state = SRM_CONNECTOR_STATE_SUSPENDING;
            connector->atomicChanges = 0;
            pthread_mutex_unlock(&connector->stateMutex);
            return srmConnectorSuspend(connector);
        }
        default:
        {
            srmConnectorUnlockRenderThread(connector, 1);
            connector->atomicChanges = 0;
            pthread_mutex_unlock(&connector->stateMutex);
            usleep(10000);
            return srmConnectorSuspend(connector);
        }
    }

    pthread_mutex_unlock(&connector->stateMutex);
    return 0;
}

UInt8 srmConnectorResume(SRMConnector *connector)
{
    if (connector->state == SRM_CONNECTOR_STATE_UNINITIALIZED)
        return 0;

    pthread_mutex_lock(&connector->stateMutex);

    switch (connector->state)
    {
        case SRM_CONNECTOR_STATE_INITIALIZED:
        {
            pthread_mutex_unlock(&connector->stateMutex);
            return 1;
        }
        case SRM_CONNECTOR_STATE_UNINITIALIZED:
        case SRM_CONNECTOR_STATE_UNINITIALIZING:
        {
            pthread_mutex_unlock(&connector->stateMutex);
            return 0;
        }
        case SRM_CONNECTOR_STATE_SUSPENDED:
        {
            connector->state = SRM_CONNECTOR_STATE_RESUMING;
            pthread_mutex_unlock(&connector->stateMutex);
            return srmConnectorResume(connector);
        }
        default:
        {
            srmConnectorUnlockRenderThread(connector, 0);
            pthread_mutex_unlock(&connector->stateMutex);
            usleep(10000);
            return srmConnectorResume(connector);
        }
    }

    pthread_mutex_unlock(&connector->stateMutex);
    return 0;
}

UInt32 srmConnectorGetBuffersCount(SRMConnector *connector)
{
    if (connector->state != SRM_CONNECTOR_STATE_INITIALIZED)
        return 0;

    return connector->renderInterface.getBuffersCount(connector);
}

SRMBuffer *srmConnectorGetBuffer(SRMConnector *connector, UInt32 bufferIndex)
{
    if (connector->state != SRM_CONNECTOR_STATE_INITIALIZED)
        return NULL;

    return connector->renderInterface.getBuffer(connector, bufferIndex);
}

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

UInt32 srmConnectorGetFramebufferID(SRMConnector *connector)
{
    if (connector->state != SRM_CONNECTOR_STATE_INITIALIZED)
        return 0;

    return connector->renderInterface.getFramebufferID(connector);
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
