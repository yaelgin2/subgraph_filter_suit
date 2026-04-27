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
// NOLINTNEXTLINE(misc-include-cleaner):
#include <boost/log/support/date_time.hpp>
//  Required for Boost.Log formatting and Phoenix operator
// overloads; detected as unused by include-cleaner due to template/macros usage.
#include <array>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
// NOLINTNEXTLINE(misc-include-cleaner):
#include <boost/phoenix/operator.hpp>
// Required for Boost.Phoenix operator expressions used
// internally by Boost.Log sinks/formatters; false positive from include dependency analysis.
#include <boost/smart_ptr/make_shared_object.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <cstddef>
#include <fstream>
#include <ios>
#include <ostream>
#include <stdexcept>
#include <string>

namespace sgf
{

namespace
{

/// Number of log levels.
constexpr size_t LOG_LEVEL_COUNT = 6U;

/**
 * @brief Severity level table indexed by LogLevel ordinal.
 *
 * Order must match the LogLevel enum declaration:
 * TRACE=0, DEBUG=1, INFO=2, WARNING=3, ERROR=4, FATAL=5.
 */
constexpr std::array<boost::log::trivial::severity_level, LOG_LEVEL_COUNT> BOOST_SEVERITY_TABLE = {
    boost::log::trivial::trace,    // TRACE
    boost::log::trivial::debug,    // DEBUG
    boost::log::trivial::info,     // INFO
    boost::log::trivial::warning,  // WARNING
    boost::log::trivial::error,    // ERROR
    boost::log::trivial::fatal,    // FATAL
};

/**
 * @brief Convert LogLevel to Boost trivial severity via lookup table.
 * @param level Input log level.
 * @return Corresponding Boost severity.
 */
boost::log::trivial::severity_level to_boost_level(const LogLevel level)
{
    return BOOST_SEVERITY_TABLE.at(static_cast<size_t>(level));
}

/**
 * @brief Label table indexed by LogLevel ordinal.
 *
 * Order must match the LogLevel enum declaration.
 */
// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
constexpr std::array<const char*, LOG_LEVEL_COUNT> LEVEL_LABEL_TABLE = {
    "[TRACE]",    // TRACE
    "[DEBUG]",    // DEBUG
    "[INFO]",     // INFO
    "[WARNING]",  // WARNING
    "[ERROR]",    // ERROR
    "[FATAL]",    // FATAL
};

/**
 * @brief Convert LogLevel to bracket label string via lookup table.
 * @param level Input log level.
 * @return Label string like [WARNING].
 */
std::string to_level_label(const LogLevel level)
{
    return {LEVEL_LABEL_TABLE.at(static_cast<size_t>(level))};
}

}  // namespace

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
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
    sink->locked_backend()->add_stream(boost::static_pointer_cast<std::ostream>(file_stream));
    sink->set_formatter(expr::stream << expr::format_date_time<boost::posix_time::ptime>(
                                            "TimeStamp", "%Y-%m-%d %H:%M:%S")
                                     << " " << expr::smessage);

    logging::core::get()->add_sink(sink);
    logging::add_common_attributes();
}

void FileLogger::log(const LogLevel level, const std::string& message)
{
    BOOST_LOG_SEV(m_logger, to_boost_level(level)) << to_level_label(level) << " " << message;
}

}  // namespace sgf
