#ifndef SRMLISTENERPRIVATE_H
#define SRMLISTENERPRIVATE_H

#include <SRMListener.h>

struct SRMListenerStruct
{
    void *callback;
    void *userData;
    SRMListItem *link;
};

SRMListener *srmListenerCreate(SRMList *list, void *callbackFunction, void *userData);

#endif // SRMLISTENERPRIVATE_H
