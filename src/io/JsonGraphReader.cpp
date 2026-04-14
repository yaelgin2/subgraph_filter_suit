#include "JsonGraphReader.h"

#include "ColoredGraph.h"
#include "GraphConstructionException.h"
#include "ILogger.h"
#include "LogLevel.h"
#include "SgfPathDoesntExistException.h"

#include <algorithm>
#include <boost/json.hpp>
#include <boost/system/system_error.hpp>
#include <cstdint>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sgf
{

std::ifstream JsonGraphReader::open_file(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        throw SgfPathDoesntExistException("cannot open file: " + path);
    }
    return file;
}

[[noreturn]] void
JsonGraphReader::rethrow_as_construction_error(const std::string& path,
                                               const std::exception& caught_exception)
{
    throw GraphConstructionException("Failed to read JSON '" + path +
                                     "': " + caught_exception.what());
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
        throw GraphConstructionException("JSON root is not an object in '" + path + "'");
    }
    return root_value.as_object();
}

std::unordered_map<uint32_t, int32_t>
JsonGraphReader::collect_node_colors(const boost::json::array& nodes_array)
{
    std::unordered_map<uint32_t, int32_t> color_by_id;
    color_by_id.reserve(nodes_array.size());
    for (const auto& node_value : nodes_array)
    {
        const boost::json::object& node_object = node_value.as_object();
        const uint32_t node_id = static_cast<uint32_t>(node_object.at("id").as_int64());
        color_by_id.emplace(node_id, static_cast<int32_t>(node_object.at("color").as_int64()));
    }
    return color_by_id;
}

std::unordered_map<uint32_t, uint32_t>
JsonGraphReader::build_id_map(const std::unordered_map<uint32_t, int32_t>& color_by_id)
{
    std::vector<uint32_t> sorted_ids;
    sorted_ids.reserve(color_by_id.size());
    for (const auto& entry : color_by_id)
    {
        sorted_ids.push_back(entry.first);
    }
    // Sort to produce a deterministic consecutive mapping regardless of
    // hash-table iteration order.
    std::sort(sorted_ids.begin(), sorted_ids.end());

    std::unordered_map<uint32_t, uint32_t> id_map;
    id_map.reserve(sorted_ids.size());
    uint32_t consecutive_index = 0;
    for (const auto& original_id : sorted_ids)
    {
        id_map.emplace(original_id, consecutive_index);
        ++consecutive_index;
    }
    return id_map;
}

std::vector<int32_t>
JsonGraphReader::build_vertex_colors(const std::unordered_map<uint32_t, int32_t>& color_by_id,
                                     const std::unordered_map<uint32_t, uint32_t>& id_map)
{
    std::vector<int32_t> colors(id_map.size());
    for (const auto& entry : id_map)
    {
        colors[entry.second] = color_by_id.at(entry.first);
    }
    return colors;
}

bool JsonGraphReader::detect_edge_colors(const boost::json::array& links_array)
{
    uint32_t colored_count = 0;
    for (const auto& link_value : links_array)
    {
        if (link_value.as_object().contains("color"))
        {
            ++colored_count;
        }
    }
    const uint32_t total_links = static_cast<uint32_t>(links_array.size());
    if (colored_count != 0 && colored_count != total_links)
    {
        throw GraphConstructionException(
            "Mixed edge colors: some links have 'color' and some do not");
    }
    return colored_count > 0;
}

std::pair<uint32_t, uint32_t>
JsonGraphReader::extract_link_endpoints(const boost::json::object& link_object,
                                        const std::unordered_map<uint32_t, uint32_t>& id_map)
{
    const uint32_t source_index =
        id_map.at(static_cast<uint32_t>(link_object.at("source").as_int64()));
    const uint32_t target_index =
        id_map.at(static_cast<uint32_t>(link_object.at("target").as_int64()));
    return {source_index, target_index};
}

std::vector<std::tuple<uint32_t, uint32_t, uint32_t>>
JsonGraphReader::extract_colored_edges(const boost::json::array& links_array,
                                       const std::unordered_map<uint32_t, uint32_t>& id_map)
{
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> edges;
    edges.reserve(links_array.size());
    for (const auto& link_value : links_array)
    {
        const boost::json::object& link_object = link_value.as_object();
        const std::pair<uint32_t, uint32_t> endpoints = extract_link_endpoints(link_object, id_map);
        const uint32_t edge_color = static_cast<uint32_t>(link_object.at("color").as_int64());
        edges.emplace_back(endpoints.first, endpoints.second, edge_color);
    }
    return edges;
}

std::vector<std::pair<uint32_t, uint32_t>>
JsonGraphReader::extract_uncolored_edges(const boost::json::array& links_array,
                                         const std::unordered_map<uint32_t, uint32_t>& id_map)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    edges.reserve(links_array.size());
    for (const auto& link_value : links_array)
    {
        const boost::json::object& link_object = link_value.as_object();
        edges.emplace_back(extract_link_endpoints(link_object, id_map));
    }
    return edges;
}

ColoredGraph JsonGraphReader::build_graph(const boost::json::array& links,
                                          const std::unordered_map<uint32_t, uint32_t>& id_map,
                                          const uint32_t vertex_count,
                                          const std::vector<int32_t>& vertex_colors,
                                          const bool is_directed)
{
    if (detect_edge_colors(links))
    {
        std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> colored_edges =
            extract_colored_edges(links, id_map);
        return ColoredGraph(vertex_count, colored_edges, vertex_colors, is_directed);
    }
    std::vector<std::pair<uint32_t, uint32_t>> uncolored_edges =
        extract_uncolored_edges(links, id_map);
    return ColoredGraph(vertex_count, uncolored_edges, vertex_colors, is_directed);
}

void JsonGraphReader::log_read_result(const std::shared_ptr<ILogger>& logger,
                                      const std::string& path)
{
    if (logger == nullptr)
    {
        return;
    }
    logger->log(LogLevel::INFO, "read JSON graph from '" + path + "'");
}

ColoredGraph JsonGraphReader::read(const std::string& path, const bool is_directed,
                                   const std::weak_ptr<ILogger> logger) const
{
    try
    {
        const boost::json::object root_object = parse_json_object(path);
        const boost::json::array& links = root_object.at("links").as_array();
        const std::unordered_map<uint32_t, int32_t> color_by_id =
            collect_node_colors(root_object.at("nodes").as_array());
        const std::unordered_map<uint32_t, uint32_t> id_map = build_id_map(color_by_id);
        const std::vector<int32_t> vertex_colors = build_vertex_colors(color_by_id, id_map);
        const uint32_t vertex_count = static_cast<uint32_t>(vertex_colors.size());
        ColoredGraph graph = build_graph(links, id_map, vertex_count, vertex_colors, is_directed);
        log_read_result(logger.lock(), path);
        return graph;
    }
    catch (const boost::system::system_error& caught_exception)
    {
        rethrow_as_construction_error(path, caught_exception);
    }
    catch (const std::invalid_argument& caught_exception)
    {
        rethrow_as_construction_error(path, caught_exception);
    }
    catch (const std::out_of_range& caught_exception)
    {
        rethrow_as_construction_error(path, caught_exception);
    }
}

}  // namespace sgf
