#ifndef SRMLIST_H
#define SRMLIST_H

#include "SRMTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

SRMList *srmListCreate();
void srmListClear(SRMList *list);
void srmListDestoy(SRMList *list);

SRMListItem *srmListGetFront(SRMList *list);
SRMListItem *srmListGetBack(SRMList *list);
SRMListItem *srmListAppendData(SRMList *list, void *data);
SRMListItem *srmListPrependData(SRMList *list, void *data);
SRMListItem *srmListInsertData(SRMList *list, SRMListItem *prev, void *data);
void *srmListPopFront(SRMList *list);
void *srmListPopBack(SRMList *list);
void *srmListRemoveItem(SRMList *list, SRMListItem *item);

UInt32 srmListGetLength(SRMList *list);
Int32 srmListIsEmpty(SRMList *list);

// Item

SRMList *srmListItemGetList(SRMListItem *item);
SRMListItem *srmListItemGetNext(SRMListItem *item);
SRMListItem *srmListItemGetPrev(SRMListItem *item);
void *srmListItemGetData(SRMListItem *item);
void srmListItemSetData(SRMListItem *item, void *data);

// Loops
#define SRMListForeach(item, list) for(SRMListItem *item = srmListGetFront(list); item != NULL; item = srmListItemGetNext(item))
#define srmListForeachRev(item, list) for(SRMListItem *item = srmListGetBack(list); item != NULL; item = srmListItemGetPrev(item))

#ifdef __cplusplus
}
#endif

#endif // SRMLIST_H
