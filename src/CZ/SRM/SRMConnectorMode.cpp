#include <CZ/SRM/SRMConnectorMode.h>
#include <CZ/SRM/SRMConnector.h>

using namespace CZ;

bool SRMConnectorMode::isPreferred() const noexcept
{
    return connector()->preferredMode() == this;
}
