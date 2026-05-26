#pragma once

#include "IPatternPreprocessor.h"
#include "IPatternWriter.h"
#include "LoggerHandler.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sgf
{

/// All mined patterns with their covering library graph indices.
using PatternOutput = std::vector<PatternPreprocessorResult>;

/// Mapping from pattern base name to the set of library graph indices it covers.
using PatternMapping = std::unordered_map<std::string, std::unordered_set<uint32_t>>;

/**
 * @brief Interface for writing and reading the pattern cache.
 *
 * Owns directory creation and path construction so that concrete subclasses
 * only handle format-specific mapping serialization.
 *
 * write() serializes each pattern graph via the owned IPatternWriter and then
 * writes a mapping file that records which library graph indices each pattern
 * covers. Pattern files are named pattern_<index>_<timestamp>[.<ext>] where
 * the extension is determined by the writer. The mapping file is named
 * pattern_index_<timestamp>.<mapping_ext>.
 *
 * read() deserializes the mapping file and returns the pattern-to-indices map.
 */
class IPatternCacheIOManager
{
public:
    /**
     * @brief Constructs an IPatternCacheIOManager.
     *
     * @param folder  Directory where all output files are written.
     * @param logger  Optional logger for diagnostics.
     * @param writer  Writer used to serialize each pattern graph.
     */
    IPatternCacheIOManager(std::string folder, LoggerHandler logger,
                           std::unique_ptr<IPatternWriter> writer);

    /**
     * @brief Default virtual destructor.
     */
    virtual ~IPatternCacheIOManager() = default;

    IPatternCacheIOManager(const IPatternCacheIOManager&) = delete;
    IPatternCacheIOManager& operator=(const IPatternCacheIOManager&) = delete;
    IPatternCacheIOManager(IPatternCacheIOManager&&) = default;
    IPatternCacheIOManager& operator=(IPatternCacheIOManager&&) = default;

    /**
     * @brief Writes each pattern graph and the pattern→graph-index mapping.
     *
     * Creates @p folder if absent, then for each pattern in @p patterns writes
     * the BoostGraph using the owned IPatternWriter and records the covering
     * graph index set in the mapping file.
     *
     * @param patterns  Mined patterns with their covering library graph indices.
     * @param timestamp Unique run identifier appended to every output filename.
     * @throws SgfPathExistsException if any file cannot be opened or written.
     */
    void write(const PatternOutput& patterns, const std::string& timestamp) const;

    /**
     * @brief Reads the pattern→graph-index mapping from a previous write() call.
     *
     * @param timestamp Timestamp used when write() was called.
     * @return Map from pattern base name to the set of library graph indices it covers.
     * @throws SgfPathExistsException if the mapping file cannot be opened.
     */
    PatternMapping read(const std::string& timestamp) const;

protected:
    /**
     * @brief Serializes @p mapping to the file at @p full_path.
     *
     * @param mapping   Pattern-to-indices mapping to persist.
     * @param full_path Destination file path including the format extension.
     * @throws SgfPathExistsException if the file cannot be opened or written.
     */
    virtual void write_mapping_to_file(const PatternMapping& mapping,
                                       const std::string& full_path) const = 0;

    /**
     * @brief Deserializes a PatternMapping from @p full_path.
     *
     * @param full_path Source file path including the format extension.
     * @return Parsed mapping from pattern base name to covering graph indices.
     * @throws SgfPathExistsException if the file cannot be opened.
     */
    virtual PatternMapping read_mapping_from_file(const std::string& full_path) const = 0;

    /**
     * @brief Returns the mapping file extension, without a leading dot.
     * @return Extension string (e.g. "csv").
     */
    [[nodiscard]] virtual std::string get_mapping_extension() const = 0;

private:
    std::string m_folder;
    LoggerHandler m_logger;
    std::unique_ptr<IPatternWriter> m_writer;

    [[nodiscard]] std::string build_pattern_base_name(uint32_t index,
                                                       const std::string& timestamp) const;
    [[nodiscard]] std::string build_pattern_full_path(uint32_t index,
                                                       const std::string& timestamp) const;
    [[nodiscard]] std::string build_mapping_path(const std::string& timestamp) const;
    void write_single_pattern(const PatternPreprocessorResult& result, uint32_t index,
                               const std::string& timestamp, PatternMapping& mapping) const;
};

}  // namespace sgf
