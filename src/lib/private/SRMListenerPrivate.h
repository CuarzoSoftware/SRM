#ifndef SRMLISTENERPRIVATE_H
#define SRMLISTENERPRIVATE_H

#include <SRMListener.h>

#ifdef __cplusplus
extern "C" {
#endif

struct SRMListenerStruct
{
    void *callback;
    void *userData;
    SRMListItem *link;
};

SRMListener *srmListenerCreate(SRMList *list, void *callbackFunction, void *userData);

#ifdef __cplusplus
}
#endif

#endif // SRMLISTENERPRIVATE_H
