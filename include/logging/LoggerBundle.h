#pragma once

#include "FileLogger.h"
#include "LoggerHandler.h"

#include <memory>
#include <string>

namespace sgf
{

/**
 * @brief Owns a FileLogger and its LoggerHandler together to manage lifetime.
 *
 * LoggerHandler holds a weak_ptr, so the FileLogger must outlive any handler
 * copies derived from it. LoggerBundle keeps the shared_ptr and the handler
 * together so callers cannot accidentally let the logger expire.
 *
 * Constructing with an empty path produces a no-op (null) logger so callers
 * that do not need logging can pass an empty string without causing a file-open
 * error.
 */
class LoggerBundle
{
public:
    /**
     * @brief Constructs a LoggerBundle.
     *
     * If @p log_file_path is empty, the bundle wraps a null logger that
     * silently drops all log calls. Otherwise, opens the file and wraps it.
     *
     * @param log_file_path Path to the log file; empty for no logging.
     * @throws SgfPathExistsException if the file cannot be opened.
     */
    explicit LoggerBundle(const std::string& log_file_path);

    /**
     * @brief Returns a copy of the LoggerHandler wrapping this bundle's logger.
     *
     * The returned handler remains valid only while this LoggerBundle is alive.
     *
     * @return LoggerHandler backed by this bundle's FileLogger.
     */
    [[nodiscard]] LoggerHandler handler() const;

private:
    std::shared_ptr<FileLogger> m_logger;  ///< Owned logger; null when no log path was given.
    LoggerHandler m_handler;               ///< Handler wrapping m_logger.
};

}  // namespace sgf
