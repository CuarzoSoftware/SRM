#ifndef SRMLISTPRIVATE_H
#define SRMLISTPRIVATE_H

#include "../SRMTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif // SRMLISTPRIVATE_H
