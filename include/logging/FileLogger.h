#pragma once

#include "ILogger.h"
#include "LogLevel.h"

#include <atomic>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/trivial.hpp>
#include <string>

namespace sgf
{

/**
 * @brief Logs to file using Boost.Log.
 */
class FileLogger : public ILogger
{
public:
    /**
     * @brief Construct logger writing to named file.
     * @param file_name Path to output log file.
     */
    explicit FileLogger(const std::string& file_name);

    /**
     * @brief Log message at given level.
     * @param level Severity of message.
     * @param message Text to log.
     */
    void log(LogLevel level, const std::string& message) override;

private:
    static constexpr const char* LOGGER_ID_KEY = "LoggerId";

    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    static std::atomic<uint32_t> s_next_id;
    uint32_t m_id;
    boost::log::sources::severity_logger<boost::log::trivial::severity_level> m_logger;
};

}  // namespace sgf
