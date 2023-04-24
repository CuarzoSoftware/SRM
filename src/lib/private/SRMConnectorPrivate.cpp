#include <private/SRMConnectorPrivate.h>
#include <private/SRMConnectorModePrivate.h>

#include <xf86drmMode.h>
#include <SRMDevice.h>
#include <SRMEncoder.h>
#include <SRMCrtc.h>
#include <SRMPlane.h>
#include <SRMLog.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

extern "C" {
#include <libdisplay-info/edid.h>
#include <libdisplay-info/info.h>
}

using namespace SRM;

SRMConnector::SRMConnectorPrivate::SRMConnectorPrivate(SRMDevice *device, SRMConnector *connector, UInt32 id)
{
    this->device = device;
    this->connector = connector;
    this->id = id;
}

int SRMConnector::SRMConnectorPrivate::updateEncoders()
{
    drmModeConnector *connectorRes = drmModeGetConnector(device->fd(), id);

    if (!connectorRes)
    {
        fprintf(stderr, "SRM Error: Could not get connector %d resources.\n", id);
        return 0;
    }

    for (int i = 0; i < connectorRes->count_encoders; i++)
    {
        for (SRMEncoder *encoder : device->encoders())
        {
            if (encoder->id() == connectorRes->encoders[i])
            {
                encoders.push_back(encoder);
            }
        }
    }

    drmModeFreeConnector(connectorRes);

    return 1;
}

int SRMConnector::SRMConnectorPrivate::updateName()
{
    if (name)
    {
        delete []name;
        name = nullptr;
    }

    if (manufacturer)
    {
        delete []manufacturer;
        manufacturer = nullptr;
    }

    if (model)
    {
        delete []model;
        model = nullptr;
    }

    if (!connected)
        return 0;

    drmModeConnector *connectorRes = drmModeGetConnector(device->fd(), id);

    if (!connectorRes)
    {
        fprintf(stderr, "SRM Error: Could not get connector %d resources.\n", id);
        return 0;
    }

    drmModePropertyBlobPtr blob = nullptr;

    for (int i = 0; i < connectorRes->count_props; i++)
    {
        drmModePropertyPtr prop = drmModeGetProperty(device->fd(), connectorRes->props[i]);

        if (prop && (strcmp(prop->name, "EDID") == 0))
        {
            blob = drmModeGetPropertyBlob(device->fd(), connectorRes->prop_values[i]);
            drmModeFreeProperty(prop);
            break;
        }

        drmModeFreeProperty(prop);
    }

    if (!blob)
    {        
        fprintf(stderr, "SRM Error: Error getting EDID property blob for connector %d: %s\n", id, strerror(errno));
        drmModeFreeConnector(connectorRes);
        return 0;
    }

    di_info *info = di_info_parse_edid(blob->data, blob->length);

    if (!info)
    {
        fprintf(stderr, "SRM Error: Failed to parse EDID of connector %d: %s\n", id, strerror(errno));
        drmModeFreePropertyBlob(blob);
        drmModeFreeConnector(connectorRes);
        return 0;
    }

    int len = strlen(di_info_get_make(info));

    if (len > 0)
    {
        manufacturer = new char[len+1];
        memcpy(manufacturer, di_info_get_make(info), len+1);
    }

    len = strlen(di_info_get_model(info));

    if (len > 0)
    {
        model = new char[len+1];
        memcpy(model, di_info_get_model(info), len+1);
    }

    di_info_destroy(info);

    drmModeFreePropertyBlob(blob);
    drmModeFreeConnector(connectorRes);

    return 1;

}

int SRMConnector::SRMConnectorPrivate::updateModes()
{
    drmModeConnector *connectorRes = drmModeGetConnector(device->fd(), id);

    if (!connectorRes)
    {
        fprintf(stderr, "SRM Error: Could not get connector %d resources.\n", id);
        return 0;
    }

    for (int i = 0; i < connectorRes->count_modes; i++)
    {
        SRMConnectorMode *connectorMode = SRMConnectorMode::create(connector, (void*)&connectorRes->modes[i]);

        if (connectorMode)
        {
            modes.push_back(connectorMode);
            connectorMode->imp()->connectorLink = std::prev(modes.end());
        }
    }

    preferredMode = findPreferredMode();

    // If there is no current mode, set the preferred as default
    if (!currentMode)
        currentMode = preferredMode;

    drmModeFreeConnector(connectorRes);

    return 1;
}

int SRMConnector::SRMConnectorPrivate::updateProperties()
{
    drmModeConnector *connectorRes = drmModeGetConnector(device->fd(), id);

    if (!connectorRes)
    {
        SRMLog::error("Could not get connector %d resources.", id);
        return 0;
    }

    mmHeight = connectorRes->mmHeight;
    mmWidth = connectorRes->mmWidth;
    connected = connectorRes->connection == DRM_MODE_CONNECTED;

    drmModeFreeConnector(connectorRes);

    drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(device->fd(), id, DRM_MODE_OBJECT_CONNECTOR);

    if (!props)
    {
        SRMLog::error("Unable to get DRM connector %d properties.", id);
        return 0;
    }

    memset(&propIDs, 0, sizeof(SRMConnectorPropIDs));

    for (UInt32 i = 0; i < props->count_props; i++)
    {
        drmModePropertyPtr prop = drmModeGetProperty(device->fd(), props->props[i]);

        if (!prop)
        {
            SRMLog::warning("Could not get property %d of connector %d.", props->props[i], id);
            continue;
        }

        if (strcmp(prop->name, "CRTC_ID") == 0)
            propIDs.CRTC_ID = prop->prop_id;
        else if (strcmp(prop->name, "DPMS") == 0)
            propIDs.DPMS = prop->prop_id;
        else if (strcmp(prop->name, "EDID") == 0)
            propIDs.EDID = prop->prop_id;
        else if (strcmp(prop->name, "PATH") == 0)
            propIDs.PATH = prop->prop_id;
        else if (strcmp(prop->name, "link-status") == 0)
            propIDs.link_status = prop->prop_id;
        else if (strcmp(prop->name, "non-desktop") == 0)
            propIDs.non_desktop = prop->prop_id;
        else if (strcmp(prop->name, "panel orientation") == 0)
            propIDs.panel_orientation = prop->prop_id;
        else if (strcmp(prop->name, "subconnector") == 0)
            propIDs.subconnector = prop->prop_id;
        else if (strcmp(prop->name, "vrr_capable") == 0)
            propIDs.vrr_capable = prop->prop_id;

        drmModeFreeProperty(prop);
    }

    drmModeFreeObjectProperties(props);

    return 1;
}

// Find a valid (encoder,crtc and primary plane) trio and a cursor plane if avaliable
// Returns 1 if a valid trio is found and 0 otherwise
int SRMConnector::SRMConnectorPrivate::getBestConfiguration(SRMEncoder **bestEncoder,
                                                            SRMCrtc **bestCrtc,
                                                            SRMPlane **bestPrimaryPlane,
                                                            SRMPlane **bestCursorPlane)
{
    int bestScore = 0;
    *bestEncoder = nullptr;
    *bestCrtc = nullptr;
    *bestPrimaryPlane = nullptr;
    *bestCursorPlane = nullptr;

    for (SRMEncoder *encoder : encoders)
    {
        for (SRMCrtc *crtc : encoder->crtcs())
        {
            // Check if already used by other connector
            if (crtc->currentConnector())
                continue;

            int score = 0;
            SRMPlane *primaryPlane = nullptr;
            SRMPlane *cursorPlane = nullptr;

            // Check if has a cursor plane
            for (SRMPlane *plane : device->planes())
            {
                if (plane->type() == SRM_PLANE_TYPE_OVERLAY)
                    continue;

                for (SRMCrtc *planeCrtc : plane->crtcs())
                {
                    // Check if already used by other connector
                    if (planeCrtc->currentConnector())
                        continue;

                    if (planeCrtc->id() == crtc->id())
                    {
                        if (plane->type() == SRM_PLANE_TYPE_PRIMARY)
                        {
                            primaryPlane = plane;
                            break;
                        }
                        else if (plane->type() == SRM_PLANE_TYPE_CURSOR)
                        {
                            cursorPlane = plane;
                            break;
                        }
                    }
                }
            }

            // Can not render if no primary plane
            if (!primaryPlane)
                continue;

            score += 100;

            if (cursorPlane)
                score += 50;

            if (score > bestScore)
            {
                bestScore = score;
                *bestEncoder = encoder;
                *bestCrtc = crtc;
                *bestPrimaryPlane = primaryPlane;
                *bestCursorPlane = cursorPlane;
            }
        }
    }

    return *bestEncoder && *bestCrtc && *bestPrimaryPlane;
}

SRMConnectorMode *SRMConnector::SRMConnectorPrivate::findPreferredMode()
{
    SRMConnectorMode *preferredMode = nullptr;

    Int32 greatestSize = -1;

    for (SRMConnectorMode *connectorMode : modes)
    {
        if (connectorMode->imp()->info.type & DRM_MODE_TYPE_PREFERRED)
        {
            preferredMode = connectorMode;
            break;
        }

        // If no mode has the preferred flag, look for the one with greatest dimensions
        Int32 currentSize = connectorMode->width() * connectorMode->height();

        if (currentSize > greatestSize)
        {
            greatestSize = currentSize;
            preferredMode = connectorMode;
        }
    }

    return preferredMode;
}

