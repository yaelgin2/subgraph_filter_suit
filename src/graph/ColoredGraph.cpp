#include "ColoredGraph.h"

#include "InvalidArgumentException.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace sgf
{

ColoredGraph::ColoredGraph(const uint32_t num_vertices,
                           std::vector<std::pair<uint32_t, uint32_t>>& edges,
                           const std::vector<uint32_t>& vertex_colors, const bool is_directed)
    : m_colors(vertex_colors)
    , m_directed(is_directed)
{
    validate_vertex_colors_size(vertex_colors, num_vertices);
    for (const auto& edge : edges)
    {
        validate_edge(edge, num_vertices);
    }
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> edge_tuples =
        to_edge_tuples(edges, {}, false);
    build_structures(num_vertices, edge_tuples);
}

ColoredGraph::ColoredGraph(const uint32_t num_vertices,
                           std::vector<std::tuple<uint32_t, uint32_t, uint32_t>>& edges,
                           const std::vector<uint32_t>& vertex_colors, const bool is_directed)
    : m_colors(vertex_colors)
    , m_directed(is_directed)
    , m_edges_colored(true)
{
    validate_vertex_colors_size(vertex_colors, num_vertices);
    for (const auto& edge : edges)
    {
        validate_edge({std::get<0>(edge), std::get<1>(edge)}, num_vertices);
    }
    build_structures(num_vertices, edges);
}

void ColoredGraph::validate_vertex_colors_size(const std::vector<uint32_t>& vertex_colors,
                                               const uint32_t num_vertices)
{
    if (vertex_colors.size() != static_cast<size_t>(num_vertices))
    {
        throw InvalidArgumentException(
            "vertex_colors size (" + std::to_string(vertex_colors.size()) +
            ") does not match num_vertices (" + std::to_string(num_vertices) + ")");
    }
}

void ColoredGraph::validate_edge(const std::pair<uint32_t, uint32_t>& edge,
                                 const uint32_t num_vertices)
{
    if (edge.first == edge.second)
    {
        throw InvalidArgumentException("self-loop at vertex " + std::to_string(edge.first) +
                                       " is not allowed");
    }
    if (edge.first >= num_vertices || edge.second >= num_vertices)
    {
        throw InvalidArgumentException(
            "edge (" + std::to_string(edge.first) + ", " + std::to_string(edge.second) +
            ") references a vertex ID >= num_vertices (" + std::to_string(num_vertices) + ")");
    }
}

void ColoredGraph::build_structures(const uint32_t num_vertices,
                                    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>>& edges)
{
    if (m_directed)
    {
        build_directed_structures(num_vertices, edges);
    }
    else
    {
        build_undirected_structures(num_vertices, edges);
    }
}

void ColoredGraph::build_undirected_structures(
    const uint32_t num_vertices,
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>>& colored_edges)
{
    const size_t original_edge_count = colored_edges.size();
    for (size_t idx = 0; idx < original_edge_count; ++idx)
    {
        colored_edges.emplace_back(std::get<1>(colored_edges.at(idx)),
                                   std::get<0>(colored_edges.at(idx)),
                                   std::get<2>(colored_edges.at(idx)));
    }
    sort_and_deduplicate(colored_edges);
    m_edge_count = static_cast<uint32_t>(colored_edges.size()) / 2U;
    std::vector<std::pair<uint32_t, uint32_t>> pairs;
    std::vector<uint32_t> edge_colors;
    extract_edges(colored_edges, pairs, edge_colors, m_edges_colored);
    initiate_graph(num_vertices, pairs, m_neighbours, m_index_of_neighbours);
    m_edge_colors = edge_colors;
}

void ColoredGraph::build_directed_structures(
    const uint32_t num_vertices,
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>>& colored_edges)
{
    sort_and_deduplicate(colored_edges);
    std::vector<std::pair<uint32_t, uint32_t>> pairs;
    std::vector<uint32_t> edge_colors;
    extract_edges(colored_edges, pairs, edge_colors, m_edges_colored);
    initiate_graph(num_vertices, pairs, m_neighbours, m_index_of_neighbours);
    m_edge_colors = edge_colors;
    m_edge_count = static_cast<uint32_t>(m_neighbours.size());
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> reversed_tuples =
        to_edge_tuples(pairs, edge_colors, true);
    sort_and_deduplicate(reversed_tuples);
    std::vector<std::pair<uint32_t, uint32_t>> reversed_pairs;
    std::vector<uint32_t> reversed_colors;
    extract_edges(reversed_tuples, reversed_pairs, reversed_colors, m_edges_colored);
    initiate_graph(num_vertices, reversed_pairs, m_reversed_neighbours,
                   m_reversed_index_of_neighbours);
    m_reversed_edge_colors = reversed_colors;
}

void ColoredGraph::sort_and_deduplicate(
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>>& edges)
{
    std::sort(edges.begin(), edges.end());
    for (size_t idx = 1; idx < edges.size(); ++idx)
    {
        if (std::get<0>(edges.at(idx - 1)) == std::get<0>(edges.at(idx)) &&
            std::get<1>(edges.at(idx - 1)) == std::get<1>(edges.at(idx)) &&
            std::get<2>(edges.at(idx - 1)) != std::get<2>(edges.at(idx)))
        {
            throw InvalidArgumentException(
                "duplicate edge (" + std::to_string(std::get<0>(edges.at(idx))) + ", " +
                std::to_string(std::get<1>(edges.at(idx))) + ") with conflicting colors");
        }
    }
    edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
}

void ColoredGraph::extract_edges(
    const std::vector<std::tuple<uint32_t, uint32_t, uint32_t>>& tuples,
    std::vector<std::pair<uint32_t, uint32_t>>& pairs, std::vector<uint32_t>& colors,
    const bool fill_colors)
{
    pairs.resize(tuples.size());
    if (fill_colors)
    {
        colors.resize(tuples.size());
    }
    for (size_t idx = 0; idx < tuples.size(); ++idx)
    {
        pairs.at(idx) = {std::get<0>(tuples.at(idx)), std::get<1>(tuples.at(idx))};
        if (fill_colors)
        {
            colors.at(idx) = std::get<2>(tuples.at(idx));
        }
    }
}

std::vector<std::tuple<uint32_t, uint32_t, uint32_t>>
ColoredGraph::to_edge_tuples(const std::vector<std::pair<uint32_t, uint32_t>>& pairs,
                             const std::vector<uint32_t>& colors, const bool reversed)
{
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> tuples;
    tuples.reserve(pairs.size());
    for (size_t idx = 0; idx < pairs.size(); ++idx)
    {
        const uint32_t color = colors.empty() ? 0 : colors.at(idx);
        const uint32_t first = reversed ? pairs.at(idx).second : pairs.at(idx).first;
        const uint32_t second = reversed ? pairs.at(idx).first : pairs.at(idx).second;
        tuples.emplace_back(first, second, color);
    }
    return tuples;
}

void ColoredGraph::fill_index_range(const uint32_t from_vertex, const uint32_t to_vertex,
                                    const uint32_t neighbour_count,
                                    std::vector<uint32_t>& index_of_neighbours)
{
    for (uint32_t fill_vertex = from_vertex + 1; fill_vertex <= to_vertex; ++fill_vertex)
    {
        index_of_neighbours.at(fill_vertex) = neighbour_count;
    }
}

void ColoredGraph::initiate_graph(const uint32_t num_vertices,
                                  const std::vector<std::pair<uint32_t, uint32_t>>& edges,
                                  std::vector<uint32_t>& neighbours,
                                  std::vector<uint32_t>& index_of_neighbours)
{
    index_of_neighbours.assign(num_vertices, 0);
    if (edges.empty())
    {
        return;
    }
    neighbours.reserve(edges.size());
    uint32_t current_vertex = edges.front().first;
    for (const auto& edge : edges)
    {
        if (edge.first != current_vertex)
        {
            fill_index_range(current_vertex, edge.first, static_cast<uint32_t>(neighbours.size()),
                             index_of_neighbours);
            current_vertex = edge.first;
        }
        neighbours.push_back(edge.second);
    }
    fill_index_range(current_vertex, num_vertices - 1, static_cast<uint32_t>(neighbours.size()),
                     index_of_neighbours);
}

std::pair<std::vector<uint32_t>::const_iterator, std::vector<uint32_t>::const_iterator>
ColoredGraph::compute_range(const uint32_t vertex, const std::vector<uint32_t>& elements,
                            const std::vector<uint32_t>& index_of_neighbours)
{
    const std::vector<uint32_t>::const_iterator range_begin =
        elements.begin() + index_of_neighbours.at(vertex);
    const std::vector<uint32_t>::const_iterator range_end =
        (static_cast<size_t>(vertex) + 1UL < index_of_neighbours.size())
            ? elements.begin() + index_of_neighbours.at(vertex + 1)
            : elements.end();
    return {range_begin, range_end};
}

std::pair<std::vector<uint32_t>::const_iterator, std::vector<uint32_t>::const_iterator>
ColoredGraph::get_neighbours(const uint32_t vertex, const bool reversed) const
{
    if (reversed && m_directed)
    {
        return compute_range(vertex, m_reversed_neighbours, m_reversed_index_of_neighbours);
    }
    return compute_range(vertex, m_neighbours, m_index_of_neighbours);
}

std::pair<std::vector<uint32_t>::const_iterator, std::vector<uint32_t>::const_iterator>
ColoredGraph::get_neighbour_edge_colors(const uint32_t vertex, const bool reversed) const
{
    if (!m_edges_colored)
    {
        throw InvalidArgumentException("graph has no edge colors");
    }
    if (reversed && m_directed)
    {
        return compute_range(vertex, m_reversed_edge_colors, m_reversed_index_of_neighbours);
    }
    return compute_range(vertex, m_edge_colors, m_index_of_neighbours);
}

bool ColoredGraph::is_edge(const uint32_t source_vertex, const uint32_t dest_vertex) const
{
    const std::pair<std::vector<uint32_t>::const_iterator, std::vector<uint32_t>::const_iterator>
        neighbour_range = get_neighbours(source_vertex, false);
    return std::find(neighbour_range.first, neighbour_range.second, dest_vertex) !=
           neighbour_range.second;
}

uint32_t ColoredGraph::vertex_count() const
{
    return static_cast<uint32_t>(m_colors.size());
}

uint32_t ColoredGraph::edge_count() const
{
    return m_edge_count;
}

uint32_t ColoredGraph::get_vertex_color(const uint32_t vertex) const
{
    return m_colors.at(vertex);
}

void ColoredGraph::set_vertex_color(const uint32_t vertex, const uint32_t new_color)
{
    m_colors.at(vertex) = new_color;
}

bool ColoredGraph::is_edges_colored() const
{
    return m_edges_colored;
}

uint32_t ColoredGraph::get_edge_color(const uint32_t source_vertex,
                                      const uint32_t dest_vertex) const
{
    if (!m_edges_colored)
    {
        throw InvalidArgumentException("graph has no edge colors");
    }
    const std::pair<std::vector<uint32_t>::const_iterator, std::vector<uint32_t>::const_iterator>
        neighbour_range = get_neighbours(source_vertex, false);
    const std::vector<uint32_t>::const_iterator found =
        std::find(neighbour_range.first, neighbour_range.second, dest_vertex);
    if (found == neighbour_range.second)
    {
        throw InvalidArgumentException("edge (" + std::to_string(source_vertex) + ", " +
                                       std::to_string(dest_vertex) + ") does not exist");
    }
    return get_edge_color_at(found, false);
}

uint32_t ColoredGraph::get_edge_color_at(const std::vector<uint32_t>::const_iterator neighbour_it,
                                         const bool reversed) const
{
    if (!m_edges_colored)
    {
        throw InvalidArgumentException("graph has no edge colors");
    }
    const std::vector<uint32_t>& edge_colors = reversed ? m_reversed_edge_colors : m_edge_colors;
    const std::vector<uint32_t>& neighbours = reversed ? m_reversed_neighbours : m_neighbours;
    const size_t offset = static_cast<size_t>(neighbour_it - neighbours.cbegin());
    return edge_colors.at(offset);
}

bool ColoredGraph::is_directed() const
{
    return m_directed;
}

}  // namespace sgf
