#pragma once

#include "FilteringUtils.h"
#include "IFilterOutputManager.h"

#include <string>
#include <vector>

namespace sgf
{

/**
 * @brief Writes filter results as a CSV file with one row per library graph.
 *
 * Output format (no header row):
 * @code
 * graph_a.graphml,1
 * graph_b.graphml,0
 * @endcode
 * 1 = pruned, 0 = survives.
 */
class CSVFilterOutputManager : public IFilterOutputManager
{
public:
    /**
     * @brief Constructs a CSVFilterOutputManager.
     *
     * @param folder        Directory where the output file will be written.
     * @param base_filename File name without extension (e.g. "filter_results").
     */
    CSVFilterOutputManager(std::string folder, std::string base_filename);

    /**
     * @brief Default virtual destructor.
     */
    ~CSVFilterOutputManager() override = default;

    CSVFilterOutputManager(const CSVFilterOutputManager&) = default;
    CSVFilterOutputManager& operator=(const CSVFilterOutputManager&) = default;
    CSVFilterOutputManager(CSVFilterOutputManager&&) = default;
    CSVFilterOutputManager& operator=(CSVFilterOutputManager&&) = default;

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
     * @brief Returns "csv".
     *
     * @return Extension string "csv".
     */
    [[nodiscard]] std::string get_extension() const override;
};

}  // namespace sgf
