#ifndef SRMCONNECTORMODEPRIVATE_H
#define SRMCONNECTORMODEPRIVATE_H

#include <SRMConnectorMode.h>
#include <xf86drmMode.h>

struct SRMConnectorModeStruct
{
    SRMConnector *connector;
    SRMListItem *connectorLink;
    drmModeModeInfo info;
};

SRMConnectorMode *srmConnectorModeCreate(SRMConnector *connector, drmModeModeInfo *info);
void srmConnectorModeDestroy(SRMConnectorMode *connectorMode);

#endif // SRMCONNECTORMODEPRIVATE_H
