#ifndef SRMFORMAT_H
#define SRMFORMAT_H

#include <SRMTypes.h>

struct SRMFormatStruct
{
    UInt32 format;
    UInt64 modifier;
};

SRMListItem *srmFormatsListAddFormat(SRMList *formatsList, UInt32 format, UInt64 modifier);
SRMList *srmFormatsListCopy(SRMList *formatsList);
void srmFormatsListDestroy(SRMList **formatsList);

#endif // SRMFORMAT_H
