#pragma once

#include "ILogger.h"
#include "LogLevel.h"

#include <memory>
#include <string>

namespace sgf
{

/**
 * @brief Wraps a weak_ptr<ILogger> for safe optional logging.
 *
 * Callers pass a LoggerHandler by value. If the underlying ILogger has
 * been destroyed, log() is a no-op. is_null() lets callers skip
 * expensive message construction when no logger is attached.
 */
class LoggerHandler
{
private:
    std::weak_ptr<ILogger> m_logger;

public:
    /**
     * @brief Construct from a weak pointer to a logger.
     * @param weak_logger Weak reference to the logger; may be empty.
     */
    explicit LoggerHandler(std::weak_ptr<ILogger> weak_logger)
        : m_logger(std::move(weak_logger))
    {}

    /**
     * @brief Log a message if the underlying logger is still alive.
     * @param level Severity level.
     * @param msg Message text.
     */
    void log(const LogLevel level, const std::string& msg) const
    {
        if (auto ptr = m_logger.lock())
        {
            ptr->log(level, msg);
        }
    }

    /**
     * @brief Return true if no live logger is attached.
     * @return True when the weak pointer is expired or empty.
     */
    bool is_null() const
    {
        return m_logger.lock() == nullptr;
    }
};

}  // namespace sgf
