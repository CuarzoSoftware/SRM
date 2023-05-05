#include <private/SRMListenerPrivate.h>
#include <SRMList.h>
#include <stdlib.h>

SRMListener *srmListenerCreate(SRMList *list, void *callbackFunction, void *userData)
{
    SRMListener *listener = calloc(1, sizeof(SRMListener));
    listener->callback = callbackFunction;
    listener->userData = userData;
    listener->link = srmListAppendData(list, listener);
    return listener;
}
