#include "LoggerBundle.h"

namespace sgf
{

LoggerBundle::LoggerBundle(const std::string& log_file_path)
    : m_logger(log_file_path.empty() ? nullptr : std::make_shared<FileLogger>(log_file_path))
    , m_handler(m_logger ? LoggerHandler(m_logger) : LoggerHandler::null())
{
}

LoggerHandler LoggerBundle::handler() const
{
    return m_handler;
}

}  // namespace sgf
