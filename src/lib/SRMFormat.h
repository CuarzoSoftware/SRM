#ifndef SRMFORMAT_H
#define SRMFORMAT_H

#include <SRMTypes.h>

struct SRMFormatStruct
{
    UInt32 format;
    UInt64 modifier;
};

SRMListItem *srmFormatsListAddFormat(SRMList *formatsList, UInt32 format, UInt64 modifier);

#endif // SRMFORMAT_H
