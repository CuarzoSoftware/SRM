#ifndef SRMCONNECTORMODE_H
#define SRMCONNECTORMODE_H

#include "SRMTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

void srmConnectorModeSetUserData(SRMConnectorMode *connectorMode, void *userData);
void *srmConnectorModeGetUserData(SRMConnectorMode *connectorMode);

SRMConnector *srmConnectorModeGetConnector(SRMConnectorMode *connectorMode);
UInt32 srmConnectorModeGetWidth(SRMConnectorMode *connectorMode);
UInt32 srmConnectorModeGetHeight(SRMConnectorMode *connectorMode);
UInt32 srmConnectorModeGetRefreshRate(SRMConnectorMode *connectorMode);
UInt8 srmConnectorModeIsPreferred(SRMConnectorMode *connectorMode);

#ifdef __cplusplus
}
#endif

#endif // SRMCONNECTORMODE_H
