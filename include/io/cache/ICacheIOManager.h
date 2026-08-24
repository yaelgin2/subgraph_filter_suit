#pragma once

#include "EnumerationPreprocessManager.h"
#include "IGraphPreprocessor.h"
#include "LoggerHandler.h"

#include <string>
#include <unordered_map>

namespace sgf
{

/**
 * @brief Interface for reading and writing one graph's enumeration frequency data to a cache
 *        file.
 *
 * Subclasses implement format-specific serialization (e.g. CSV, binary). The base class owns
 * path construction and directory creation so that concrete implementations only need to
 * handle the byte-level encoding. Each cache file holds exactly one graph's EnumerationResult;
 * the graph's identity lives in its filename, not in the file's content.
 */
class ICacheIOManager
{
public:
    /**
     * @brief Constructs an ICacheIOManager targeting a specific directory.
     *
     * @param folder  Directory where the cache file will be written.
     * @param logger  Optional logger for diagnostics.
     */
    explicit ICacheIOManager(std::string folder, LoggerHandler logger = LoggerHandler::null());

    /**
     * @brief Default virtual destructor.
     */
    virtual ~ICacheIOManager() = default;

    ICacheIOManager(const ICacheIOManager&) = default;
    ICacheIOManager& operator=(const ICacheIOManager&) = default;
    ICacheIOManager(ICacheIOManager&&) = default;
    ICacheIOManager& operator=(ICacheIOManager&&) = default;

    /**
     * @brief Creates the target directory if absent, then writes @p data to the cache file.
     *
     * @param base_filename File name without extension (e.g. "motif_cache_<graph>").
     * @param data          One graph's enumeration result.
     * @throws SgfPathDoesntExistException if directory creation or file writing fails.
     */
    void write(const std::string& base_filename, const EnumerationResult& data) const;

    /**
     * @brief Reads and merges every cache file in this manager's folder whose base filename
     *        matches @p base_filename, keyed by the graph name derived from each file's name.
     *
     * A file matches when its own base filename equals @p base_filename exactly (one specific
     * graph's file) or starts with "<base_filename>_" (a whole cache category, e.g.
     * "motif_cache", matching every "motif_cache_<graph>" file). The graph name in the
     * returned map comes from the matched filename itself — the part after the category
     * prefix — never from the file's content. Matching files are read individually (file by
     * file) and merged into the returned map.
     *
     * @param base_filename File name (without extension) or cache-category prefix.
     * @return Map from graph name to its enumeration result, merged across matching files.
     * @throws SgfPathDoesntExistException if the folder does not exist.
     */
    std::unordered_map<std::string, EnumerationResult> read(const std::string& base_filename) const;

    /**
     * @brief Checks whether a cache file for @p base_filename already exists on disk.
     *
     * @param base_filename File name without extension (e.g. "motif_cache_graph_0").
     * @return True if the cache file exists.
     */
    [[nodiscard]] bool exists(const std::string& base_filename) const;

protected:
    /**
     * @brief Serializes one graph's @p data to @p full_path.
     *
     * @param data      Enumeration data to serialize.
     * @param full_path Destination file path including the format extension.
     */
    virtual void write_to_file(const EnumerationResult& data,
                               const std::string& full_path) const = 0;

    /**
     * @brief Deserializes one graph's enumeration result from @p full_path.
     *
     * @param full_path Source file path including the format extension.
     * @return Deserialized enumeration result.
     */
    virtual EnumerationResult read_from_file(const std::string& full_path) const = 0;

    /**
     * @brief Returns the file extension used by this format, without a leading dot.
     *
     * @return Extension string (e.g. "csv" or "bin").
     */
    [[nodiscard]] virtual std::string get_extension() const = 0;

private:
    std::string m_folder;
    LoggerHandler m_logger;

    /**
     * @brief Builds the full output path from folder, base filename, and extension.
     *
     * @return Full path string.
     */
    [[nodiscard]] std::string build_full_path(const std::string& base_filename) const;
};

}  // namespace sgf
