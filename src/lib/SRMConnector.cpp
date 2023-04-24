#include <private/SRMConnectorPrivate.h>
#include <private/SRMDevicePrivate.h>
#include <private/SRMEncoderPrivate.h>
#include <private/SRMCrtcPrivate.h>
#include <private/SRMPlanePrivate.h>
#include <SRMLog.h>
#include <xf86drmMode.h>
#include <gbm.h>

using namespace SRM;

SRMConnector::SRMConnectorPrivate *SRMConnector::imp() const
{
    return m_imp;
}

SRMCore *SRMConnector::core() const
{
    return device()->core();
}

SRMDevice *SRMConnector::device() const
{
    return imp()->device;
}

UInt32 SRMConnector::id() const
{
    return imp()->id;
}

SRM_CONNECTOR_STATE SRMConnector::state() const
{
    return imp()->state;
}

bool SRMConnector::connected() const
{
    return imp()->connected;
}

bool SRMConnector::hasHardwareCursor() const
{
    return imp()->cursorBO != nullptr; // imp()->currentCursorPlane != nullptr;
}

int SRMConnector::setCursor(UInt8 *pixels)
{
    if (!imp()->cursorBO)
        return 0;

    gbm_bo_write(imp()->cursorBO, pixels, 64*64*4);

    drmModeSetCursor(device()->fd(),
                     imp()->currentCrtc->id(),
                     gbm_bo_get_handle(imp()->cursorBO).u32,
                     64,
                     64);

    return 1;
}

int SRMConnector::setCursorPos(Int32 x, Int32 y)
{
    if (!imp()->cursorBO)
        return 0;

    drmModeMoveCursor(device()->fd(),
                      imp()->currentCrtc->id(),
                      x,
                      y);

    return 1;
}

UInt32 SRMConnector::mmWidth() const
{
    return imp()->mmWidth;
}

UInt32 SRMConnector::mmHeight() const
{
    return imp()->mmHeight;
}

const char *SRMConnector::name() const
{
    return imp()->name ? imp()->name : "Unknown";
}

const char *SRMConnector::manufacturer() const
{
    return imp()->manufacturer ? imp()->manufacturer : "Unknown";
}

const char *SRMConnector::model() const
{
    return imp()->model ? imp()->model : "Unknown";
}

const std::list<SRMEncoder *> &SRMConnector::encoders() const
{
    return imp()->encoders;
}

const std::list<SRMConnectorMode *> &SRMConnector::modes() const
{
    return imp()->modes;
}

SRMEncoder *SRMConnector::currentEncoder() const
{
    return imp()->currentEncoder;
}

SRMCrtc *SRMConnector::currentCrtc() const
{
    return imp()->currentCrtc;
}

SRMPlane *SRMConnector::currentPrimaryPlane() const
{
    return imp()->currentPrimaryPlane;
}

SRMPlane *SRMConnector::currentCursorPlane() const
{
    return imp()->currentCursorPlane;
}

SRMConnectorMode *SRMConnector::preferredMode() const
{
    return imp()->preferredMode;
}

SRMConnectorMode *SRMConnector::currentMode() const
{
    return imp()->currentMode;
}

int SRMConnector::setMode(SRMConnectorMode *mode)
{
    if (state() == SRM_CONNECTOR_STATE_INITIALIZED)
    {
        /* TODO handle */
    }

    return 1;
}

int SRMConnector::initialize(SRMConnectorInterface *interface, void *data)
{
    if (state() != SRM_CONNECTOR_STATE_UNINITIALIZED)
        return 0;

    imp()->state = SRM_CONNECTOR_STATE_INITIALIZING;

    // Find best config
    SRMEncoder *bestEncoder;
    SRMCrtc *bestCrtc;
    SRMPlane *bestPrimaryPlane;
    SRMPlane *bestCursorPlane;

    if (!imp()->getBestConfiguration(&bestEncoder, &bestCrtc, &bestPrimaryPlane, &bestCursorPlane))
    {
        // Fails to get a valid encoder, crtc or primary plane
        SRMLog::warning("Could not get a Encoder, Crtc and Primary Plane trio for connector %d.", id());
        return 0;
    }

    imp()->currentEncoder = bestEncoder;
    imp()->currentCrtc = bestCrtc;
    imp()->currentPrimaryPlane = bestPrimaryPlane;
    imp()->currentCursorPlane = bestCursorPlane;

    bestEncoder->imp()->currentConnector = this;
    bestCrtc->imp()->currentConnector = this;
    bestPrimaryPlane->imp()->currentConnector = this;

    if (bestCursorPlane)
        bestCursorPlane->imp()->currentConnector = this;

    imp()->interfaceData = data;
    imp()->interface = interface;

    Int32 initResult = 0;

    imp()->renderThread = new std::thread(&SRMConnectorPrivate::initRenderer, this, &initResult);

    while (!initResult) { /* WAIT */ }

    // If failed
    if (initResult != 1)
    {
        imp()->currentEncoder = nullptr;
        imp()->currentCrtc = nullptr;
        imp()->currentPrimaryPlane = nullptr;
        imp()->currentCursorPlane = nullptr;

        bestEncoder->imp()->currentConnector = nullptr;
        bestCrtc->imp()->currentConnector = nullptr;
        bestPrimaryPlane->imp()->currentConnector = nullptr;;

        if (bestCursorPlane)
            bestCursorPlane->imp()->currentConnector = nullptr;

        imp()->interfaceData = nullptr;
        imp()->interface = nullptr;
        imp()->renderThread = nullptr;

        imp()->state = SRM_CONNECTOR_STATE_UNINITIALIZED;
        return 0;
    }

    return 1;
}

int SRMConnector::repaint()
{
    if (state() != SRM_CONNECTOR_STATE_INITIALIZED)
        return 0;

    std::lock_guard<std::mutex> lock(imp()->renderMutex);
    imp()->repaintRequested = true;
    imp()->renderConditionVariable.notify_one();
    return 1;
}

int SRMConnector::unitialize()
{

}

SRM::SRMConnector *SRMConnector::createConnector(SRMDevice *device, UInt32 id)
{
    SRMConnector *connector = new SRMConnector(device, id);

    connector->imp()->updateProperties();
    connector->imp()->updateName();
    connector->imp()->updateEncoders();
    connector->imp()->updateModes();

    return connector;
}

SRMConnector::SRMConnector(SRMDevice *device, UInt32 id)
{
    m_imp = new SRMConnectorPrivate(device, this, id);
}

