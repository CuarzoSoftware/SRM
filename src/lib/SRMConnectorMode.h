#ifndef SRMCONNECTORMODE_H
#define SRMCONNECTORMODE_H

#include <SRMNamespaces.h>

class SRM::SRMConnectorMode
{
public:
    UInt32 width() const;
    UInt32 height() const;
    UInt32 refreshRate() const;
    SRMCore *core() const;
    SRMDevice *device() const;
    SRMConnector *connector() const;

    class SRMConnectorModePrivate;
    SRMConnectorModePrivate *imp() const;
private:
    friend class SRMConnector;
    friend class SRMDevice;
    static SRMConnectorMode *create(SRMConnector *connector, void *info);
    SRMConnectorMode(SRMConnector *connector, void *info);
    ~SRMConnectorMode();
    SRMConnectorModePrivate *m_imp = nullptr;
};

#endif // SRMCONNECTORMODE_H
