#include <SRMLog.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

#define BRELN "\n"

static int level = 0;


void SRMLogInit()
{
    char *env = getenv("SRM_DEBUG");

    if(env)
        level = atoi(env);
    else
        level = 0;
}


void SRMFatal(const char *format, ...)
{
    if(level >= 1)
    {
        printf("%sSRM fatal:%s ", KRED, KNRM);
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        printf(BRELN);
    }
}

void SRMError(const char *format, ...)
{
    if(level >= 2)
    {
        printf("%sSRM error:%s ", KRED, KNRM);
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        printf(BRELN);
    }
}

void SRMWarning(const char *format, ...)
{
    if(level >= 3)
    {
        printf("%sSRM warning:%s ", KYEL, KNRM);
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        printf(BRELN);
    }
}

void SRMDebug(const char *format, ...)
{
    if(level >= 4)
    {
        printf("%sSRM debug:%s ", KGRN, KNRM);
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        printf(BRELN);
    }
}

void SRMLog(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf(BRELN);
}
