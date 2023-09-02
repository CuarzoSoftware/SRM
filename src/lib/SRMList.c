#include <SRMList.h>
#include <private/SRMListPrivate.h>
#include <stdlib.h>
#include <pthread.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

SRMList *srmListCreate()
{
    return calloc(1, sizeof(SRMList));
}

void srmListClear(SRMList *list)
{
    while (!srmListIsEmpty(list))
        srmListPopBack(list);
}

void srmListDestroy(SRMList *list)
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
    pthread_mutex_lock(&mutex);
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
    pthread_mutex_unlock(&mutex);
    return item;
}

SRMListItem *srmListPrependData(SRMList *list, void *data)
{
    pthread_mutex_lock(&mutex);
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
    pthread_mutex_unlock(&mutex);
    return item;
}

SRMListItem *srmListInsertData(SRMList *list, SRMListItem *prev, void *data)
{
    pthread_mutex_lock(&mutex);
    if (prev == list->back)
    {
        pthread_mutex_unlock(&mutex);
        return srmListAppendData(list, data);
    }
    else if (prev == NULL)
    {
        pthread_mutex_unlock(&mutex);
        return srmListPrependData(list, data);
    }

    if (prev->list != list)
    {
        pthread_mutex_unlock(&mutex);
        return NULL;
    }

    SRMListItem *item = malloc(sizeof(SRMListItem));
    item->data = data;
    item->list = list;
    item->prev = prev;

    item->next = prev->next;
    prev->next = item;
    item->next->prev = item;

    list->count++;
    pthread_mutex_unlock(&mutex);

    return item;

}

void *srmListPopFront(SRMList *list)
{
    pthread_mutex_lock(&mutex);

    if (!list->front)
    {
        pthread_mutex_unlock(&mutex);
        return NULL;
    }

    if (list->count == 1)
    {
        void *data = list->front->data;
        free(list->front);
        list->front = NULL;
        list->back = NULL;
        list->count = 0;
        pthread_mutex_unlock(&mutex);
        return data;
    }

    SRMListItem *item = list->front;
    list->front = list->front->next;
    list->front->prev = NULL;
    list->count--;
    void *data = item->data;
    free(item);

    pthread_mutex_unlock(&mutex);

    return data;
}

void *srmListPopBack(SRMList *list)
{
    pthread_mutex_lock(&mutex);

    if (!list->back)
    {
        pthread_mutex_unlock(&mutex);
        return NULL;
    }

    if (list->count == 1)
    {
        void *data = list->back->data;
        free(list->back);
        list->front = NULL;
        list->back = NULL;
        list->count = 0;
        pthread_mutex_unlock(&mutex);
        return data;
    }

    SRMListItem *item = list->back;
    list->back = list->back->prev;
    list->back->next = NULL;
    list->count--;
    void *data = item->data;
    free(item);
    pthread_mutex_unlock(&mutex);
    return data;
}


void *srmListRemoveItem(SRMList *list, SRMListItem *item)
{
    pthread_mutex_lock(&mutex);

    if (item->list != list)
    {
        pthread_mutex_unlock(&mutex);
        return NULL;
    }

    if (item == list->front)
    {
        pthread_mutex_unlock(&mutex);
        return srmListPopFront(list);
    }
    else if (item == list->back)
    {
        pthread_mutex_unlock(&mutex);
        return srmListPopBack(list);
    }

    item->prev->next = item->next;
    item->next->prev = item->prev;
    list->count--;
    void *data = item->data;
    free(item);

    pthread_mutex_unlock(&mutex);

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
