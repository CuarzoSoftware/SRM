#include <SRMList.h>
#include <private/SRMListPrivate.h>
#include <stdlib.h>

SRMList *srmListCreate()
{
    return calloc(1, sizeof(SRMList));
}

void srmListClear(SRMList *list)
{
    while (!srmListIsEmpty(list))
        srmListPopBack(list);
}

void srmListDestoy(SRMList *list)
{
    srmListClear(list);
    free(list);
}

SRMListItem *srmListGetFront(SRMList *list)
{
    return list->front;
}

SRMListItem *srmListGetBack(SRMList *list)
{
    return list->back;
}

SRMListItem *srmListAppendData(SRMList *list, void *data)
{
    SRMListItem *item = malloc(sizeof(SRMListItem));
    item->data = data;
    item->list = list;
    item->next = NULL;
    item->prev = list->back;

    if (list->back)
    {
        list->back->next = item;
        list->back = item;
    }
    else
    {
        list->front = item;
        list->back = item;
    }

    list->count++;

    return item;
}

SRMListItem *srmListPrependData(SRMList *list, void *data)
{
    SRMListItem *item = malloc(sizeof(SRMListItem));
    item->data = data;
    item->list = list;
    item->prev = NULL;
    item->next = list->front;

    if (list->front)
    {
        list->front->prev = item;
        list->front = item;
    }
    else
    {
        list->front = item;
        list->back = item;
    }

    list->count++;

    return item;
}

SRMListItem *srmListInsertData(SRMList *list, SRMListItem *prev, void *data)
{
    if (prev == list->back)
        return srmListAppendData(list, data);
    else if (prev == NULL)
        return srmListPrependData(list, data);

    if (prev->list != list)
        return NULL;

    SRMListItem *item = malloc(sizeof(SRMListItem));
    item->data = data;
    item->list = list;
    item->prev = prev;

    item->next = prev->next;
    prev->next = item;
    item->next->prev = item;

    list->count++;

    return item;

}

void *srmListPopFront(SRMList *list)
{
    if (!list->front)
        return NULL;

    if (list->count == 1)
    {
        void *data = list->front->data;
        free(list->front);
        list->front = NULL;
        list->back = NULL;
        list->count = 0;
        return data;
    }

    SRMListItem *item = list->front;
    list->front = list->front->next;
    list->front->prev = NULL;
    list->count--;
    void *data = item->data;
    free(item);
    return data;
}

void *srmListPopBack(SRMList *list)
{
    if (!list->back)
        return NULL;

    if (list->count == 1)
    {
        void *data = list->back->data;
        free(list->back);
        list->front = NULL;
        list->back = NULL;
        list->count = 0;
        return data;
    }

    SRMListItem *item = list->back;
    list->back = list->back->prev;
    list->back->next = NULL;
    list->count--;
    void *data = item->data;
    free(item);
    return data;
}


void *srmListRemoveItem(SRMList *list, SRMListItem *item)
{
    if (item->list != list)
        return NULL;

    if (item == list->front)
        return srmListPopFront(list);
    else if (item == list->back)
        return srmListPopBack(list);

    item->prev->next = item->next;
    item->next->prev = item->prev;
    list->count--;
    void *data = item->data;
    free(item);
    return data;
}


UInt32 srmListGetLength(SRMList *list)
{
    return list->count;
}

Int32 srmListIsEmpty(SRMList *list)
{
    return list->count == 0;
}

SRMList *srmListItemGetList(SRMListItem *item)
{
    return item->list;
}

SRMListItem *srmListItemGetNext(SRMListItem *item)
{
    return item->next;
}

SRMListItem *srmListItemGetPrev(SRMListItem *item)
{
    return item->prev;
}

void *srmListItemGetData(SRMListItem *item)
{
    return item->data;
}

void srmListItemSetData(SRMListItem *item, void *data)
{
    item->data = data;
}
