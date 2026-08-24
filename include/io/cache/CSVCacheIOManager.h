#pragma once

#include "ICacheIOManager.h"
#include "Int128.h"
#include "LoggerHandler.h"

#include <fstream>
#include <string>

namespace sgf
{

/**
 * @class CSVCacheIOManager
 * @brief Reads and writes one graph's enumeration frequency data in CSV format.
 *
 * File format: header row followed by one data row per motif entry.
 * Columns: motif_number (decimal UInt128), appearances. A graph whose enumeration
 * result is empty (e.g. a star graph has no 5-vertex simple paths at all) writes a
 * header-only file with zero data rows — since the graph's identity lives in the
 * filename rather than in the file's content, an empty file still correctly
 * represents "this graph exists and matched nothing".
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
    explicit CSVCacheIOManager(std::string folder, LoggerHandler logger = LoggerHandler::null());

protected:
    /**
     * @brief Writes one graph's enumeration data to a CSV file at @p full_path.
     *
     * @param data      Enumeration data to serialize.
     * @param full_path Destination file path including the .csv extension.
     * @throws SgfPathExistsException if the file cannot be opened.
     */
    void write_to_file(const EnumerationResult& data, const std::string& full_path) const override;

    /**
     * @brief Reads one graph's enumeration data from a CSV file at @p full_path.
     *
     * @param full_path Source file path including the .csv extension.
     * @return Parsed enumeration data.
     * @throws SgfPathExistsException if the file cannot be opened.
     * @throws GraphConstructionException if any row contains malformed values.
     */
    EnumerationResult read_from_file(const std::string& full_path) const override;

    /**
     * @brief Returns the CSV file extension.
     * @return "csv"
     */
    [[nodiscard]] std::string get_extension() const override;

private:
    static constexpr const char* CSV_COLUMN_MOTIF_NUMBER = "motif_number";
    static constexpr const char* CSV_COLUMN_APPEARANCES = "appearances";

    /**
     * @brief Writes the CSV header row to @p file.
     * @param file Opened output stream.
     */
    static void write_header(std::ofstream& file);

    /**
     * @brief Writes one CSV row per motif entry to @p file.
     * @param data Enumeration data to write.
     * @param file Opened output stream.
     */
    static void write_rows(const EnumerationResult& data, std::ofstream& file);

    /**
     * @brief Converts a UInt128 to its decimal string representation.
     * @param value Value to convert (modified in place during conversion).
     * @return Decimal string.
     */
    static std::string uint128_to_decimal(UInt128 value);

    /**
     * @brief Parses all data rows from @p file into an EnumerationResult.
     * @param file Opened input stream positioned after the header row.
     * @return Parsed enumeration data.
     */
    static EnumerationResult parse_file(std::ifstream& file);

    /**
     * @brief Parses one CSV @p line and inserts the entry into @p data.
     *
     * @param line Comma-separated row: motif_number,appearances.
     * @param data Collection to insert the parsed entry into.
     */
    static void insert_row(const std::string& line, EnumerationResult& data);

    /**
     * @brief Converts a decimal string to a UInt128 value.
     * @param decimal_str Decimal representation of the 128-bit integer.
     * @return Parsed UInt128.
     */
    static UInt128 decimal_to_uint128(const std::string& decimal_str);
};

}  // namespace sgf
