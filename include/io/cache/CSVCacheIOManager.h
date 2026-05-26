#pragma once

#include "ICacheIOManager.h"
#include "Int128.h"
#include "LoggerHandler.h"

#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace sgf
{

/**
 * @class CSVCacheIOManager
 * @brief Reads and writes enumeration frequency data in CSV format.
 *
 * File format: header row followed by one data row per (graph, motif) pair.
 * Columns: graph_index, motif_number (decimal UInt128), appearances.
 */
class CSVCacheIOManager : public ICacheIOManager
{
public:
    /**
     * @brief Constructs a CSVCacheIOManager targeting a specific directory.
     *
     * @param folder  Directory of the CSV file.
     * @param logger  Optional logger for diagnostics.
     */
    CSVCacheIOManager(std::string folder, LoggerHandler logger = LoggerHandler::null());

protected:
    /**
     * @brief Writes enumeration data to a CSV file at @p full_path.
     *
     * @param data      Enumeration data to serialize.
     * @param full_path Destination file path including the .csv extension.
     * @throws SgfPathExistsException if the file cannot be opened.
     */
    void write_to_file(const EnumerationResultVector& data,
                       const std::vector<std::string>& graph_names,
                       const std::string& full_path) const override;

    /**
     * @brief Reads enumeration data from a CSV file at @p full_path.
     *
     * @param full_path Source file path including the .csv extension.
     * @return Parsed enumeration data.
     * @throws SgfPathExistsException if the file cannot be opened.
     * @throws GraphConstructionException if any row contains malformed values.
     */
    std::unordered_map<std::string, EnumerationResult>
    read_from_file(const std::string& full_path) const override;

    /**
     * @brief Returns the CSV file extension.
     * @return "csv"
     */
    [[nodiscard]] std::string get_extension() const override;

private:
    static constexpr const char* CSV_COLUMN_GRAPH_NAME = "graph_name";
    static constexpr const char* CSV_COLUMN_MOTIF_NUMBER = "motif_number";
    static constexpr const char* CSV_COLUMN_APPEARANCES = "appearances";

    /**
     * @brief Writes the CSV header row to @p file.
     * @param file Opened output stream.
     */
    static void write_header(std::ofstream& file);

    /**
     * @brief Writes one CSV row per (graph, motif) pair to @p file.
     *
     * @param data        Enumeration data indexed by position.
     * @param graph_names Names aligned with @p data.
     * @param file        Opened output stream.
     */
    static void write_rows(const EnumerationResultVector& data,
                           const std::vector<std::string>& graph_names, std::ofstream& file);

    /**
     * @brief Converts a UInt128 to its decimal string representation.
     * @param value Value to convert (modified in place during conversion).
     * @return Decimal string.
     */
    static std::string uint128_to_decimal(UInt128 value);

    /**
     * @brief Parses all data rows from @p file into an EnumerationResultVector collection.
     * @param file Opened input stream positioned after the header row.
     * @return Parsed enumeration data.
     */
    static std::unordered_map<std::string, EnumerationResult> parse_file(std::ifstream& file);

    /**
     * @brief Parses one CSV @p line and inserts the entry into @p data.
     *
     * Resizes @p data if the parsed graph_index exceeds the current size.
     *
     * @param line Comma-separated row: graph_index,motif_number,appearances.
     * @param data Collection to insert the parsed entry into.
     * @throws GraphConstructionException if graph_index exceeds MAX_GRAPH_INDEX.
     */
    static void insert_row(const std::string& line,
                           std::unordered_map<std::string, EnumerationResult>& data);

    /**
     * @brief Converts a decimal string to a UInt128 value.
     * @param decimal_str Decimal representation of the 128-bit integer.
     * @return Parsed UInt128.
     */
    static UInt128 decimal_to_uint128(const std::string& decimal_str);
};

}  // namespace sgf
