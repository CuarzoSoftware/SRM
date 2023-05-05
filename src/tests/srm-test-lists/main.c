#include <SRMList.h>
#include <SRMLog.h>
#include <stdlib.h>

static int test_01()
{
    SRMList *list = srmListCreate();

    if (!list)
        return 0;

    if (srmListGetLength(list) != 0)
        return 1;

    if (!srmListIsEmpty(list))
        return 2;

    if (srmListGetBack(list))
        return 3;

    if (srmListGetFront(list))
        return 4;

    srmListDestoy(list);

    return -1;
}

static int test_02()
{
    SRMList *list = srmListCreate();

    if (!list)
        return 0;

    Int32 num = 7;

    srmListAppendData(list, &num);

    if (srmListGetLength(list) != 1)
        return 1;

    if (srmListIsEmpty(list))
        return 2;

    if (!srmListGetBack(list))
        return 3;

    SRMListItem *item = srmListGetFront(list);

    if (!item)
        return 4;

    Int32 data = *(Int32*)srmListItemGetData(item);

    if (data != 7)
        return 5;

    if (srmListItemGetList(item) != list)
        return 6;

    if (srmListItemGetNext(item))
        return 7;

    if (srmListItemGetPrev(item))
        return 8;

    srmListDestoy(list);

    return -1;
}

static int test_03()
{
    SRMList *list = srmListCreate();

    if (!list)
        return 0;

    Int32 num = 7;

    srmListPrependData(list, &num);

    if (srmListGetLength(list) != 1)
        return 1;

    if (srmListIsEmpty(list))
        return 2;

    if (!srmListGetBack(list))
        return 3;

    SRMListItem *item = srmListGetFront(list);

    if (!item)
        return 4;

    Int32 data = *(Int32*)srmListItemGetData(item);

    if (data != 7)
        return 5;

    if (srmListItemGetList(item) != list)
        return 6;

    if (srmListItemGetNext(item))
        return 7;

    if (srmListItemGetPrev(item))
        return 8;

    srmListDestoy(list);

    return -1;
}

static int test_04()
{
    SRMList *list = srmListCreate();

    if (!list)
        return 0;

    Int32 num = 7;

    srmListInsertData(list, NULL, &num);

    if (srmListGetLength(list) != 1)
        return 1;

    if (srmListIsEmpty(list))
        return 2;

    if (!srmListGetBack(list))
        return 3;

    SRMListItem *item = srmListGetFront(list);

    if (!item)
        return 4;

    Int32 data = *(Int32*)srmListItemGetData(item);

    if (data != 7)
        return 5;

    if (srmListItemGetList(item) != list)
        return 6;

    if (srmListItemGetNext(item))
        return 7;

    if (srmListItemGetPrev(item))
        return 8;

    srmListDestoy(list);

    return -1;
}

static int test_05()
{
    SRMList *list = srmListCreate();

    Int32 nums[100];

    for (Int32 i = 0; i < 100; i++)
    {
        nums[i] = i + 1;
        srmListAppendData(list, &nums[i]);
    }

    if (srmListGetLength(list) != 100)
        return 0;

    Int32 i = 0;

    SRMListForeach(item, list)
    {
        if (*(Int32*)srmListItemGetData(item) != nums[i])
            return i;
        i++;
    }

    if (i != 100)
        return 101;

    srmListDestoy(list);

    return -1;
}

static int runTest(int(*test)(), const char *name)
{
    Int32 res = test();

    if (res == -1)
    {
        SRMDebug("Test PASSED: [%s].", name);
        return 1;
    }

    SRMError("Test FAILED: [%s] (RET CODE %d).", name, res);
    return 0;
}

int main(void)
{
    setenv("SRM_DEBUG", "4", 1);
    SRMLogInit();

    Int32 ret = 1;

    ret = ret && runTest(&test_01, "Create list");
    ret = ret && runTest(&test_02, "Append data");
    ret = ret && runTest(&test_03, "Prepend data");
    ret = ret && runTest(&test_04, "Insert data");
    ret = ret && runTest(&test_05, "Foreach loop");

    return !ret;
}
