#include <private/SRMListenerPrivate.h>
#include <SRMList.h>
#include <stdlib.h>

void srmListenerSetUserData(SRMListener *listener, void *userData)
{
    listener->userData = userData;
}

void *srmListenerGetUserData(SRMListener *listener)
{
    return listener->userData;
}

void srmListenerSetCallbackFunction(SRMListener *listener, void *callbackFunction)
{
    listener->callback = callbackFunction;
}

void *srmListenerGetCallbackFunction(SRMListener *listener)
{
    return listener->callback;
}

void srmListenerDestroy(SRMListener *listener)
{
    SRMList *list = srmListItemGetList(listener->link);
    srmListRemoveItem(list, listener->link);
    free(listener);
}