#include <private/SRMConnectorModePrivate.h>
#include <string.h>

using namespace SRM;

SRMConnectorMode::SRMConnectorModePrivate::SRMConnectorModePrivate(SRMConnector *connector, SRMConnectorMode *connectorMode, drmModeModeInfo *info)
{
    this->connector = connector;
    this->connectorMode = connectorMode;
    memcpy(&this->info, info, sizeof(drmModeModeInfo));
}
