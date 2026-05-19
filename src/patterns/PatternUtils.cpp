#include "PatternUtils.h"

#include <algorithm>
#include <map>
#include <iostream>

/* ---------- private helpers ---------- */

namespace sgf
{

void PatternUtils::scan_graph_colors(const ColoredGraph& graph,
                                     std::map<int32_t, uint32_t>& original_to_remapped_color,
                                     std::vector<int32_t>& color_map)
{
    for (uint32_t vertex_index = 0; vertex_index < graph.vertex_count(); ++vertex_index)
    {
        const int32_t original_color = graph.get_vertex_color(vertex_index);
        if (!original_to_remapped_color.count(original_color))
        {
            original_to_remapped_color[original_color] = static_cast<uint32_t>(color_map.size());
            color_map.push_back(original_color);
        }
    }
}

void PatternUtils::recolor_graph(const std::map<int32_t, uint32_t>& original_to_remapped_color,
                                  ColoredGraph& graph)
{
    for (uint32_t vertex_index = 0; vertex_index < graph.vertex_count(); ++vertex_index)
    {
        graph.set_vertex_color(
            vertex_index,
            original_to_remapped_color.at(graph.get_vertex_color(vertex_index)));
    }
}

/* ---------- public interface ---------- */

std::vector<int32_t> PatternUtils::map_colors(ColoredGraph& graph)
{
    std::vector<int32_t> color_map;
    std::map<int32_t, uint32_t> original_to_remapped_color;
    scan_graph_colors(graph, original_to_remapped_color, color_map);
    recolor_graph(original_to_remapped_color, graph);
    return color_map;
}

std::vector<int32_t> PatternUtils::map_colors(ColoredGraph& first_graph, ColoredGraph& second_graph)
{
    std::vector<int32_t> color_map;
    std::map<int32_t, uint32_t> original_to_remapped_color;
    scan_graph_colors(first_graph, original_to_remapped_color, color_map);
    scan_graph_colors(second_graph, original_to_remapped_color, color_map);
    recolor_graph(original_to_remapped_color, first_graph);
    recolor_graph(original_to_remapped_color, second_graph);
    return color_map;
}

std::vector<int32_t> PatternUtils::map_colors(std::vector<ColoredGraph>& graphs)
{
    std::vector<int32_t> color_map;
    std::map<int32_t, uint32_t> original_to_remapped_color;
    for (const auto& graph : graphs)
    {
        scan_graph_colors(graph, original_to_remapped_color, color_map);
    }
    for (auto& graph : graphs)
    {
        recolor_graph(original_to_remapped_color, graph);
    }
    return color_map;
}

void PatternUtils::recolor_pattern(BoostGraph& pattern,
                                    const std::vector<int32_t>& color_map)
{
    for (auto vertex : boost::make_iterator_range(vertices(pattern)))
    {
        pattern[vertex].m_color = static_cast<uint32_t>(color_map[pattern[vertex].m_color]);
    }
}

std::vector<uint32_t> PatternUtils::find_initial_matches(const ColoredGraph& graph,
                                                          uint32_t color)
{
    std::vector<uint32_t> matches;
    for (uint32_t vertex = 0; vertex < graph.vertex_count(); ++vertex)
    {
        if (graph.get_vertex_color(vertex) == color)
        {
            matches.push_back(vertex);
        }
    }
    return matches;
}

void PatternUtils::count_vertex_colors(const ColoredGraph& graph,
                                       std::vector<uint32_t>& counts,
                                       uint64_t& total_vertices)
{
    for (uint32_t vertex = 0; vertex < graph.vertex_count(); ++vertex) 
    {
        counts[graph.get_vertex_color(vertex)]++;
        ++total_vertices;
    }
}

std::vector<std::vector<uint32_t>> PatternUtils::get_all_color_matches(const ColoredGraph& graph, uint32_t color_count)
{
    std::vector<std::vector<uint32_t>> vertices_by_color(color_count);
    for (uint32_t vertex_index = 0; vertex_index < graph.vertex_count(); ++vertex_index)
    {
        const int32_t vertex_color = graph.get_vertex_color(vertex_index);
        if (vertex_color >= 0 && vertex_color < static_cast<int32_t>(color_count))
        {
            vertices_by_color[vertex_color].push_back(vertex_index);
        }
    }
    return vertices_by_color;
}

std::vector<double> PatternUtils::counts_to_probability(
    const std::vector<uint32_t>& counts,
    uint64_t total_vertices)
{
    std::vector<double> probability(counts.size());
    for (uint32_t color_index = 0; color_index < counts.size(); ++color_index)
    {
        probability[color_index] = (total_vertices > 0 && counts[color_index] > 0)
            ? static_cast<double>(counts[color_index]) / static_cast<double>(total_vertices)
            : 0.0;
    }
    return probability;
}

std::vector<double> PatternUtils::compute_color_distribution(
    uint32_t                         color_count,
    int32_t                          search_graph_count,
    const std::vector<ColoredGraph>& search_graphs)
{
    std::vector<uint32_t> color_vertex_counts(color_count, 0);
    uint64_t total_vertices = 0;
    for (int32_t graph_index = 0; graph_index < search_graph_count; ++graph_index)
    {
        count_vertex_colors(search_graphs[graph_index], color_vertex_counts, total_vertices);
    }
    return counts_to_probability(color_vertex_counts, total_vertices);
}

std::vector<double> PatternUtils::compute_color_distribution(
    uint32_t            color_count,
    const ColoredGraph& source_graph)
{
    std::vector<uint32_t> color_vertex_counts(color_count, 0);
    uint64_t total_vertices = 0;
    count_vertex_colors(source_graph, color_vertex_counts, total_vertices);
    return counts_to_probability(color_vertex_counts, total_vertices);
}

double PatternUtils::compute_density(uint32_t vertex_count, uint32_t edge_count)
{
    if (vertex_count < 2)
    {
        return 0.0;
    }
    const uint64_t max_possible_edges =
        static_cast<uint64_t>(vertex_count) * (vertex_count - 1) / 2;
    if (max_possible_edges == 0)
    {
        return 0.0;
    } 
    return static_cast<double>(edge_count) / static_cast<double>(max_possible_edges);
}

void PatternUtils::add_edge(bool is_directed, BoostGraph& graph, uint32_t source, uint32_t target)
{
    if (is_directed) 
    {
        boost::add_edge(source, target, graph);
    } 
    else 
    {
        boost::add_edge(source, target, graph);
        boost::add_edge(target, source, graph);
    }
}

} // namespace sgf
