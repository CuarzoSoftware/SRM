#include <CZ/SRM/SRM.h>
#include <algorithm>
#include <array>

using namespace CZ;

std::string_view CZ::SRMModeString(SRMMode type) noexcept
{
    static constexpr const std::array<std::string_view, 5> strings { "Self", "Prime", "Dumb", "CPU", "Unknown" };
    return strings[std::clamp(static_cast<UInt32>(type) - 1U, 0U, 4U)];
}
