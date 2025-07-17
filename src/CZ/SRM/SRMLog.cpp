#include <CZ/SRM/SRMLog.h>

using namespace CZ;

const CZ::CZLogger &SRMLogger() noexcept
{
    static CZLogger logger { "SRM", "CZ_SRM_LOG_LEVEL" };
    return logger;
}
