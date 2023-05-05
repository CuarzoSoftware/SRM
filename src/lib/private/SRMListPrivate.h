#ifndef SRMLISTPRIVATE_H
#define SRMLISTPRIVATE_H

#include <SRMTypes.h>

struct SRMListItemStruct
{
    SRMListItem *prev;
    SRMListItem *next;
    SRMList *list;
    void *data;
};

struct SRMListStruct
{
    SRMListItem *front;
    SRMListItem *back;
    UInt32 count;
};

#endif // SRMLISTPRIVATE_H
