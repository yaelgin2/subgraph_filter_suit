#pragma once

#include "ILogger.h"
#include "LogLevel.h"

#include <memory>

namespace sgf
{

class LoggerHandler
{
private:
    std::weak_ptr<ILogger> logger;

public:
    explicit LoggerHandler(std::weak_ptr<ILogger> log)
        : logger(std::move(log))
    {
    }

    void log(LogLevel level, const std::string& msg) const
    {
        if (auto ptr = logger.lock())
        {
            ptr->log(level, msg);
        }
    }

    bool is_null() const
    {
        return logger.lock() == nullptr;
    }
};

}  // namespace sgf
