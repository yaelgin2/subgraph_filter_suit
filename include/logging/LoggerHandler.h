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
    explicit LoggerRef(std::weak_ptr<ILogger> log)
        : logger(std::move(log))
    {}

    void log(LogLevel level, const std::string& msg) const
    {
        if (auto ptr = logger.lock())
        {
            ptr->log(level, msg);
        }
    }
};

}  // namespace sgf
