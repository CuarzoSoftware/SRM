#ifndef SRMLIST_H
#define SRMLIST_H

#include "SRMTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup SRMList SRMList
 *
 * @brief Module for managing linked lists of data items.
 *
 * The SRMList module provides a simple linked list data structure and functions for creating,
 * manipulating, and iterating over lists of data items.
 *
 * @{
 */

/**
 * @struct SRMList
 * @brief Structure representing a linked list.
 */

/**
 * @struct SRMListItem
 * @brief Structure representing an item in a linked list.
 */

/**
 * @brief Create a new empty linked list.
 *
 * @return A pointer to the newly created SRMList.
 */
SRMList *srmListCreate();

/**
 * @brief Clear all items from a linked list without deallocating the list itself.
 *
 * @param list A pointer to the SRMList to clear.
 */
void srmListClear(SRMList *list);

/**
 * @brief Destroy a linked list and deallocate all associated memory.
 *
 * @param list A pointer to the SRMList to destroy. The pointer is set to NULL after destruction.
 */
void srmListDestroy(SRMList *list);

/**
 * @brief Get the front (head) item of a linked list.
 *
 * @param list A pointer to the SRMList.
 *
 * @return A pointer to the front SRMListItem, or NULL if the list is empty.
 */
SRMListItem *srmListGetFront(SRMList *list);

/**
 * @brief Get the back (tail) item of a linked list.
 *
 * @param list A pointer to the SRMList.
 *
 * @return A pointer to the back SRMListItem, or NULL if the list is empty.
 */
SRMListItem *srmListGetBack(SRMList *list);

/**
 * @brief Append data to the end of a linked list.
 *
 * @param list A pointer to the SRMList.
 * @param data A pointer to the data to append.
 *
 * @return A pointer to the newly created SRMListItem.
 */
SRMListItem *srmListAppendData(SRMList *list, void *data);

/**
 * @brief Prepend data to the front of a linked list.
 *
 * @param list A pointer to the SRMList.
 * @param data A pointer to the data to prepend.
 *
 * @return A pointer to the newly created SRMListItem.
 */
SRMListItem *srmListPrependData(SRMList *list, void *data);

/**
 * @brief Insert data after a specific item in a linked list.
 *
 * @param list A pointer to the SRMList.
 * @param prev A pointer to the SRMListItem after which the data should be inserted.
 * @param data A pointer to the data to insert.
 *
 * @return A pointer to the newly created SRMListItem.
 */
SRMListItem *srmListInsertData(SRMList *list, SRMListItem *prev, void *data);

/**
 * @brief Pop the front (head) item from a linked list and return its data.
 *
 * @param list A pointer to the SRMList.
 *
 * @return A pointer to the data from the popped SRMListItem, or NULL if the list is empty.
 */
void *srmListPopFront(SRMList *list);

/**
 * @brief Pop the back (tail) item from a linked list and return its data.
 *
 * @param list A pointer to the SRMList.
 *
 * @return A pointer to the data from the popped SRMListItem, or NULL if the list is empty.
 */
void *srmListPopBack(SRMList *list);

/**
 * @brief Remove a specific item from a linked list and return its data.
 *
 * @param list A pointer to the SRMList.
 * @param item A pointer to the SRMListItem to remove.
 *
 * @return A pointer to the data from the removed SRMListItem, or NULL if the item is not found.
 */
void *srmListRemoveItem(SRMList *list, SRMListItem *item);

/**
 * @brief Get the length (number of items) in a linked list.
 *
 * @param list A pointer to the SRMList.
 *
 * @return The number of items in the list.
 */
UInt32 srmListGetLength(SRMList *list);

/**
 * @brief Check if a linked list is empty.
 *
 * @param list A pointer to the SRMList.
 *
 * @return 1 if the list is empty, 0 if it contains items.
 */
Int32 srmListIsEmpty(SRMList *list);

/**
 * @brief Get the linked list associated with an SRMListItem.
 *
 * @param item A pointer to the SRMListItem.
 *
 * @return A pointer to the SRMList that contains the item.
 */
SRMList *srmListItemGetList(SRMListItem *item);

/**
 * @brief Get the next item in the linked list after the given SRMListItem.
 *
 * @param item A pointer to the SRMListItem.
 *
 * @return A pointer to the next SRMListItem, or NULL if the item is the last in the list.
 */
SRMListItem *srmListItemGetNext(SRMListItem *item);

/**
 * @brief Get the previous item in the linked list before the given SRMListItem.
 *
 * @param item A pointer to the SRMListItem.
 *
 * @return A pointer to the previous SRMListItem, or NULL if the item is the first in the list.
 */
SRMListItem *srmListItemGetPrev(SRMListItem *item);

/**
 * @brief Get the data associated with an SRMListItem.
 *
 * @param item A pointer to the SRMListItem.
 *
 * @return A pointer to the data associated with the item.
 */
void *srmListItemGetData(SRMListItem *item);

/**
 * @brief Set the data associated with an SRMListItem.
 *
 * @param item A pointer to the SRMListItem.
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
 * @param list A pointer to the SRMList to iterate.
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
 * @param list A pointer to the SRMList to iterate.
 */
#define SRMListForeachRev(item, list) for(SRMListItem *item = srmListGetBack(list); item != NULL; item = srmListItemGetPrev(item))

/**
 * @}
 */
#ifdef __cplusplus
}
#endif

#endif // SRMLIST_H
