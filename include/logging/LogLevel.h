#pragma once

namespace sgf
{

/**
 * @brief Severity levels for log messages.
 * Mirrors Boost.Log trivial severity levels exactly.
 */
enum class LogLevel
{
    TRACE,
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL
};

}  // namespace sgf
