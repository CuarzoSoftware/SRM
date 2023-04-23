#ifndef SRMCONNECTORMODEPRIVATE_H
#define SRMCONNECTORMODEPRIVATE_H

#include <SRMConnectorMode.h>
#include <xf86drmMode.h>

class SRM::SRMConnectorMode::SRMConnectorModePrivate
{
public:
    SRMConnectorModePrivate(SRMConnector *connector, SRMConnectorMode *connectorMode, drmModeModeInfo *info);
    ~SRMConnectorModePrivate() = default;
    std::list<SRMConnectorMode*>::iterator connectorLink;
    SRMConnector *connector;
    SRMConnectorMode *connectorMode;
    drmModeModeInfo info;
};

#endif // SRMCONNECTORMODEPRIVATE_H
