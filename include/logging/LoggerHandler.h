#pragma once

#include "ILogger.h"
#include "LogLevel.h"

#include <memory>

namespace sgf
{

class LoggerHandler
{
private:
    std::weak_ptr<ILogger> m_logger;

public:
    explicit LoggerHandler(std::weak_ptr<ILogger> log)
        : m_logger(std::move(log))
    {
    }

    void log(LogLevel level, const std::string& msg) const
    {
        if (auto ptr = m_logger.lock())
        {
            ptr->log(level, msg);
        }
    }

    bool is_null() const
    {
        return m_logger.lock() == nullptr;
    }
};

}  // namespace sgf
