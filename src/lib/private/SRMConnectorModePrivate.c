#include <private/SRMConnectorModePrivate.h>
#include <stdlib.h>
#include <string.h>

SRMConnectorMode *srmConnectorModeCreate(SRMConnector *connector, drmModeModeInfo *info)
{
    SRMConnectorMode *connectorMode = calloc(1, sizeof(SRMConnectorMode));
    connectorMode->connector = connector;
    memcpy(&connectorMode->info, info, sizeof(drmModeModeInfo));
    return connectorMode;
}

void srmConnectorModeDestroy(SRMConnectorMode *connectorMode)
{
    free(connectorMode);
}
