#include <private/SRMConnectorModePrivate.h>
#include <SRMDevice.h>
#include <SRMConnector.h>

using namespace SRM;

UInt32 SRMConnectorMode::width() const
{
    return imp()->info.hdisplay;
}

UInt32 SRMConnectorMode::height() const
{
    return imp()->info.vdisplay;
}

UInt32 SRMConnectorMode::refreshRate() const
{
    return imp()->info.vrefresh;
}

SRMCore *SRMConnectorMode::core() const
{
    return device()->core();
}

SRMDevice *SRMConnectorMode::device() const
{
    return connector()->device();
}

SRMConnector *SRMConnectorMode::connector() const
{
    return imp()->connector;
}

SRMConnectorMode::SRMConnectorModePrivate *SRMConnectorMode::imp() const
{
    return m_imp;
}

SRM::SRMConnectorMode *SRMConnectorMode::create(SRMConnector *connector, void *info)
{
    SRMConnectorMode *connectorMode = new SRMConnectorMode(connector, info);
    return connectorMode;
}

SRMConnectorMode::SRMConnectorMode(SRMConnector *connector, void *info)
{
    m_imp = new SRMConnectorModePrivate(connector, this, (drmModeModeInfo*)info);
}

SRMConnectorMode::~SRMConnectorMode()
{
    delete m_imp;
}
