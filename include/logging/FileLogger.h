#pragma once

#include "ILogger.h"
#include "LogLevel.h"

#include <atomic>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
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

    FileLogger(const FileLogger&) = delete;
    FileLogger& operator=(const FileLogger&) = delete;
    FileLogger(FileLogger&&) = delete;
    FileLogger& operator=(FileLogger&&) = delete;

    /**
     * @brief Flush and remove sink from Boost.Log core.
     */
    ~FileLogger() override;

    /**
     * @brief Log message at given level.
     * @param level Severity of message.
     * @param message Text to log.
     */
    void log(LogLevel level, const std::string& message) override;

private:
    using TextSink = boost::log::sinks::synchronous_sink<boost::log::sinks::text_ostream_backend>;

    boost::shared_ptr<TextSink> m_sink;

    static constexpr const char* LOGGER_ID_KEY = "LoggerId";

    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    static std::atomic<bool> s_is_initialized;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    static std::atomic<int> s_next_id;
    uint32_t m_id;

    boost::log::sources::severity_logger<boost::log::trivial::severity_level> m_logger;

    boost::shared_ptr<std::ofstream> m_file_stream;

    static void initialize_logging();
};

}  // namespace sgf
