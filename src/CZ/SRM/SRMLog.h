#ifndef SRMLOG_H
#define SRMLOG_H

#include <CZ/Core/CZLogger.h>

#define SRMLog SRMLogger()

const CZ::CZLogger &SRMLogger() noexcept;

#endif // SRMLOG_H
