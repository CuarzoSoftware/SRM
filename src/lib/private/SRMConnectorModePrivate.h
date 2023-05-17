#ifndef SRMCONNECTORMODEPRIVATE_H
#define SRMCONNECTORMODEPRIVATE_H

#include "../SRMConnectorMode.h"
#include <xf86drmMode.h>

#ifdef __cplusplus
extern "C" {
#endif

struct SRMConnectorModeStruct
{
    void *userData;
    SRMConnector *connector;
    SRMListItem *connectorLink;
    drmModeModeInfo info;
};

SRMConnectorMode *srmConnectorModeCreate(SRMConnector *connector, drmModeModeInfo *info);
void srmConnectorModeDestroy(SRMConnectorMode *connectorMode);

#ifdef __cplusplus
}
#endif

#endif // SRMCONNECTORMODEPRIVATE_H
