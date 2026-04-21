#include "VertexEdgeGraphReader.h"

#include "ColoredGraph.h"
#include "GraphConstructionException.h"
#include "LogLevel.h"
#include "LoggerHandler.h"
#include "SgfPathDoesntExistException.h"

#include <algorithm>
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

constexpr uint32_t TOKENS_PER_VERTEX_LINE = 2;
constexpr uint32_t TOKENS_PER_UNCOLORED_EDGE_LINE = 2;
constexpr uint32_t TOKENS_PER_COLORED_EDGE_LINE = 3;
constexpr const char* VERTEX_INDICES_SUFFIX = ".vertex_indices";
constexpr const char* EDGES_SUFFIX = ".edges";

}  // namespace

std::ifstream VertexEdgeGraphReader::open_file(const std::string& file_path)
{
    std::ifstream file(file_path);
    if (!file.is_open())
    {
        throw SgfPathDoesntExistException("cannot open file: " + file_path);
    }
    return file;
}

[[noreturn]] void VertexEdgeGraphReader::rethrow_as_construction_error(const std::string& file_path,
                                                                       const std::out_of_range& exc)
{
    throw GraphConstructionException("Failed to read '" + file_path + "': " + exc.what());
}

std::pair<uint32_t, uint32_t> VertexEdgeGraphReader::parse_vertex_line(
    const std::string& line, const std::string& file_path)
{
    std::istringstream stream(line);
    uint32_t vertex_id = 0;
    uint32_t color = 0;
    if (!(stream >> vertex_id >> color))
    {
        throw GraphConstructionException("Malformed vertex line in '" + file_path + "': '" + line +
                                         "' (expected " +
                                         std::to_string(TOKENS_PER_VERTEX_LINE) + " tokens)");
    }
    uint32_t extra = 0;
    if (stream >> extra)
    {
        throw GraphConstructionException("Malformed vertex line in '" + file_path + "': '" + line +
                                         "' (too many tokens, expected " +
                                         std::to_string(TOKENS_PER_VERTEX_LINE) + ")");
    }
    return {vertex_id, color};
}

std::unordered_map<uint32_t, uint32_t>
VertexEdgeGraphReader::parse_vertex_file(const std::string& vertices_path)
{
    std::ifstream file = open_file(vertices_path);
    std::unordered_map<uint32_t, uint32_t> vertex_color_by_original_id;
    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty())
        {
            continue;
        }
        const std::pair<uint32_t, uint32_t> parsed = parse_vertex_line(line, vertices_path);
        if (!vertex_color_by_original_id.emplace(parsed.first, parsed.second).second)
        {
            throw GraphConstructionException("Duplicate vertex ID " +
                                             std::to_string(parsed.first) + " in '" +
                                             vertices_path + "'");
        }
    }
    return vertex_color_by_original_id;
}

std::unordered_map<uint32_t, uint32_t> VertexEdgeGraphReader::build_consecutive_index_map(
    const std::unordered_map<uint32_t, uint32_t>& vertex_color_by_original_id)
{
    std::vector<uint32_t> sorted_ids;
    sorted_ids.reserve(vertex_color_by_original_id.size());
    for (const auto& entry : vertex_color_by_original_id)
    {
        sorted_ids.push_back(entry.first);
    }
    std::sort(sorted_ids.begin(), sorted_ids.end());
    std::unordered_map<uint32_t, uint32_t> consecutive_index_by_original_id;
    consecutive_index_by_original_id.reserve(sorted_ids.size());
    uint32_t consecutive_index = 0;
    for (const uint32_t original_id : sorted_ids)
    {
        consecutive_index_by_original_id.emplace(original_id, consecutive_index);
        ++consecutive_index;
    }
    return consecutive_index_by_original_id;
}

std::vector<uint32_t> VertexEdgeGraphReader::build_vertex_colors(
    const std::unordered_map<uint32_t, uint32_t>& vertex_color_by_original_id,
    const std::unordered_map<uint32_t, uint32_t>& consecutive_index_by_original_id)
{
    std::vector<uint32_t> vertex_colors(consecutive_index_by_original_id.size(), 0);
    for (const auto& entry : consecutive_index_by_original_id)
    {
        vertex_colors.at(entry.second) = vertex_color_by_original_id.at(entry.first);
    }
    return vertex_colors;
}

uint32_t VertexEdgeGraphReader::resolve_vertex_id(
    const uint32_t raw_id,
    const std::unordered_map<uint32_t, uint32_t>& consecutive_index_by_original_id,
    const std::string& role, const std::string& line)
{
    const std::unordered_map<uint32_t, uint32_t>::const_iterator it =
        consecutive_index_by_original_id.find(raw_id);
    if (it == consecutive_index_by_original_id.end())
    {
        throw GraphConstructionException("Unknown " + role + " vertex ID " +
                                         std::to_string(raw_id) + " in edge line: '" + line + "'");
    }
    return it->second;
}

bool VertexEdgeGraphReader::parse_edge_line(
    const std::string& line,
    const std::unordered_map<uint32_t, uint32_t>& consecutive_index_by_original_id,
    uint32_t& out_src, uint32_t& out_dst, uint32_t& out_color)
{
    std::istringstream stream(line);
    uint32_t raw_src = 0;
    uint32_t raw_dst = 0;
    if (!(stream >> raw_src >> raw_dst))
    {
        throw GraphConstructionException("Malformed edge line (expected at least " +
                                         std::to_string(TOKENS_PER_UNCOLORED_EDGE_LINE) +
                                         " tokens): '" + line + "'");
    }
    out_src = resolve_vertex_id(raw_src, consecutive_index_by_original_id, "source", line);
    out_dst = resolve_vertex_id(raw_dst, consecutive_index_by_original_id, "destination", line);
    uint32_t color = 0;
    const bool has_color = static_cast<bool>(stream >> color);
    out_color = color;
    if (has_color)
    {
        uint32_t extra = 0;
        if (stream >> extra)
        {
            throw GraphConstructionException(
                "Malformed edge line (too many tokens, expected at most " +
                std::to_string(TOKENS_PER_COLORED_EDGE_LINE) + "): '" + line + "'");
        }
    }
    return has_color;
}

void VertexEdgeGraphReader::parse_edge_file(
    const std::string& edges_path,
    const std::unordered_map<uint32_t, uint32_t>& consecutive_index_by_original_id,
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>>& colored_edges,
    std::vector<std::pair<uint32_t, uint32_t>>& uncolored_edges)
{
    std::ifstream file = open_file(edges_path);
    std::string line;
    bool edges_have_colors = false;
    bool color_mode_set = false;
    while (std::getline(file, line))
    {
        if (line.empty())
        {
            continue;
        }
        uint32_t src = 0;
        uint32_t dst = 0;
        uint32_t color = 0;
        const bool has_color =
            parse_edge_line(line, consecutive_index_by_original_id, src, dst, color);
        if (!color_mode_set)
        {
            edges_have_colors = has_color;
            color_mode_set = true;
        }
        else if (has_color != edges_have_colors)
        {
            throw GraphConstructionException("Mixed edge colors in '" + edges_path +
                                             "': some lines have a color token and some do not");
        }
        if (edges_have_colors)
        {
            colored_edges.emplace_back(src, dst, color);
        }
        else
        {
            uncolored_edges.emplace_back(src, dst);
        }
    }
}

ColoredGraph VertexEdgeGraphReader::read(const std::string& path, const bool is_directed,
                                         const LoggerHandler& logger) const
{
    try
    {
        const std::string vertices_path = path + VERTEX_INDICES_SUFFIX;
        const std::string edges_path = path + EDGES_SUFFIX;
        const std::unordered_map<uint32_t, uint32_t> color_by_id = parse_vertex_file(vertices_path);
        const std::unordered_map<uint32_t, uint32_t> consecutive_index_by_original_id =
            build_consecutive_index_map(color_by_id);
        const std::vector<uint32_t> vertex_colors =
            build_vertex_colors(color_by_id, consecutive_index_by_original_id);
        std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> colored_edges;
        std::vector<std::pair<uint32_t, uint32_t>> uncolored_edges;
        parse_edge_file(edges_path, consecutive_index_by_original_id, colored_edges,
                        uncolored_edges);
        logger.log(LogLevel::INFO, "read vertex-edge graph from '" + path + "'");
        const uint32_t vertex_count = static_cast<uint32_t>(vertex_colors.size());
        if (colored_edges.empty())
        {
            return {vertex_count, uncolored_edges, vertex_colors, is_directed};
        }
        return {vertex_count, colored_edges, vertex_colors, is_directed};
    }
    catch (const std::out_of_range& exc)
    {
        rethrow_as_construction_error(path, exc);
    }
}

}  // namespace sgf
