#include "ColoredGraph.h"

#include "InvalidArgumentException.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace sgf
{

ColoredGraph::ColoredGraph(const uint32_t num_vertices,
                           std::vector<std::pair<uint32_t, uint32_t>>& edges,
                           const std::vector<int32_t>& colors, const bool is_directed)
    : m_directed(is_directed)
{
    if (colors.size() != static_cast<size_t>(num_vertices))
    {
        throw InvalidArgumentException("colors size (" + std::to_string(colors.size()) +
                                       ") does not match num_vertices (" +
                                       std::to_string(num_vertices) + ")");
    }

    for (const auto& edge : edges)
    {
        validate_edge(edge, num_vertices);
    }

    m_colors = colors;

    if (!m_directed)
    {
        const size_t original_edge_count = edges.size();
        for (size_t edge_idx = 0; edge_idx < original_edge_count; ++edge_idx)
        {
            edges.emplace_back(edges.at(edge_idx).second, edges.at(edge_idx).first);
        }
        initiate_graph(num_vertices, edges, m_neighbours, m_index_of_neighbours);
        // Each unique undirected edge is stored twice (both directions), so divide by 2.
        m_edge_count = static_cast<uint32_t>(m_neighbours.size() / 2);
    }
    else
    {
        initiate_graph(num_vertices, edges, m_neighbours, m_index_of_neighbours);
        m_edge_count = static_cast<uint32_t>(m_neighbours.size());

        std::vector<std::pair<uint32_t, uint32_t>> reversed_edges;
        reversed_edges.reserve(edges.size());
        for (const auto& edge : edges)
        {
            reversed_edges.emplace_back(edge.second, edge.first);
        }
        initiate_graph(num_vertices, reversed_edges, m_reversed_neighbours,
                       m_reversed_index_of_neighbours);
    }
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
                                  std::vector<std::pair<uint32_t, uint32_t>>& edges,
                                  std::vector<uint32_t>& neighbours,
                                  std::vector<uint32_t>& index_of_neighbours)
{
    index_of_neighbours.assign(num_vertices, 0);

    std::sort(edges.begin(), edges.end());
    const std::vector<std::pair<uint32_t, uint32_t>>::iterator new_end =
        std::unique(edges.begin(), edges.end());
    edges.erase(new_end, edges.end());

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

std::pair<std::vector<uint32_t>::const_iterator, std::vector<uint32_t>::const_iterator>
ColoredGraph::compute_neighbour_range(const uint32_t vertex,
                                      const std::vector<uint32_t>& neighbours,
                                      const std::vector<uint32_t>& index_of_neighbours)
{
    const std::vector<uint32_t>::const_iterator range_begin =
        neighbours.begin() + index_of_neighbours.at(vertex);
    const std::vector<uint32_t>::const_iterator range_end =
        (vertex + 1 < index_of_neighbours.size())
            ? neighbours.begin() + index_of_neighbours.at(vertex + 1)
            : neighbours.end();
    return {range_begin, range_end};
}

std::pair<std::vector<uint32_t>::const_iterator, std::vector<uint32_t>::const_iterator>
ColoredGraph::get_neighbours(const uint32_t vertex, const bool reversed) const
{
    if (reversed && m_directed)
    {
        return compute_neighbour_range(vertex, m_reversed_neighbours,
                                       m_reversed_index_of_neighbours);
    }
    return compute_neighbour_range(vertex, m_neighbours, m_index_of_neighbours);
}

bool ColoredGraph::is_edge(const uint32_t source_vertex, const uint32_t dest_vertex) const
{
    const std::pair<std::vector<uint32_t>::const_iterator, std::vector<uint32_t>::const_iterator>
        range = get_neighbours(source_vertex, false);
    return std::find(range.first, range.second, dest_vertex) != range.second;
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
    return static_cast<uint32_t>(m_colors.at(vertex));
}

void ColoredGraph::set_vertex_color(const uint32_t vertex, const int32_t new_color)
{
    m_colors.at(vertex) = new_color;
}

bool ColoredGraph::is_directed() const
{
    return m_directed;
}

}  // namespace sgf
