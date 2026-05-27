#pragma once

#include <fstream>
#include <mutex>
#include <string>

namespace sgf
{

/**
 * @brief Writes subgraph match results to a file, one match per line.
 *
 * Opens the output file on construction and closes it on destruction.
 * Thread-safe: concurrent calls to write_match are serialised by an
 * internal mutex.
 *
 * Subclasses may override write_match to redirect output (e.g. for testing).
 * They must call the protected default constructor instead of the file-path one.
 */
class MatchOutputWriter
{
public:
    /**
     * @brief Opens the output file for writing.
     * @param file_path Path to the file that will receive match lines.
     */
    explicit MatchOutputWriter(const std::string& file_path);

    /**
     * @brief Closes the output file.
     */
    virtual ~MatchOutputWriter();

    MatchOutputWriter(const MatchOutputWriter&) = delete;
    MatchOutputWriter& operator=(const MatchOutputWriter&) = delete;
    MatchOutputWriter(MatchOutputWriter&&) = delete;
    MatchOutputWriter& operator=(MatchOutputWriter&&) = delete;

    /**
     * @brief Appends a match line to the output file.
     * @param match Formatted match string to write.
     */
    virtual void write_match(const std::string& match);

protected:
    /**
     * @brief Default constructor for subclasses that do not write to a file.
     */
    MatchOutputWriter() = default;

private:
    std::ofstream m_file;
    mutable std::mutex m_mutex;
};

}  // namespace sgf
