#pragma once

#include "IPatternCacheIOManager.h"
#include "LoggerHandler.h"

#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace sgf
{

/**
 * @class CSVPatternCacheIOManager
 * @brief Reads and writes the pattern→graph-index mapping in CSV format.
 *
 * The mapping file is named pattern_index_<timestamp>.csv and is written to
 * the folder supplied at construction. Each row records one (pattern, graph)
 * pair:
 *
 * @code
 * pattern_name,graph_index
 * pattern_0_1748300000.graphml,0
 * pattern_0_1748300000.graphml,3
 * pattern_1_1748300000.graphml,1
 * @endcode
 *
 * Pattern graph files are written by the IPatternWriter supplied to write().
 * The mapping key is the full filename (base name + extension), so readers can
 * reconstruct the file path from the cache folder alone without knowing the writer type.
 */
class CSVPatternCacheIOManager : public IPatternCacheIOManager
{
public:
    /**
     * @brief Constructs a CSVPatternCacheIOManager.
     *
     * @param folder  Directory where pattern files and the mapping file are written.
     * @param logger  Optional logger for diagnostics.
     */
    CSVPatternCacheIOManager(std::string folder, LoggerHandler logger);

    /**
     * @brief Default virtual destructor.
     */
    ~CSVPatternCacheIOManager() override = default;

    CSVPatternCacheIOManager(const CSVPatternCacheIOManager&) = delete;
    CSVPatternCacheIOManager& operator=(const CSVPatternCacheIOManager&) = delete;
    CSVPatternCacheIOManager(CSVPatternCacheIOManager&&) = default;
    CSVPatternCacheIOManager& operator=(CSVPatternCacheIOManager&&) = default;

protected:
    /**
     * @brief Writes @p mapping to @p full_path as CSV rows.
     *
     * @param mapping   Pattern-to-indices mapping to serialize.
     * @param full_path Destination file path including the .csv extension.
     * @throws SgfPathExistsException if the file cannot be opened or written.
     */
    void write_mapping_to_file(const PatternMapping& mapping,
                               const std::string& full_path) const override;

    /**
     * @brief Reads a PatternMapping from the CSV file at @p full_path.
     *
     * @param full_path Source CSV file path.
     * @return Parsed mapping from pattern base name to covering graph indices.
     * @throws SgfPathExistsException if the file cannot be opened.
     * @throws GraphConstructionException if any row is malformed.
     */
    PatternMapping read_mapping_from_file(const std::string& full_path) const override;

    /**
     * @brief Returns "csv".
     * @return Extension string "csv".
     */
    [[nodiscard]] std::string get_mapping_extension() const override;

private:
    static constexpr const char* CSV_COL_PATTERN_NAME = "pattern_name";
    static constexpr const char* CSV_COL_GRAPH_INDEX = "graph_index";

    /**
     * @brief Writes the CSV header row to @p file.
     * @param file Opened output stream.
     */
    static void write_header(std::ofstream& file);

    /**
     * @brief Writes one row per (pattern_name, graph_index) pair to @p file.
     *
     * @param mapping Pattern mapping to serialize.
     * @param file    Opened output stream.
     */
    static void write_rows(const PatternMapping& mapping, std::ofstream& file);

    /**
     * @brief Parses all data rows from @p file into a PatternMapping.
     *
     * Skips the header row before reading data rows.
     *
     * @param file Opened input stream positioned at the beginning of the file.
     * @return Parsed mapping.
     * @throws GraphConstructionException on malformed values.
     */
    static PatternMapping parse_file(std::ifstream& file);

    /**
     * @brief Parses one CSV row and inserts the entry into @p mapping.
     *
     * @param line    Comma-separated row: pattern_name,graph_index.
     * @param mapping Collection to insert the parsed entry into.
     * @throws std::invalid_argument if graph_index is not a valid integer.
     * @throws std::out_of_range if graph_index overflows uint32_t.
     */
    static void insert_row(const std::string& line, PatternMapping& mapping);
};

}  // namespace sgf
