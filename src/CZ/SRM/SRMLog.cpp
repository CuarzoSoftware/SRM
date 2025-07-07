#include <CZ/SRM/SRMLog.h>
#include <algorithm>
#include <cstdlib>
#include <stdio.h>
#include <stdarg.h>

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

#define BRELN "\n"

int level = 0;

void CZ::SRMLogInit() noexcept
{
    char *env = getenv("SRM_DEBUG");

    if (env)
        level = std::clamp(atoi(env), 0, 4);
    else
        level = 0;
}

void CZ::SRMFatal(const char *format, ...) noexcept
{
    if (level >= 1)
    {
        fprintf(stderr, "%sSRM fatal:%s ", KRED, KNRM);
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
        fprintf(stderr, BRELN);
    }
}

void CZ::SRMFatal(const std::source_location location, const char *format, ...) noexcept
{
    if (level >= 1)
    {
        fprintf(stderr, "%sSRM fatal:%s ", KRED, KNRM);
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
        fprintf(stderr, " %s %s(%d:%d) %s", location.function_name(), location.file_name(), location.line(), location.column(), BRELN);
    }
}

void CZ::SRMError(const char *format, ...) noexcept
{
    if (level >= 2)
    {
        fprintf(stderr, "%sSRM error:%s ", KRED, KNRM);
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
        fprintf(stderr, BRELN);
    }
}

void CZ::SRMError(const std::source_location location, const char *format, ...) noexcept
{
    if (level >= 2)
    {
        fprintf(stderr, "%sSRM error:%s ", KRED, KNRM);
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
        fprintf(stderr, " %s %s(%d:%d) %s", location.function_name(), location.file_name(), location.line(), location.column(), BRELN);
    }
}

void CZ::SRMWarning(const char *format, ...) noexcept
{
    if (level >= 3)
    {
        printf("%sSRM warning:%s ", KYEL, KNRM);
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        printf(BRELN);
    }
}

void CZ::SRMWarning(const std::source_location location, const char *format, ...) noexcept
{
    if (level >= 3)
    {
        printf("%sSRM warning:%s ", KYEL, KNRM);
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        fprintf(stdout, " %s %s(%d:%d) %s", location.function_name(), location.file_name(), location.line(), location.column(), BRELN);
    }
}

void CZ::SRMDebug(const char *format, ...) noexcept
{
    if (level >= 4)
    {
        printf("%sSRM debug:%s ", KGRN, KNRM);
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        printf(BRELN);
    }
}

void CZ::SRMDebug(const std::source_location location, const char *format, ...) noexcept
{
    if (level >= 4)
    {
        printf("%sSRM debug:%s ", KGRN, KNRM);
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        fprintf(stdout, " %s %s(%d:%d) %s", location.function_name(), location.file_name(), location.line(), location.column(), BRELN);
    }
}

void CZ::SRMLog(const char *format, ...) noexcept
{
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf(BRELN);
}

int CZ::SRMLogLevel() noexcept
{
    return level;
}
