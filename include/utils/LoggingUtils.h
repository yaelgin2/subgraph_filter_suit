#pragma once

#include <cstdint>
#include <string>

namespace sgf
{

/**
 * @brief Utility functions for logging-related type conversions.
 */
class LoggingUtils
{
public:
    /**
     * @brief Convert a 128-bit signed integer to its decimal string representation.
     * @param value The value to convert.
     * @return Decimal string, with leading '-' for negative values.
     */
    static std::string int128_to_string(__int128_t value);

private:
    /**
     * @brief Base used to convert a 128-bit value to decimal digits.
     */
    static constexpr uint32_t DECIMAL_BASE = 10U;
};

}  // namespace sgf
