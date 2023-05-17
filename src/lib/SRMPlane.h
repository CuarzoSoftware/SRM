#ifndef SRMPLANE_H
#define SRMPLANE_H

#include "SRMTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

UInt32 srmPlaneGetID(SRMPlane *plane);
SRMDevice *srmPlaneGetDevice(SRMPlane *plane);
SRMList *srmPlaneGetCrtcs(SRMPlane *plane);
SRMConnector *srmPlaneGetCurrentConnector(SRMPlane *plane);
SRM_PLANE_TYPE srmPlaneGetType(SRMPlane *plane);

#ifdef __cplusplus
}
#endif

#endif // SRMPLANE_H
