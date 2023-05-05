#ifndef SRMCONNECTORMODE_H
#define SRMCONNECTORMODE_H

#include <SRMTypes.h>

SRMConnector *srmConnectorModeGetConnector(SRMConnectorMode *connectorMode);
UInt32 srmConnectorModeGetWidth(SRMConnectorMode *connectorMode);
UInt32 srmConnectorModeGetHeight(SRMConnectorMode *connectorMode);
UInt32 srmConnectorModeGetRefreshRate(SRMConnectorMode *connectorMode);

#endif // SRMCONNECTORMODE_H
