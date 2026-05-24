#pragma once

#include "FilteringUtils.h"
#include "IFilterOutputManager.h"
#include "LoggerHandler.h"

#include <string>
#include <vector>

namespace sgf
{

/**
 * @brief Writes filter results as a JSON object mapping filename to filter result.
 *
 * Output format:
 * @code
 * {"graph_a.graphml": true, "graph_b.graphml": false, ...}
 * @endcode
 * true = pruned, false = survives.
 */
class JsonFilterOutputManager : public IFilterOutputManager
{
public:
    /**
     * @brief Constructs a JsonFilterOutputManager.
     *
     * @param folder  Directory where the output file will be written.
     * @param logger  Optional logger for diagnostics.
     */
    JsonFilterOutputManager(std::string folder, LoggerHandler logger = LoggerHandler::null());

    /**
     * @brief Default virtual destructor.
     */
    ~JsonFilterOutputManager() override = default;

    JsonFilterOutputManager(const JsonFilterOutputManager&) = default;
    JsonFilterOutputManager& operator=(const JsonFilterOutputManager&) = default;
    JsonFilterOutputManager(JsonFilterOutputManager&&) = default;
    JsonFilterOutputManager& operator=(JsonFilterOutputManager&&) = default;

protected:
    /**
     * @brief Writes filter results as JSON to @p full_path.
     *
     * @param filenames Library graph filenames.
     * @param results   Filter result per library graph.
     * @param full_path Destination file path.
     * @throws SgfPathExistsException if the file cannot be opened or written.
     */
    void write_to_file(const std::vector<std::string>& filenames, const FilterResult& results,
                       const std::string& full_path) const override;

    /**
     * @brief Returns "json".
     *
     * @return Extension string "json".
     */
    [[nodiscard]] std::string get_extension() const override;
};

}  // namespace sgf
