#ifndef SRMLISTENER_H
#define SRMLISTENER_H

#include <SRMNamespaces.h>

class SRM::SRMListener
{
public:
    ~SRMListener();

    void setUserData(void *userdata);
    void setCallback(void *callback);

    void *userData() const;
    void *callback() const;

    class SRMListenerPrivate;
    SRMListenerPrivate *imp() const;
private:
    friend class SRMCore;
    SRMListener(std::list<SRMListener *> *list, void *callback, void *userdata);
    SRMListenerPrivate *m_imp = nullptr;
};

#endif // SRMLISTENER_H
