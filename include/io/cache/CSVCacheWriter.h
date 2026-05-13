#pragma once

#include "ICacheWriter.h"
#include "Int128.h"

#include <fstream>
#include <string>

namespace sgf
{

/**
 * @class CSVCacheWriter
 * @brief Writes enumeration frequency data to a CSV cache file.
 *
 * Output format: one row per (graph, motif) pair with three columns:
 * graph_index, motif_number, appearances.
 */
class CSVCacheWriter : public ICacheWriter
{
public:
    /**
     * @brief Constructs a CSVCacheWriter.
     *
     * @param folder        Directory where the CSV file will be written.
     * @param base_filename File name without extension.
     */
    CSVCacheWriter(std::string folder, std::string base_filename);

protected:
    /**
     * @brief Writes enumeration data to a CSV file at @p full_path.
     *
     * @param data      The enumeration data to serialize.
     * @param full_path Destination file path including the .csv extension.
     * @throws SgfPathDoesntExistException if the file cannot be opened for writing.
     */
    void write_to_file(const EnumerationData& data, const std::string& full_path) const override;

    /**
     * @brief Returns the CSV file extension.
     * @return "csv"
     */
    [[nodiscard]] std::string get_extension() const override;

private:
    static constexpr const char* CSV_COLUMN_GRAPH_INDEX = "graph_index";
    static constexpr const char* CSV_COLUMN_MOTIF_NUMBER = "motif_number";
    static constexpr const char* CSV_COLUMN_APPEARANCES = "appearances";

    /**
     * @brief Writes the CSV column header row to @p file.
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
     * @brief Converts a UInt128 value to its decimal string representation.
     * @param value The value to convert (passed by value; modified in place during conversion).
     * @return Decimal string.
     */
    static std::string uint128_to_string(UInt128 value);
};

}  // namespace sgf
