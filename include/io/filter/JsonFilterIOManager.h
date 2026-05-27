#pragma once

#include "FilteringUtils.h"
#include "IFilterIOManager.h"
#include "LoggerHandler.h"

#include <boost/json/object.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace sgf
{

/**
 * @brief Reads and writes filter results as a JSON object mapping filename to filter result.
 *
 * File format:
 * @code
 * {"graph_a.graphml": true, "graph_b.graphml": false, ...}
 * @endcode
 * true = pruned, false = survives.
 */
class JsonFilterIOManager : public IFilterIOManager
{
public:
    /**
     * @brief Constructs a JsonFilterIOManager.
     *
     * @param folder  Directory where the output file will be written.
     * @param logger  Optional logger for diagnostics.
     */
    explicit JsonFilterIOManager(std::string folder, LoggerHandler logger = LoggerHandler::null());

    /**
     * @brief Default virtual destructor.
     */
    ~JsonFilterIOManager() override = default;

    JsonFilterIOManager(const JsonFilterIOManager&) = default;
    JsonFilterIOManager& operator=(const JsonFilterIOManager&) = default;
    JsonFilterIOManager(JsonFilterIOManager&&) = default;
    JsonFilterIOManager& operator=(JsonFilterIOManager&&) = default;

protected:
    /**
     * @brief Writes filter results as a JSON object to @p full_path.
     *
     * @param filenames Library graph filenames.
     * @param results   Filter result per library graph.
     * @param full_path Destination file path.
     * @throws SgfPathExistsException if the file cannot be opened or written.
     */
    void write_to_file(const std::vector<std::string>& filenames, const FilterResult& results,
                       const std::string& full_path) const override;

    /**
     * @brief Reads filter results from the JSON file at @p full_path.
     *
     * @param full_path Source JSON file path.
     * @return Map from library graph filename to its filter result.
     * @throws SgfPathExistsException if the file cannot be opened.
     * @throws GraphConstructionException if the file content is malformed JSON.
     */
    std::unordered_map<std::string, bool>
    read_from_file(const std::string& full_path) const override;

    /**
     * @brief Returns "json".
     * @return Extension string "json".
     */
    [[nodiscard]] std::string get_extension() const override;

private:
    /**
     * @brief Builds the JSON object from @p filenames and @p results.
     *
     * @param filenames Library graph filenames.
     * @param results   Filter result per library graph.
     * @return JSON object mapping filename → result.
     */
    static boost::json::object build_json_object(const std::vector<std::string>& filenames,
                                                 const FilterResult& results);

    /**
     * @brief Serializes @p json_obj and writes it to @p full_path.
     *
     * @param json_obj  JSON object to serialize.
     * @param full_path Destination file path.
     * @throws SgfPathExistsException if the file cannot be opened or written.
     */
    static void write_json_to_file(const boost::json::object& json_obj,
                                   const std::string& full_path);

    /**
     * @brief Parses a JSON object from @p full_path into a filename→result map.
     *
     * @param full_path Source file path.
     * @return Parsed map.
     * @throws SgfPathExistsException if the file cannot be opened.
     * @throws GraphConstructionException if parsing fails.
     */
    static std::unordered_map<std::string, bool> parse_json_file(const std::string& full_path);
};

}  // namespace sgf
