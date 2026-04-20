#include "JsonGraphReader.h"

#include "ColoredGraph.h"
#include "GraphConstructionException.h"
#include "LogLevel.h"
#include "LoggerHandler.h"
#include "Constants.h"
#include "SgfPathDoesntExistException.h"

#include <algorithm>
#include <boost/json/array.hpp>
#include <cstddef>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <boost/system/system_error.hpp>
#include <cstdint>
#include <fstream>
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
    throw GraphConstructionException(std::string(ERR_FAILED_TO_READ) + path + "': " +
                                     what_message);
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

    const boost::json::value root_value = boost::json::parse(file_contents);
    if (!root_value.is_object())
    {
        throw GraphConstructionException(std::string(ERR_NOT_AN_OBJECT) + path + "'");
    }
    return root_value.as_object();
}

std::unordered_map<uint32_t, uint32_t>
JsonGraphReader::collect_node_colors(const boost::json::array& nodes_array)
{
    std::unordered_map<uint32_t, uint32_t> color_by_id;
    color_by_id.reserve(nodes_array.size());
    for (const auto& node_value : nodes_array)
    {
        const boost::json::object& node_object = node_value.as_object();
        const uint64_t node_id_signed =  node_object.at(NODE_ID_KEY).as_uint64();
        if (node_id_signed > SgfConstants::MAX_VERTEX_COLOR)
        {
            throw GraphConstructionException("too many distinct colors: exceeds maximum of " +
                 std::to_string(SgfConstants::MAX_VERTEX_COLOR));
        }
        const uint32_t node_id = static_cast<uint32_t>(node_id_signed);
        color_by_id.emplace(node_id,
                            static_cast<int32_t>(node_object.at(COLOR_KEY).as_int64()));
    }
    return color_by_id;
}

std::unordered_map<uint32_t, uint32_t>
JsonGraphReader::build_consecutive_index_map(
    const std::unordered_map<uint32_t, uint32_t>& color_by_id)
{
    std::vector<uint32_t> all_ids;
    all_ids.reserve(color_by_id.size());
    for (const auto& entry : color_by_id)
    {
        all_ids.push_back(entry.first);
    }

    std::unordered_map<uint32_t, uint32_t> consecutive_index_by_original_id;
    consecutive_index_by_original_id.reserve(all_ids.size());
    uint32_t consecutive_index = 0;
    for (const uint32_t original_id : all_ids)
    {
        if (consecutive_index == INT32_MAX)
        {
            throw GraphConstructionException("vertex number exeeded maximum of " + std::to_string(INT32_MAX));

        }
        if (consecutive_index != 0)
        {
            ++consecutive_index;
        }
        consecutive_index_by_original_id.emplace(original_id, consecutive_index);
    }
    return consecutive_index_by_original_id;
}

std::vector<uint32_t>
JsonGraphReader::build_vertex_colors(
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
        if (link_value.as_object().contains(COLOR_KEY))
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

std::pair<uint32_t, uint32_t>
JsonGraphReader::extract_link_endpoints(
    const boost::json::object& link_object,
    const std::unordered_map<uint32_t, uint32_t>& consecutive_index_by_original_id)
{
    const uint32_t source_index = consecutive_index_by_original_id.at(
        static_cast<uint32_t>(link_object.at(SOURCE_KEY).as_int64()));
    const uint32_t target_index = consecutive_index_by_original_id.at(
        static_cast<uint32_t>(link_object.at(TARGET_KEY).as_int64()));
    return {source_index, target_index};
}

std::vector<std::tuple<uint32_t, uint32_t, uint32_t>>
JsonGraphReader::extract_colored_edges(
    const boost::json::array& links_array,
    const std::unordered_map<uint32_t, uint32_t>& consecutive_index_by_original_id)
{
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> edges;
    edges.reserve(links_array.size());
    for (const auto& link_value : links_array)
    {
        const boost::json::object& link_object = link_value.as_object();
        const std::pair<uint32_t, uint32_t> endpoints =
            extract_link_endpoints(link_object, consecutive_index_by_original_id);
        const uint32_t edge_color =
            static_cast<uint32_t>(link_object.at(COLOR_KEY).as_int64());
        edges.emplace_back(endpoints.first, endpoints.second, edge_color);
    }
    return edges;
}

std::vector<std::pair<uint32_t, uint32_t>>
JsonGraphReader::extract_uncolored_edges(
    const boost::json::array& links_array,
    const std::unordered_map<uint32_t, uint32_t>& consecutive_index_by_original_id)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    edges.reserve(links_array.size());
    for (const auto& link_value : links_array)
    {
        const boost::json::object& link_object = link_value.as_object();
        edges.emplace_back(extract_link_endpoints(link_object, consecutive_index_by_original_id));
    }
    return edges;
}

ColoredGraph JsonGraphReader::build_graph(
    const boost::json::array& links,
    const std::unordered_map<uint32_t, uint32_t>& consecutive_index_by_original_id,
    const uint32_t vertex_count,
    const std::vector<uint32_t>& vertex_colors,
    const bool is_directed)
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
    try
    {
        const boost::json::object root_object = parse_json_object(path);
        const boost::json::array& links = root_object.at(LINKS_KEY).as_array();
        const std::unordered_map<uint32_t, uint32_t> color_by_id =
            collect_node_colors(root_object.at(NODES_KEY).as_array());
        const std::unordered_map<uint32_t, uint32_t> consecutive_index_by_original_id =
            build_consecutive_index_map(color_by_id);
        const std::vector<uint32_t> vertex_colors =
            build_vertex_colors(color_by_id, consecutive_index_by_original_id);
        const uint32_t vertex_count = static_cast<uint32_t>(vertex_colors.size());
        const ColoredGraph graph =
            build_graph(links, consecutive_index_by_original_id, vertex_count, vertex_colors,
                        is_directed);
        logger.log(LogLevel::INFO, "read JSON graph from '" + path + "'");
        return graph;
    }
    catch (const boost::system::system_error& caught_exception)
    {
        rethrow_as_construction_error(path, caught_exception.what());
    }
    catch (const std::invalid_argument& caught_exception)
    {
        rethrow_as_construction_error(path, caught_exception.what());
    }
    catch (const std::out_of_range& caught_exception)
    {
        rethrow_as_construction_error(path, caught_exception.what());
    }
}

}  // namespace sgf
