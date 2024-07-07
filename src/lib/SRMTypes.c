#include <SRMTypes.h>
#include <SRMDevice.h>
#include <SRMPlane.h>
#include <SRMConnector.h>
#include <xf86drmMode.h>

const char *srmGetRenderModeString(SRM_RENDER_MODE mode)
{
    switch (mode)
    {
        case SRM_RENDER_MODE_ITSELF:    return "ITSELF";
        case SRM_RENDER_MODE_PRIME:     return "PRIME";
        case SRM_RENDER_MODE_DUMB:      return "DUMB";
        case SRM_RENDER_MODE_CPU:       return "CPU";
        default:                        return "UNKNOWN RENDER MODE";
    }
}

const char *srmGetPlaneTypeString(SRM_PLANE_TYPE type)
{
    switch (type)
    {
        case SRM_PLANE_TYPE_OVERLAY:    return "OVERLAY";
        case SRM_PLANE_TYPE_PRIMARY:    return "PRIMARY";
        case SRM_PLANE_TYPE_CURSOR:     return "CURSOR";
        default:                        return "UNKNOWN PLANE TYPE";
    }
}

const char *srmGetConnectorStateString(SRM_CONNECTOR_STATE state)
{
    switch (state)
    {
        case SRM_CONNECTOR_STATE_UNINITIALIZED:     return "UNINITIALIZED";
        case SRM_CONNECTOR_STATE_INITIALIZED:       return "INITIALIZED";
        case SRM_CONNECTOR_STATE_UNINITIALIZING:    return "UNINITIALIZING";
        case SRM_CONNECTOR_STATE_INITIALIZING:      return "INITIALIZING";
        case SRM_CONNECTOR_STATE_CHANGING_MODE:     return "CHANGING MODE";
        default:                                    return "UNKNOWN CONNECTOR STATE";
    }
}

const char *srmGetConnectorTypeString(UInt32 type)
{
    switch (type)
    {
        case DRM_MODE_CONNECTOR_Unknown:     return "unknown";
        case DRM_MODE_CONNECTOR_VGA:         return "VGA";
        case DRM_MODE_CONNECTOR_DVII:        return "DVI-I";
        case DRM_MODE_CONNECTOR_DVID:        return "DVI-D";
        case DRM_MODE_CONNECTOR_DVIA:        return "DVI-A";
        case DRM_MODE_CONNECTOR_Composite:   return "composite";
        case DRM_MODE_CONNECTOR_SVIDEO:      return "S-VIDEO";
        case DRM_MODE_CONNECTOR_LVDS:        return "LVDS";
        case DRM_MODE_CONNECTOR_Component:   return "component";
        case DRM_MODE_CONNECTOR_9PinDIN:     return "DIN";
        case DRM_MODE_CONNECTOR_DisplayPort: return "DisplayPort";
        case DRM_MODE_CONNECTOR_HDMIA:       return "HDMI-A";
        case DRM_MODE_CONNECTOR_HDMIB:       return "HDMI-B";
        case DRM_MODE_CONNECTOR_TV:          return "TV";
        case DRM_MODE_CONNECTOR_eDP:         return "eDP";
        case DRM_MODE_CONNECTOR_VIRTUAL:     return "virtual";
        case DRM_MODE_CONNECTOR_DSI:         return "DSI";
        case DRM_MODE_CONNECTOR_DPI:         return "DPI";
        case DRM_MODE_CONNECTOR_WRITEBACK:   return "writeback";
        default:                             return "unknown";
    }
}

const char *srmGetConnectorSubPixelString(SRM_CONNECTOR_SUBPIXEL subpixel)
{
    switch (subpixel)
    {
        case SRM_CONNECTOR_SUBPIXEL_UNKNOWN:        return "UNKNOWN";
        case SRM_CONNECTOR_SUBPIXEL_HORIZONTAL_RGB: return "HORIZONTAL_RGB";
        case SRM_CONNECTOR_SUBPIXEL_HORIZONTAL_BGR: return "HORIZONTAL_BGR";
        case SRM_CONNECTOR_SUBPIXEL_VERTICAL_RGB:   return "VERTICAL_RGB";
        case SRM_CONNECTOR_SUBPIXEL_VERTICAL_BGR:   return "VERTICAL_BGR";
        case SRM_CONNECTOR_SUBPIXEL_NONE:           return "NONE";
        default:                                    return "unknown";
    }
}

const char *srmGetConnectorContentTypeString(SRM_CONNECTOR_CONTENT_TYPE contentType)
{
    switch (contentType)
    {
    case SRM_CONNECTOR_CONTENT_TYPE_GRAPHICS:   return "GRAPHICS";
    case SRM_CONNECTOR_CONTENT_TYPE_PHOTO:      return "PHOTO";
    case SRM_CONNECTOR_CONTENT_TYPE_VIDEO:      return "VIDEO";
    case SRM_CONNECTOR_CONTENT_TYPE_GAME:       return "GAME";
    default:                                    return "unknown";
    }
}
