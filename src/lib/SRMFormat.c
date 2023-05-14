#include <SRMFormat.h>
#include <SRMList.h>
#include <stdlib.h>

SRMListItem *srmFormatsListAddFormat(SRMList *formatsList, UInt32 format, UInt64 modifier)
{
    SRMFormat *fmt = malloc(sizeof(SRMFormat));
    fmt->format = format;
    fmt->modifier = modifier;
    return srmListAppendData(formatsList, fmt);
}

void srmFormatsListDestroy(SRMList **formatsList)
{
    if (*formatsList)
    {
        while (!srmListIsEmpty(*formatsList))
            free(srmListPopBack(*formatsList));

        srmListDestoy(*formatsList);
        *formatsList = NULL;
    }
}

SRMList *srmFormatsListCopy(SRMList *formatsList)
{
    SRMList *newList = srmListCreate();

    SRMListForeach(fmtIt, formatsList)
    {
        SRMFormat *fmt = srmListItemGetData(fmtIt);
        srmFormatsListAddFormat(newList, fmt->format, fmt->modifier);
    }

    return newList;
}
