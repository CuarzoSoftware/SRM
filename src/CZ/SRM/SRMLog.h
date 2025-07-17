#ifndef SRMLOG_H
#define SRMLOG_H

#include <CZ/CZLogger.h>

#define SRMLog SRMLogger()

const CZ::CZLogger &SRMLogger() noexcept;

#endif // SRMLOG_H
