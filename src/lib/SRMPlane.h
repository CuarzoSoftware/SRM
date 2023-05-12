#ifndef SRMPLANE_H
#define SRMPLANE_H

#include <SRMTypes.h>


UInt32 srmPlaneGetID(SRMPlane *plane);
SRMDevice *srmPlaneGetDevice(SRMPlane *plane);
SRMList *srmPlaneGetCrtcs(SRMPlane *plane);
SRMConnector *srmPlaneGetCurrentConnector(SRMPlane *plane);
SRM_PLANE_TYPE srmPlaneGetType(SRMPlane *plane);

#endif // SRMPLANE_H
