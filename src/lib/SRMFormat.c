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
