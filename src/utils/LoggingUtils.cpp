#include "LoggingUtils.h"

#include <algorithm>
#include <string>

namespace sgf
{

std::string LoggingUtils::int128_to_string(const __int128_t value)
{
    if (value == 0)
    {
        return "0";
    }

    const bool negative = value < 0;
    __uint128_t temp =
        negative ? -static_cast<__uint128_t>(value) : static_cast<__uint128_t>(value);

    std::string result;

    while (temp > 0)
    {
        result.push_back(static_cast<char>('0' + static_cast<char>(temp % DECIMAL_BASE)));
        temp /= DECIMAL_BASE;
    }

    if (negative)
    {
        result.push_back('-');
    }

    std::reverse(result.begin(), result.end());
    return result;
}

}  // namespace sgf
