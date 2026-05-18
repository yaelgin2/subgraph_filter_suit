#include "PatternUtils.h"

#include "BoostGraph.h"
#include "ColoredGraph.h"

#include <algorithm>
#include <boost/graph/adjacency_list.hpp>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

namespace sgf
{

/* ---------- Private helpers ---------- */

void PatternUtils::scan_graph_colors(
    const ColoredGraph& graph,
    std::map<int32_t, uint32_t>& old_to_new,
    std::vector<int32_t>& color_map)
{
    const uint32_t vertex_count = graph.vertex_count();
    for (uint32_t vertex_idx = 0U; vertex_idx < vertex_count; ++vertex_idx)
    {
        const int32_t original_color = static_cast<int32_t>(graph.get_vertex_color(vertex_idx));
        if (old_to_new.find(original_color) == old_to_new.end())
        {
            const uint32_t new_id = static_cast<uint32_t>(color_map.size());
            old_to_new[original_color] = new_id;
            color_map.push_back(original_color);
        }
    }
}

void PatternUtils::recolor_graph(
    const std::map<int32_t, uint32_t>& old_to_new,
    ColoredGraph& graph)
{
    const uint32_t vertex_count = graph.vertex_count();
    for (uint32_t vertex_idx = 0U; vertex_idx < vertex_count; ++vertex_idx)
    {
        const int32_t original_color = static_cast<int32_t>(graph.get_vertex_color(vertex_idx));
        graph.set_vertex_color(vertex_idx, old_to_new.at(original_color));
    }
}

void PatternUtils::count_vertex_colors(
    const ColoredGraph& graph,
    std::vector<uint32_t>& counts,
    uint64_t& total_vertices)
{
    const uint32_t vertex_count = graph.vertex_count();
    for (uint32_t vertex_idx = 0U; vertex_idx < vertex_count; ++vertex_idx)
    {
        const uint32_t color = graph.get_vertex_color(vertex_idx);
        if (color < static_cast<uint32_t>(counts.size()))
        {
            counts[color]++;
        }
        total_vertices++;
    }
}

std::vector<double> PatternUtils::counts_to_probability(
    const std::vector<uint32_t>& counts,
    const uint64_t total_vertices)
{
    std::vector<double> probabilities(counts.size(), 0.0);
    if (total_vertices == 0U)
    {
        return probabilities;
    }
    const uint32_t color_count = static_cast<uint32_t>(counts.size());
    for (uint32_t color_idx = 0U; color_idx < color_count; ++color_idx)
    {
        probabilities[color_idx] =
            static_cast<double>(counts[color_idx]) / static_cast<double>(total_vertices);
    }
    return probabilities;
}

/* ---------- Public API ---------- */

std::vector<int32_t> PatternUtils::map_colors(ColoredGraph& graph)
{
    std::map<int32_t, uint32_t> old_to_new;
    std::vector<int32_t> color_map;

    scan_graph_colors(graph, old_to_new, color_map);
    recolor_graph(old_to_new, graph);

    return color_map;
}

std::vector<int32_t> PatternUtils::map_colors(ColoredGraph& first_graph, ColoredGraph& second_graph)
{
    std::map<int32_t, uint32_t> old_to_new;
    std::vector<int32_t> color_map;

    scan_graph_colors(first_graph, old_to_new, color_map);
    scan_graph_colors(second_graph, old_to_new, color_map);

    recolor_graph(old_to_new, first_graph);
    recolor_graph(old_to_new, second_graph);

    return color_map;
}

std::vector<int32_t> PatternUtils::map_colors(std::vector<ColoredGraph>& graphs)
{
    std::map<int32_t, uint32_t> old_to_new;
    std::vector<int32_t> color_map;

    for (ColoredGraph& graph : graphs)
    {
        scan_graph_colors(graph, old_to_new, color_map);
    }
    for (ColoredGraph& graph : graphs)
    {
        recolor_graph(old_to_new, graph);
    }

    return color_map;
}

void PatternUtils::recolor_pattern(
    BoostGraph& pattern,
    const std::vector<int32_t>& color_map)
{
    const uint32_t vertex_count = static_cast<uint32_t>(boost::num_vertices(pattern));
    for (uint32_t vertex_idx = 0U; vertex_idx < vertex_count; ++vertex_idx)
    {
        const uint32_t compact_color = pattern[vertex_idx].m_color;
        if (compact_color < static_cast<uint32_t>(color_map.size()))
        {
            pattern[vertex_idx].m_color = static_cast<uint32_t>(color_map[compact_color]);
        }
    }
}

std::vector<uint32_t> PatternUtils::find_initial_matches(
    const ColoredGraph& s_graph,
    const uint32_t color)
{
    std::vector<uint32_t> matches;
    const uint32_t vertex_count = s_graph.vertex_count();
    for (uint32_t vertex_idx = 0U; vertex_idx < vertex_count; ++vertex_idx)
    {
        if (s_graph.get_vertex_color(vertex_idx) == color)
        {
            matches.push_back(vertex_idx);
        }
    }
    return matches;
}

std::vector<std::vector<uint32_t>> PatternUtils::get_all_color_matches(
    const ColoredGraph& s_graph,
    const uint32_t color_count)
{
    std::vector<std::vector<uint32_t>> matches(color_count);
    const uint32_t vertex_count = s_graph.vertex_count();
    for (uint32_t vertex_idx = 0U; vertex_idx < vertex_count; ++vertex_idx)
    {
        const uint32_t color = s_graph.get_vertex_color(vertex_idx);
        if (color < color_count)
        {
            matches[color].push_back(vertex_idx);
        }
    }
    return matches;
}

std::vector<double> PatternUtils::compute_color_distribution(
    const uint32_t color_number,
    const int32_t s_size,
    const std::vector<ColoredGraph>& s_list)
{
    if (s_size <= 0)
    {
        return {};
    }

    std::vector<uint32_t> counts(color_number, 0U);
    uint64_t total_vertices = 0U;

    for (const ColoredGraph& graph : s_list)
    {
        count_vertex_colors(graph, counts, total_vertices);
    }

    return counts_to_probability(counts, total_vertices);
}

std::vector<double> PatternUtils::compute_color_distribution(
    const uint32_t color_number,
    const ColoredGraph& g)
{
    std::vector<uint32_t> counts(color_number, 0U);
    uint64_t total_vertices = 0U;

    count_vertex_colors(g, counts, total_vertices);

    return counts_to_probability(counts, total_vertices);
}

double PatternUtils::compute_density(
    const uint32_t vertex_count,
    const uint32_t edge_count)
{
    if (vertex_count < 2U)
    {
        return 0.0;
    }
    const double max_edges =
        static_cast<double>(vertex_count) * static_cast<double>(vertex_count - 1U) / 2.0;
    return static_cast<double>(edge_count) / max_edges;
}

void PatternUtils::add_edge(
    const bool is_directed,
    BoostGraph& graph,
    const uint32_t source,
    const uint32_t target)
{
    boost::add_edge(source, target, graph);
    if (!is_directed)
    {
        boost::add_edge(target, source, graph);
    }
}

}  // namespace sgf
