#pragma once

#include <cstdint>

namespace sgf
{

/**
 * @brief Severity levels for log messages.
 * Mirrors Boost.Log trivial severity levels exactly.
 */
enum class LogLevel : uint8_t
{
    TRACE,
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL
};

}  // namespace sgf
