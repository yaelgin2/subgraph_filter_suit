#pragma once

#include "LogLevel.h"
#include "LoggerHandler.h"

/**
 * @brief Emit a DEBUG-level log in debug builds; compiles to nothing in release.
 *
 * NDEBUG is defined by CMake for Release/RelWithDebInfo configurations, so these
 * calls are fully elided in production with zero runtime overhead.
 *
 * Usage:
 * @code
 *   SGF_DEBUG_LOG(m_logger, "entered foo: x=" + std::to_string(x));
 * @endcode
 */
#ifndef NDEBUG
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define SGF_DEBUG_LOG(logger, msg) (logger).log(sgf::LogLevel::DEBUG, (msg))
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define SGF_DEBUG_LOG(logger, msg) static_cast<void>(0)
#endif
