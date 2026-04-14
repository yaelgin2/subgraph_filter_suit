#pragma once

#include "ILogger.h"
#include "LogLevel.h"

#include <memory>

namespace sgf
{

class Loggable
{
protected:
    std::weak_ptr<ILogger> logger;

    void log(LogLevel level, const std::string& msg) const
    {
        if (std::shared_ptr<ILogger> ptr = logger.lock())
        {
            ptr->log(level, msg);
        }
    }

public:
    explicit Loggable(std::weak_ptr<ILogger> log)
        : logger(std::move(log))
    {
    }
};

}  // namespace sgf
