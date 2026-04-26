#include "JsonGraphReader.h"

#include "ColoredGraph.h"
#include "Constants.h"
#include "GraphConstructionException.h"
#include "LogLevel.h"
#include "LoggerHandler.h"
#include "SgfPathDoesntExistException.h"

#include <algorithm>
#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <boost/system/system_error.hpp>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sgf
{

namespace
{

constexpr const char* NODES_KEY = "nodes";
constexpr const char* LINKS_KEY = "links";
constexpr const char* NODE_ID_KEY = "id";
constexpr const char* COLOR_KEY = "color";
constexpr const char* SOURCE_KEY = "source";
constexpr const char* TARGET_KEY = "target";
constexpr const char* ERR_FAILED_TO_READ = "Failed to read JSON '";
constexpr const char* ERR_NOT_AN_OBJECT = "JSON root is not an object in '";
constexpr const char* ERR_MIXED_COLORS =
    "Mixed edge colors: some links have 'color' and some do not";

/**
 * @brief Extract a typed array from a JSON object by key.
 * @param obj The JSON object to query.
 * @param key The key whose value must be an array.
 * @return Reference to the array value.
 */
const boost::json::array& extract_array(const boost::json::object& obj, const char* key)
{
    try
    {
        return obj.at(key).as_array();
    }
    catch (const boost::system::system_error& exc)
    {
        throw GraphConstructionException("missing or invalid key '" + std::string(key) + "': " +
                                         exc.what());
    }
}

/**
 * @brief Convert a JSON value to an object reference.
 * @param link_value The JSON value expected to be an object.
 * @return Reference to the JSON object.
 */
const boost::json::object& as_link_object(const boost::json::value& link_value)
{
    try
    {
        return link_value.as_object();
    }
    catch (const boost::system::system_error& exc)
    {
        throw GraphConstructionException("link is not a JSON object: " + std::string(exc.what()));
    }
}

}  // namespace

std::ifstream JsonGraphReader::open_file(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        throw SgfPathDoesntExistException("cannot open file: " + path);
    }
    return file;
}

[[noreturn]] void JsonGraphReader::rethrow_as_construction_error(const std::string& path,
                                                                 const std::string& what_message)
{
    throw GraphConstructionException(std::string(ERR_FAILED_TO_READ) + path + "': " + what_message);
}

std::string JsonGraphReader::read_file_contents(std::ifstream& file)
{
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

boost::json::object JsonGraphReader::parse_json_object(const std::string& path)
{
    std::ifstream file = open_file(path);
    const std::string file_contents = read_file_contents(file);
    try
    {
        const boost::json::value root_value = boost::json::parse(file_contents);
        if (!root_value.is_object())
        {
            throw GraphConstructionException(std::string(ERR_NOT_AN_OBJECT) + path + "'");
        }
        return root_value.as_object();
    }
    catch (const boost::system::system_error& exc)
    {
        rethrow_as_construction_error(path, exc.what());
    }
}

std::unordered_map<uint32_t, uint32_t>
JsonGraphReader::collect_node_colors(const boost::json::array& nodes_array)
{
    std::unordered_map<uint32_t, uint32_t> color_by_id;
    color_by_id.reserve(nodes_array.size());
    try
    {
        for (const auto& node_value : nodes_array)
        {
            const boost::json::object& node_object = node_value.as_object();
            const int64_t node_id_raw = node_object.at(NODE_ID_KEY).as_int64();
            if (node_id_raw < 0 ||
                node_id_raw > static_cast<int64_t>(SgfConstants::MAX_VERTEX_COLOR))
            {
                throw GraphConstructionException("invalid node id: " +
                                                 std::to_string(node_id_raw));
            }
            const uint32_t node_id = static_cast<uint32_t>(node_id_raw);
            const std::pair<std::unordered_map<uint32_t, uint32_t>::iterator, bool> insert_result =
                color_by_id.emplace(node_id,
                                    static_cast<uint32_t>(node_object.at(COLOR_KEY).as_int64()));
            if (!insert_result.second)
            {
                throw GraphConstructionException("duplicate node id: " + std::to_string(node_id));
            }
        }
    }
    catch (const boost::system::system_error& exc)
    {
        throw GraphConstructionException("malformed node entry: " + std::string(exc.what()));
    }
    return color_by_id;
}

std::unordered_map<uint32_t, uint32_t> JsonGraphReader::build_consecutive_index_map(
    const std::unordered_map<uint32_t, uint32_t>& color_by_id)
{
    std::unordered_map<uint32_t, uint32_t> consecutive_index_by_original_id;
    consecutive_index_by_original_id.reserve(color_by_id.size());
    uint32_t consecutive_index = 0;
    for (const auto& entry : color_by_id)
    {
        if (consecutive_index == std::numeric_limits<uint32_t>::max())
        {
            throw GraphConstructionException("vertex count exceeded maximum of " +
                                             std::to_string(std::numeric_limits<uint32_t>::max()));
        }
        consecutive_index_by_original_id.emplace(entry.first, consecutive_index);
        ++consecutive_index;
    }
    return consecutive_index_by_original_id;
}

std::vector<uint32_t> JsonGraphReader::build_vertex_colors(
    const std::unordered_map<uint32_t, uint32_t>& color_by_id,
    const std::unordered_map<uint32_t, uint32_t>& consecutive_index_by_original_id)
{
    std::vector<uint32_t> colors(consecutive_index_by_original_id.size());
    for (const auto& entry : consecutive_index_by_original_id)
    {
        colors.at(entry.second) = color_by_id.at(entry.first);
    }
    return colors;
}

bool JsonGraphReader::detect_edge_colors(const boost::json::array& links_array)
{
    size_t colored_count = 0;
    for (const auto& link_value : links_array)
    {
        if (as_link_object(link_value).contains(COLOR_KEY))
        {
            ++colored_count;
        }
    }
    const size_t total_links = links_array.size();
    if (colored_count != 0 && colored_count != total_links)
    {
        throw GraphConstructionException(ERR_MIXED_COLORS);
    }
    return colored_count > 0;
}

std::pair<uint32_t, uint32_t> JsonGraphReader::extract_link_endpoints(
    const boost::json::object& link_object,
    const std::unordered_map<uint32_t, uint32_t>& consecutive_index_by_original_id)
{
    try
    {
        const uint32_t source_index = consecutive_index_by_original_id.at(
            static_cast<uint32_t>(link_object.at(SOURCE_KEY).as_int64()));
        const uint32_t target_index = consecutive_index_by_original_id.at(
            static_cast<uint32_t>(link_object.at(TARGET_KEY).as_int64()));
        return {source_index, target_index};
    }
    catch (const boost::system::system_error& exc)
    {
        throw GraphConstructionException("malformed link endpoint: " + std::string(exc.what()));
    }
    catch (const std::out_of_range& exc)
    {
        throw GraphConstructionException("unknown vertex in link: " + std::string(exc.what()));
    }
}

std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> JsonGraphReader::extract_colored_edges(
    const boost::json::array& links_array,
    const std::unordered_map<uint32_t, uint32_t>& consecutive_index_by_original_id)
{
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> edges;
    edges.reserve(links_array.size());
    try
    {
        for (const auto& link_value : links_array)
        {
            const boost::json::object& link_object = as_link_object(link_value);
            const std::pair<uint32_t, uint32_t> endpoints =
                extract_link_endpoints(link_object, consecutive_index_by_original_id);
            const uint32_t edge_color = static_cast<uint32_t>(link_object.at(COLOR_KEY).as_int64());
            edges.emplace_back(endpoints.first, endpoints.second, edge_color);
        }
    }
    catch (const boost::system::system_error& exc)
    {
        throw GraphConstructionException("malformed edge color: " + std::string(exc.what()));
    }
    return edges;
}

std::vector<std::pair<uint32_t, uint32_t>> JsonGraphReader::extract_uncolored_edges(
    const boost::json::array& links_array,
    const std::unordered_map<uint32_t, uint32_t>& consecutive_index_by_original_id)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    edges.reserve(links_array.size());
    for (const auto& link_value : links_array)
    {
        edges.emplace_back(
            extract_link_endpoints(as_link_object(link_value), consecutive_index_by_original_id));
    }
    return edges;
}

ColoredGraph JsonGraphReader::build_graph(
    const boost::json::array& links,
    const std::unordered_map<uint32_t, uint32_t>& consecutive_index_by_original_id,
    const uint32_t vertex_count, const std::vector<uint32_t>& vertex_colors, const bool is_directed)
{
    if (detect_edge_colors(links))
    {
        std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> colored_edges =
            extract_colored_edges(links, consecutive_index_by_original_id);
        return {vertex_count, colored_edges, vertex_colors, is_directed};
    }
    std::vector<std::pair<uint32_t, uint32_t>> uncolored_edges =
        extract_uncolored_edges(links, consecutive_index_by_original_id);
    return {vertex_count, uncolored_edges, vertex_colors, is_directed};
}

ColoredGraph JsonGraphReader::read(const std::string& path, const bool is_directed,
                                   const LoggerHandler& logger) const
{
    const boost::json::object root_object = parse_json_object(path);
    const boost::json::array& nodes_array = extract_array(root_object, NODES_KEY);
    const boost::json::array& links = extract_array(root_object, LINKS_KEY);
    const std::unordered_map<uint32_t, uint32_t> color_by_id = collect_node_colors(nodes_array);
    const std::unordered_map<uint32_t, uint32_t> consecutive_index_by_original_id =
        build_consecutive_index_map(color_by_id);
    const std::vector<uint32_t> vertex_colors =
        build_vertex_colors(color_by_id, consecutive_index_by_original_id);
    const uint32_t vertex_count = static_cast<uint32_t>(vertex_colors.size());
    const ColoredGraph graph = build_graph(links, consecutive_index_by_original_id, vertex_count,
                                           vertex_colors, is_directed);
    logger.log(LogLevel::INFO, "read JSON graph from '" + path + "'");
    return graph;
}

}  // namespace sgf
