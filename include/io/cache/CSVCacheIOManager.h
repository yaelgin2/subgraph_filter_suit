#pragma once

#include "ICacheIOManager.h"
#include "Int128.h"

#include <fstream>
#include <string>

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
     * @brief Constructs a CSVCacheIOManager targeting a specific directory and base filename.
     *
     * @param folder        Directory of the CSV file.
     * @param base_filename File name without extension.
     */
    CSVCacheIOManager(std::string folder, std::string base_filename);

protected:
    /**
     * @brief Writes enumeration data to a CSV file at @p full_path.
     *
     * @param data      Enumeration data to serialize.
     * @param full_path Destination file path including the .csv extension.
     * @throws SgfPathDoesntExistException if the file cannot be opened.
     */
    void write_to_file(const EnumerationData& data, const std::string& full_path) const override;

    /**
     * @brief Reads enumeration data from a CSV file at @p full_path.
     *
     * @param full_path Source file path including the .csv extension.
     * @return Parsed enumeration data.
     * @throws SgfPathDoesntExistException if the file cannot be opened.
     * @throws GraphConstructionException if any row contains malformed values.
     */
    EnumerationData read_from_file(const std::string& full_path) const override;

    /**
     * @brief Returns the CSV file extension.
     * @return "csv"
     */
    [[nodiscard]] std::string get_extension() const override;

private:
    static constexpr const char* CSV_COLUMN_GRAPH_INDEX = "graph_index";
    static constexpr const char* CSV_COLUMN_MOTIF_NUMBER = "motif_number";
    static constexpr const char* CSV_COLUMN_APPEARANCES = "appearances";
    static constexpr size_t MAX_GRAPH_INDEX = 1'000'000U;

    /**
     * @brief Writes the CSV header row to @p file.
     * @param file Opened output stream.
     */
    static void write_header(std::ofstream& file);

    /**
     * @brief Writes one CSV row per (graph, motif) pair to @p file.
     *
     * @param data Enumeration data indexed by graph position.
     * @param file Opened output stream.
     */
    static void write_rows(const EnumerationData& data, std::ofstream& file);

    /**
     * @brief Converts a UInt128 to its decimal string representation.
     * @param value Value to convert (modified in place during conversion).
     * @return Decimal string.
     */
    static std::string uint128_to_decimal(UInt128 value);

    /**
     * @brief Parses all data rows from @p file into an EnumerationData collection.
     * @param file Opened input stream positioned after the header row.
     * @return Parsed enumeration data.
     */
    static EnumerationData parse_file(std::ifstream& file);

    /**
     * @brief Parses one CSV @p line and inserts the entry into @p data.
     *
     * Resizes @p data if the parsed graph_index exceeds the current size.
     *
     * @param line Comma-separated row: graph_index,motif_number,appearances.
     * @param data Collection to insert the parsed entry into.
     * @throws GraphConstructionException if graph_index exceeds MAX_GRAPH_INDEX.
     */
    static void insert_row(const std::string& line, EnumerationData& data);

    /**
     * @brief Converts a decimal string to a UInt128 value.
     * @param decimal_str Decimal representation of the 128-bit integer.
     * @return Parsed UInt128.
     */
    static UInt128 decimal_to_uint128(const std::string& decimal_str);
};

}  // namespace sgf
