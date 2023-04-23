#ifndef SRMLISTENERPRIVATE_H
#define SRMLISTENERPRIVATE_H

#include <SRMListener.h>

using namespace SRM;

class SRMListener::SRMListenerPrivate
{
public:
    SRMListenerPrivate() = default;
    ~SRMListenerPrivate() = default;
    void *callback = nullptr;
    void *userdata = nullptr;
    std::list<SRMListener*>*list;
    std::list<SRMListener*>::iterator link;
};

#endif // SRMLISTENERPRIVATE_H
