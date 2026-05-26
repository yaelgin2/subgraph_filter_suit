#pragma once

#include "FilteringUtils.h"
#include "LoggerHandler.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace sgf
{

/**
 * @brief Interface for reading and writing filter results paired with library graph filenames.
 *
 * Subclasses implement format-specific serialization (e.g. JSON, CSV).
 * The base class owns path construction and directory creation so that concrete
 * implementations only need to handle the byte-level encoding.
 */
class IFilterIOManager
{
public:
    /**
     * @brief Constructs an IFilterIOManager targeting a directory.
     *
     * @param folder  Directory where the output file will be written.
     * @param logger  Optional logger for diagnostics.
     */
    explicit IFilterIOManager(std::string folder, LoggerHandler logger = LoggerHandler::null());

    /**
     * @brief Default virtual destructor.
     */
    virtual ~IFilterIOManager() = default;

    IFilterIOManager(const IFilterIOManager&) = default;
    IFilterIOManager& operator=(const IFilterIOManager&) = default;
    IFilterIOManager(IFilterIOManager&&) = default;
    IFilterIOManager& operator=(IFilterIOManager&&) = default;

    /**
     * @brief Creates the target directory if absent, then writes filter results to file.
     *
     * The full output path is: @c folder / @c base_filename + "." + get_extension().
     * Each entry pairs @p filenames[i] with @p results[i].
     *
     * @param base_filename Stem of the output file (no extension).
     * @param filenames     Library graph filenames, one per library graph.
     * @param results       Filter result per library graph; true = pruned, false = survives.
     * @throws SgfPathExistsException if directory creation or file writing fails.
     */
    void write(const std::string& base_filename, const std::vector<std::string>& filenames,
               const FilterResult& results) const;

    /**
     * @brief Reads filter results from the file written by a previous write() call.
     *
     * @param base_filename Stem of the file to read (no extension).
     * @return Map from library graph filename to its filter result (true = pruned).
     * @throws SgfPathExistsException if the file cannot be opened.
     * @throws GraphConstructionException if the file content is malformed.
     */
    std::unordered_map<std::string, bool> read(const std::string& base_filename) const;

protected:
    /**
     * @brief Serializes filter results to @p full_path in the concrete format.
     *
     * @param filenames Library graph filenames.
     * @param results   Filter result per library graph.
     * @param full_path Destination file path including the format extension.
     * @throws SgfPathExistsException if the file cannot be opened or written.
     */
    virtual void write_to_file(const std::vector<std::string>& filenames,
                               const FilterResult& results,
                               const std::string& full_path) const = 0;

    /**
     * @brief Deserializes filter results from @p full_path in the concrete format.
     *
     * @param full_path Source file path including the format extension.
     * @return Map from library graph filename to its filter result.
     * @throws SgfPathExistsException if the file cannot be opened.
     * @throws GraphConstructionException if the file content is malformed.
     */
    virtual std::unordered_map<std::string, bool>
    read_from_file(const std::string& full_path) const = 0;

    /**
     * @brief Returns the file extension used by this format, without a leading dot.
     *
     * @return Extension string (e.g. "json" or "csv").
     */
    [[nodiscard]] virtual std::string get_extension() const = 0;

private:
    std::string m_folder;
    LoggerHandler m_logger;

    /**
     * @brief Builds the full output path from folder, base filename, and extension.
     *
     * @param base_filename File name without extension.
     * @return Full path string.
     */
    [[nodiscard]] std::string build_full_path(const std::string& base_filename) const;
};

}  // namespace sgf
