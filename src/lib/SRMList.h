#ifndef SRMLIST_H
#define SRMLIST_H

#include "SRMTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup SRMList SRMList
 *
 * @brief Module for managing linked lists.
 *
 * The @ref SRMList module provides a simple linked list data structure and functions for creating,
 * manipulating, and iterating over lists of pointers.
 *
 * @{
 */

/**
 * @brief Create a new empty linked list.
 *
 * @return A pointer to the newly created @ref SRMList.
 */
SRMList *srmListCreate();

/**
 * @brief Clear all items from a linked list without deallocating the list itself.
 *
 * @param list A pointer to the @ref SRMList to clear.
 */
void srmListClear(SRMList *list);

/**
 * @brief Destroy a linked list.
 *
 * @param list A pointer to the SRMList to destroy.
 * 
 * @note The actual data contained in each item is not automatically freed.
 */
void srmListDestroy(SRMList *list);

/**
 * @brief Get the front (head) item of a linked list.
 *
 * @param list A pointer to the @ref SRMList.
 *
 * @return A pointer to the front @ref SRMListItem, or `NULL` if the list is empty.
 */
SRMListItem *srmListGetFront(SRMList *list);

/**
 * @brief Get the back (tail) item of a linked list.
 *
 * @param list A pointer to the @ref SRMList.
 *
 * @return A pointer to the back @ref SRMListItem, or `NULL` if the list is empty.
 */
SRMListItem *srmListGetBack(SRMList *list);

/**
 * @brief Append data to the end of a linked list.
 *
 * @param list A pointer to the @ref SRMList.
 * @param data A pointer to the data to append.
 *
 * @return A pointer to the newly created @ref SRMListItem.
 */
SRMListItem *srmListAppendData(SRMList *list, void *data);

/**
 * @brief Prepend data to the front of a linked list.
 *
 * @param list A pointer to the @ref SRMList.
 * @param data A pointer to the data to prepend.
 *
 * @return A pointer to the newly created @ref SRMListItem.
 */
SRMListItem *srmListPrependData(SRMList *list, void *data);

/**
 * @brief Insert data after a specific item in a linked list.
 *
 * @param list A pointer to the @ref SRMList.
 * @param prev A pointer to the @ref SRMListItem after which the data should be inserted.
 * @param data A pointer to the data to insert.
 *
 * @return A pointer to the newly created @ref SRMListItem.
 */
SRMListItem *srmListInsertData(SRMList *list, SRMListItem *prev, void *data);

/**
 * @brief Pop the front (head) item from a linked list and return its data.
 *
 * @param list A pointer to the @ref SRMList.
 *
 * @return A pointer to the data from the popped @ref SRMListItem, or `NULL` if the list is empty.
 * 
 * @note The actual data contained in each item is not automatically freed.
 */
void *srmListPopFront(SRMList *list);

/**
 * @brief Pop the back (tail) item from a linked list and return its data.
 *
 * @param list A pointer to the @ref SRMList.
 *
 * @return A pointer to the data from the popped @ref SRMListItem, or `NULL` if the list is empty.
 * 
 * @note The actual data contained in each item is not automatically freed.
 */
void *srmListPopBack(SRMList *list);

/**
 * @brief Remove a specific item from a linked list and return its data.
 *
 * @param list A pointer to the @ref SRMList.
 * @param item A pointer to the @ref SRMListItem to remove.
 *
 * @return A pointer to the data from the removed @ref SRMListItem, or `NULL` if the item is not found.
 * 
 * @note The actual data contained in each item is not automatically freed.
 */
void *srmListRemoveItem(SRMList *list, SRMListItem *item);

/**
 * @brief Get the length (number of items) in a linked list.
 *
 * @param list A pointer to the @ref SRMList.
 *
 * @return The number of items in the list.
 */
UInt32 srmListGetLength(SRMList *list);

/**
 * @brief Check if a linked list is empty.
 *
 * @param list A pointer to the @ref SRMList.
 *
 * @return 1 if the list is empty, 0 if it contains items.
 */
Int32 srmListIsEmpty(SRMList *list);

/**
 * @brief Get the linked list associated with an @ref SRMListItem.
 *
 * @param item A pointer to the @ref SRMListItem.
 *
 * @return A pointer to the @ref SRMList that contains the item.
 */
SRMList *srmListItemGetList(SRMListItem *item);

/**
 * @brief Get the next item in the linked list after the given @ref SRMListItem.
 *
 * @param item A pointer to the @ref SRMListItem.
 *
 * @return A pointer to the next @ref SRMListItem, or `NULL` if the item is the last in the list.
 */
SRMListItem *srmListItemGetNext(SRMListItem *item);

/**
 * @brief Get the previous item in the linked list before the given @ref SRMListItem.
 *
 * @param item A pointer to the @ref SRMListItem.
 *
 * @return A pointer to the previous @ref SRMListItem, or `NULL` if the item is the first in the list.
 */
SRMListItem *srmListItemGetPrev(SRMListItem *item);

/**
 * @brief Get the data associated with an @ref SRMListItem.
 *
 * @param item A pointer to the @ref SRMListItem.
 *
 * @return A pointer to the data associated with the item.
 */
void *srmListItemGetData(SRMListItem *item);

/**
 * @brief Set the data associated with an @ref SRMListItem.
 *
 * @param item A pointer to the @ref SRMListItem.
 * @param data A pointer to the data to associate with the item.
 */
void srmListItemSetData(SRMListItem *item, void *data);

/**
 * @def SRMListForeach(item, list)
 * @brief Iterate over items in a linked list from front to back.
 *
 * This macro provides a convenient way to iterate over items in a linked list, moving from the front
 * (head) to the back (tail) of the list.
 *
 * Example usage:
 * ```
 * SRMListForeach(item, myList) {
 *     // Process srmListItemGetData(item)
 * }
 * ```
 *
 * @param item A variable to represent the current item being iterated.
 * @param list A pointer to the @ref SRMList to iterate.
 */
#define SRMListForeach(item, list) for(SRMListItem *item = srmListGetFront(list); item != NULL; item = srmListItemGetNext(item))

/**
 * @def SRMListForeachRev(item, list)
 * @brief Iterate over items in a linked list from back to front.
 *
 * This macro provides a convenient way to iterate over items in a linked list, moving from the back
 * (tail) to the front (head) of the list.
 *
 * Example usage:
 * ```
 * SRMListForeachRev(item, myList) {
 *     // Process srmListItemGetData(item)
 * }
 * ```
 *
 * @param item A variable to represent the current item being iterated.
 * @param list A pointer to the @ref SRMList to iterate.
 */
#define SRMListForeachRev(item, list) for(SRMListItem *item = srmListGetBack(list); item != NULL; item = srmListItemGetPrev(item))

/**
 * @}
 */
#ifdef __cplusplus
}
#endif

#endif // SRMLIST_H
