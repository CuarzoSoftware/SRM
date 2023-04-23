#include <SRMNamespaces.h>

const char *SRM::getRenderModeString(SRM_RENDER_MODE mode)
{
    switch (mode)
    {
        case SRM_RENDER_MODE_ITSELF:
        {
            return "ITSELF";
        } break;
        case SRM_RENDER_MODE_DUMB:
        {
            return "DUMB";
        } break;
        case SRM_RENDER_MODE_CPU:
        {
            return "CPU";
        } break;
        default:
        {
            return "UNKNOWN RENDER MODE";
        }
    }
}

const char *SRM::getPlaneTypeString(SRM_PLANE_TYPE type)
{
    switch (type)
    {
        case SRM_PLANE_TYPE_OVERLAY:
        {
            return "OVERLAY";
        } break;
        case SRM_PLANE_TYPE_PRIMARY:
        {
            return "PRIMARY";
        } break;
        case SRM_PLANE_TYPE_CURSOR:
        {
            return "CURSOR";
        } break;
        default:
        {
            return "UNKNOWN PLANE TYPE";
        }
    }
}

const char *SRM::getConnectorStateString(SRM_CONNECTOR_STATE state)
{
    switch (state)
    {
        case SRM_CONNECTOR_STATE_UNINITIALIZED:
        {
            return "UNINITIALIZED";
        } break;
        case SRM_CONNECTOR_STATE_INITIALIZED:
        {
            return "INITIALIZED";
        } break;
        case SRM_CONNECTOR_STATE_UNINITIALIZING:
        {
            return "UNINITIALIZING";
        } break;
        case SRM_CONNECTOR_STATE_INITIALIZING:
        {
            return "INITIALIZING";
        } break;
        case SRM_CONNECTOR_STATE_CHANGING_MODE:
        {
            return "CHANGING MODE";
        } break;
        default:
        {
            return "UNKNOWN CONNECTOR STATE";
        }
    }
}
