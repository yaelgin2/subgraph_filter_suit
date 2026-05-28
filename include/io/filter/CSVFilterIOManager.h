#pragma once

#include "FilteringUtils.h"
#include "IFilterIOManager.h"
#include "LoggerHandler.h"

#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace sgf
{

/**
 * @brief Reads and writes filter results as a CSV file with one row per library graph.
 *
 * File format (no header row):
 * @code
 * graph_a.graphml,1
 * graph_b.graphml,0
 * @endcode
 * 1 = pruned, 0 = survives.
 */
class CSVFilterIOManager : public IFilterIOManager
{
public:
    /**
     * @brief Constructs a CSVFilterIOManager.
     *
     * @param folder  Directory where the output file will be written.
     * @param logger  Optional logger for diagnostics.
     */
    explicit CSVFilterIOManager(std::string folder, LoggerHandler logger = LoggerHandler::null());

    /**
     * @brief Default virtual destructor.
     */
    ~CSVFilterIOManager() override = default;

    CSVFilterIOManager(const CSVFilterIOManager&) = default;
    CSVFilterIOManager& operator=(const CSVFilterIOManager&) = default;
    CSVFilterIOManager(CSVFilterIOManager&&) = default;
    CSVFilterIOManager& operator=(CSVFilterIOManager&&) = default;

protected:
    /**
     * @brief Writes filter results as CSV rows to @p full_path.
     *
     * @param filenames Library graph filenames.
     * @param results   Filter result per library graph.
     * @param full_path Destination file path.
     * @throws SgfPathExistsException if the file cannot be opened or written.
     */
    void write_to_file(const std::vector<std::string>& filenames, const FilterResult& results,
                       const std::string& full_path) const override;

    /**
     * @brief Reads filter results from the CSV file at @p full_path.
     *
     * @param full_path Source CSV file path.
     * @return Map from library graph filename to its filter result.
     * @throws SgfPathExistsException if the file cannot be opened.
     * @throws GraphConstructionException if any row is malformed.
     */
    std::unordered_map<std::string, bool>
    read_from_file(const std::string& full_path) const override;

    /**
     * @brief Returns "csv".
     * @return Extension string "csv".
     */
    [[nodiscard]] std::string get_extension() const override;

private:
    /**
     * @brief Parses all rows from @p file into a filename→result map.
     *
     * @param file Opened input stream.
     * @return Parsed map.
     * @throws GraphConstructionException on malformed values.
     */
    static std::unordered_map<std::string, bool> parse_file(std::ifstream& file);

    /**
     * @brief Parses one CSV row and inserts the entry into @p result.
     *
     * @param line   Comma-separated row: filename,0_or_1.
     * @param result Collection to insert the parsed entry into.
     * @throws std::invalid_argument if the result field is not a valid integer.
     */
    static void insert_row(const std::string& line, std::unordered_map<std::string, bool>& result);
};

}  // namespace sgf
