#include <private/SRMConnectorModePrivate.h>
#include <SRMConnector.h>

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

void srmConnectorModeSetUserData(SRMConnectorMode *connectorMode, void *userData)
{
    connectorMode->userData = userData;
}

void *srmConnectorModeGetUserData(SRMConnectorMode *connectorMode)
{
    return connectorMode->userData;
}

UInt8 srmConnectorModeIsPreferred(SRMConnectorMode *connectorMode)
{
    return connectorMode == srmConnectorGetPreferredMode(connectorMode->connector);
}
