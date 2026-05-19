#include "LoggingUtils.h"

#include "Int128.h"

#include <algorithm>
#include <string>

namespace sgf
{

std::string LoggingUtils::int128_to_string(UInt128 value)
{
    if (!value)
    {
        return "0";
    }

    std::string result;

    while (value)
    {
        result.push_back(static_cast<char>('0' + static_cast<char>(value % DECIMAL_BASE)));
        value /= DECIMAL_BASE;
    }

    std::reverse(result.begin(), result.end());
    return result;
}

}  // namespace sgf
