#include "FileLogger.h"

#include "LogLevel.h"
#include "SgfPathDoesntExistException.h"

#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/log/core/core.hpp>
#include <boost/log/expressions/formatters/date_time.hpp>
#include <boost/log/expressions/formatters/stream.hpp>
#include <boost/log/expressions/message.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/sources/severity_feature.hpp>
#include <boost/log/support/date_time.hpp>  // NOLINT(misc-include-cleaner) — effect include: registers ptime formatter generator traits
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/phoenix/operator.hpp>  // NOLINT(misc-include-cleaner)
#include <boost/smart_ptr/make_shared_object.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <fstream>
#include <ios>
#include <string>

namespace sgf
{

namespace
{

/**
 * @brief Convert LogLevel to Boost trivial severity.
 * @param level Input log level.
 * @return Corresponding Boost severity.
 */
boost::log::trivial::severity_level to_boost_level(const LogLevel level)
{
    switch (level)
    {
    case LogLevel::TRACE:
        return boost::log::trivial::trace;
    case LogLevel::DEBUG:
        return boost::log::trivial::debug;
    case LogLevel::INFO:
        return boost::log::trivial::info;
    case LogLevel::WARNING:
        return boost::log::trivial::warning;
    case LogLevel::ERROR:
        return boost::log::trivial::error;
    case LogLevel::FATAL:
        return boost::log::trivial::fatal;
    }
    return boost::log::trivial::info;
}

/**
 * @brief Convert LogLevel to bracket label string.
 * @param level Input log level.
 * @return Label string like [WARNING].
 */
std::string to_level_label(const LogLevel level)
{
    switch (level)
    {
    case LogLevel::TRACE:
        return "[TRACE]";
    case LogLevel::DEBUG:
        return "[DEBUG]";
    case LogLevel::INFO:
        return "[INFO]";
    case LogLevel::WARNING:
        return "[WARNING]";
    case LogLevel::ERROR:
        return "[ERROR]";
    case LogLevel::FATAL:
        return "[FATAL]";
    }
    return "[UNKNOWN]";
}

}  // namespace

FileLogger::FileLogger(const std::string& file_name)
{
    namespace logging = boost::log;
    namespace sinks = boost::log::sinks;
    namespace expr = boost::log::expressions;

    const boost::shared_ptr<std::ofstream> file_stream =
        boost::make_shared<std::ofstream>(file_name, std::ios::app);
    if (!file_stream->is_open())
    {
        throw SgfPathDoesntExistException("Failed to open log file: " + file_name);
    }

    using TextSink = sinks::synchronous_sink<sinks::text_ostream_backend>;
    const boost::shared_ptr<TextSink> sink = boost::make_shared<TextSink>();
    sink->locked_backend()->add_stream(file_stream);
    sink->set_formatter(expr::stream << expr::format_date_time<boost::posix_time::ptime>(
                                            "TimeStamp", "%Y-%m-%d %H:%M:%S")
                                     << " " << expr::smessage);

    logging::core::get()->add_sink(sink);
    logging::add_common_attributes();
}

void FileLogger::log(const LogLevel level, const std::string& message)
{
    const std::string labeled_message = to_level_label(level) + " " + message;
    BOOST_LOG_SEV(m_logger, to_boost_level(level)) << labeled_message;
}

}  // namespace sgf
