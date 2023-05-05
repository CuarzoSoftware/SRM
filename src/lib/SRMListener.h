#ifndef SRMLISTENER_H
#define SRMLISTENER_H

#include <SRMTypes.h>

void srmListenerSetUserData(SRMListener *listener, void *userData);
void *srmListenerGetUserData(SRMListener *listener);
void srmListenerSetCallbackFunction(SRMListener *listener, void *callbackFunction);
void *srmListenerGetCallbackFunction(SRMListener *listener);
void srmListenerDestroy(SRMListener *listener);

#endif // SRMLISTENER_H
