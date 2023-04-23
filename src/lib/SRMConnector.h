#ifndef SRMCONNECTOR_H
#define SRMCONNECTOR_H

#include <SRMNamespaces.h>

class SRM::SRMConnector
{
public:
    class SRMConnectorPrivate;
    SRMConnectorPrivate *imp() const;
    SRMCore *core() const;
    SRMDevice *device() const;

    UInt32 id() const;
    SRM_CONNECTOR_STATE state() const;
    bool connected() const;
    UInt32 mmWidth() const;
    UInt32 mmHeight() const;
    const char *name() const;
    const char *manufacturer() const;
    const char *model() const;

    // Cursor
    bool hasHardwareCursor() const;
    int setCursor(UInt8 *pixels);
    int setCursorPos(Int32 x, Int32 y);

    SRMEncoder *currentEncoder() const;
    SRMCrtc *currentCrtc() const;
    SRMPlane *currentPrimaryPlane() const;
    SRMPlane *currentCursorPlane() const;
    SRMConnectorMode *currentMode() const;

    const std::list<SRMEncoder*> &encoders() const;
    const std::list<SRMConnectorMode*> &modes() const;

    SRMConnectorMode *preferredMode() const;

    int setMode(SRMConnectorMode *mode);

    int initialize(SRMConnectorInterface *interface, void *data);
    int repaint();
    int unitialize();
private:
    friend class SRMDevice;
    static SRMConnector *createConnector(SRMDevice *device, UInt32 id);
    SRMConnector(SRMDevice *device, UInt32 id);
    SRMConnectorPrivate * m_imp = nullptr;
};

#endif // SRMCONNECTOR_H
