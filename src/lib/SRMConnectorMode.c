#include <private/SRMConnectorModePrivate.h>

SRMConnector *srmConnectorModeGetConnector(SRMConnectorMode *connectorMode)
{
    return connectorMode->connector;
}

UInt32 srmConnectorModeGetWidth(SRMConnectorMode *connectorMode)
{
    return connectorMode->info.hdisplay;
}

UInt32 srmConnectorModeGetHeight(SRMConnectorMode *connectorMode)
{
    return connectorMode->info.vdisplay;
}

UInt32 srmConnectorModeGetRefreshRate(SRMConnectorMode *connectorMode)
{
    return connectorMode->info.vrefresh;
}
