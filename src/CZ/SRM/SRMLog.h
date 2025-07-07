#ifndef SRMLOG_H
#define SRMLOG_H

#include <source_location>

#if DOXYGEN
#define FORMAT_CHECK
#define FORMAT_CHECK2
#else
#define FORMAT_CHECK __attribute__((format(printf, 1, 2)))
#define FORMAT_CHECK2 __attribute__((format(printf, 2, 3)))
#endif

namespace CZ
{
void SRMLogInit() noexcept;
int SRMLogLevel() noexcept;

/// Prints general messages independent of the value of **SRM_DEBUG**.
FORMAT_CHECK void SRMLog(const char *format, ...) noexcept;

/// Reports an unrecoverable error. **SRM_DEBUG** >= 1.
FORMAT_CHECK void SRMFatal(const char *format, ...) noexcept;
FORMAT_CHECK2 void SRMFatal(const std::source_location location, const char *format, ...) noexcept;

/// Reports a nonfatal error. **SRM_DEBUG** >= 2.
FORMAT_CHECK void SRMError(const char *format, ...) noexcept;
FORMAT_CHECK2 void SRMError(const std::source_location location, const char *format, ...) noexcept;

/// Messages that report a risk for the compositor. **SRM_DEBUG** >= 3.
FORMAT_CHECK void SRMWarning(const char *format, ...) noexcept;
FORMAT_CHECK2 void SRMWarning(const std::source_location location, const char *format, ...) noexcept;

/// Debugging messages. **SRM_DEBUG** >= 4.
FORMAT_CHECK void SRMDebug(const char *format, ...) noexcept;
FORMAT_CHECK2 void SRMDebug(const std::source_location location, const char *format, ...) noexcept;
}

#endif // SRMLOG_H
