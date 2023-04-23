#include <private/SRMListenerPrivate.h>

using namespace SRM;

SRMListener::SRMListener(std::list<SRMListener*>*list, void *callback, void *userdata)
{
    m_imp = new SRMListenerPrivate();
    setCallback(callback);
    setUserData(userdata);
    imp()->list = list;
    list->push_back(this);
    imp()->link = std::prev(list->end());
}

SRMListener::~SRMListener()
{
    imp()->list->erase(imp()->link);
    delete m_imp;
}

void SRMListener::setUserData(void *userdata)
{
    imp()->userdata = userdata;
}

void SRMListener::setCallback(void *callback)
{
    imp()->callback = callback;
}

void *SRMListener::userData() const
{
    return imp()->userdata;
}

void *SRMListener::callback() const
{
    return imp()->callback;
}

SRMListener::SRMListenerPrivate *SRMListener::imp() const
{
    return m_imp;
}
