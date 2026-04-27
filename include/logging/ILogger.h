#pragma once

#include "LogLevel.h"

#include <string>

namespace sgf
{

/**
 * @brief Interface for logging.
 */
class ILogger
{
public:
    ILogger() = default;

    /**
     * @brief Virtual destructor.
     */
    virtual ~ILogger() = default;

    ILogger(const ILogger&) = default;
    ILogger& operator=(const ILogger&) = default;
    ILogger(ILogger&&) = default;
    ILogger& operator=(ILogger&&) = default;

    /**
     * @brief Log message at given level.
     * @param level Severity of message.
     * @param message Text to log.
     */
    virtual void log(LogLevel level, const std::string& message) = 0;
};

}  // namespace sgf
