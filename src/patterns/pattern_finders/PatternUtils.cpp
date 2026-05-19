#include "PatternUtils.h"

#include "BoostGraph.h"
#include "ColoredGraph.h"

#include <boost/graph/adjacency_list.hpp>
#include <boost/range/iterator_range_core.hpp>
#include <cstdint>
#include <map>
#include <vector>

/* ---------- private helpers ---------- */

namespace sgf
{
void PatternUtils::scan_graph_colors(const ColoredGraph& graph,
                                     std::map<int32_t, uint32_t>& old_to_new,
                                     std::vector<int32_t>& color_map)
{
    for (uint32_t vertex = 0U; vertex < graph.vertex_count(); ++vertex)
    {
        const int32_t original_color = static_cast<int32_t>(graph.get_vertex_color(vertex));
        if (old_to_new.count(original_color) == 0U)
        {
            old_to_new[original_color] = static_cast<uint32_t>(color_map.size());
            color_map.push_back(original_color);
        }
    }
}

void PatternUtils::recolor_graph(const std::map<int32_t, uint32_t>& old_to_new, ColoredGraph& graph)
{
    for (uint32_t vertex = 0U; vertex < graph.vertex_count(); ++vertex)
    {
        graph.set_vertex_color(vertex,
                               old_to_new.at(static_cast<int32_t>(graph.get_vertex_color(vertex))));
    }
}

/* ---------- public interface ---------- */

std::vector<int32_t> PatternUtils::map_colors(ColoredGraph& graph)
{
    std::vector<int32_t> color_map;
    std::map<int32_t, uint32_t> old_to_new;
    scan_graph_colors(graph, old_to_new, color_map);
    recolor_graph(old_to_new, graph);
    return color_map;
}

std::vector<int32_t> PatternUtils::map_colors(ColoredGraph& graph_a, ColoredGraph& graph_b)
{
    std::vector<int32_t> color_map;
    std::map<int32_t, uint32_t> old_to_new;
    scan_graph_colors(graph_a, old_to_new, color_map);
    scan_graph_colors(graph_b, old_to_new, color_map);
    recolor_graph(old_to_new, graph_a);
    recolor_graph(old_to_new, graph_b);
    return color_map;
}

std::vector<int32_t> PatternUtils::map_colors(std::vector<ColoredGraph>& graphs)
{
    std::vector<int32_t> color_map;
    std::map<int32_t, uint32_t> old_to_new;
    for (const auto& graph : graphs)
    {
        scan_graph_colors(graph, old_to_new, color_map);
    }
    for (auto& graph : graphs)
    {
        recolor_graph(old_to_new, graph);
    }
    return color_map;
}

void PatternUtils::recolor_pattern(BoostGraph& pattern, const std::vector<int32_t>& color_map)
{
    for (auto vertex : boost::make_iterator_range(vertices(pattern)))
    {
        pattern[vertex].m_color = static_cast<uint32_t>(color_map[pattern[vertex].m_color]);
    }
}

std::vector<uint32_t> PatternUtils::find_initial_matches(const ColoredGraph& graph,
                                                         const uint32_t color)
{
    std::vector<uint32_t> matches;
    for (uint32_t vertex = 0U; vertex < graph.vertex_count(); ++vertex)
    {
        if (graph.get_vertex_color(vertex) == color)
        {
            matches.push_back(vertex);
        }
    }
    return matches;
}

void PatternUtils::count_vertex_colors(const ColoredGraph& graph, std::vector<uint32_t>& counts,
                                       uint64_t& total_vertices)
{
    for (uint32_t vertex = 0U; vertex < graph.vertex_count(); ++vertex)
    {
        counts[graph.get_vertex_color(vertex)]++;
        ++total_vertices;
    }
}

std::vector<std::vector<uint32_t>> PatternUtils::get_all_color_matches(const ColoredGraph& graph,
                                                                       const uint32_t color_count)
{
    std::vector<std::vector<uint32_t>> all_matches(color_count);
    for (uint32_t vertex = 0U; vertex < graph.vertex_count(); ++vertex)
    {
        const int32_t color = static_cast<int32_t>(graph.get_vertex_color(vertex));
        if (color >= 0 && color < static_cast<int32_t>(color_count))
        {
            all_matches[static_cast<uint32_t>(color)].push_back(vertex);
        }
    }
    return all_matches;
}

std::vector<double> PatternUtils::counts_to_probability(const std::vector<uint32_t>& counts,
                                                        const uint64_t total_vertices)
{
    std::vector<double> probability(counts.size());
    for (uint32_t color_index = 0U; color_index < static_cast<uint32_t>(counts.size());
         ++color_index)
    {
        probability[color_index] =
            (total_vertices > 0U && counts[color_index] > 0U)
                ? static_cast<double>(counts[color_index]) / static_cast<double>(total_vertices)
                : 0.0;
    }
    return probability;
}

std::vector<double>
PatternUtils::compute_color_distribution(const uint32_t color_number, const int32_t s_size,
                                         const std::vector<ColoredGraph>& s_list)
{
    std::vector<uint32_t> counts(color_number, 0U);
    uint64_t total_vertices = 0U;
    for (int32_t graph_index = 0; graph_index < s_size; ++graph_index)
    {
        count_vertex_colors(s_list[static_cast<uint32_t>(graph_index)], counts, total_vertices);
    }
    return counts_to_probability(counts, total_vertices);
}

std::vector<double> PatternUtils::compute_color_distribution(const uint32_t color_number,
                                                             const ColoredGraph& graph)
{
    std::vector<uint32_t> counts(color_number, 0U);
    uint64_t total_vertices = 0U;
    count_vertex_colors(graph, counts, total_vertices);
    return counts_to_probability(counts, total_vertices);
}

double PatternUtils::compute_density(const uint32_t vertex_count, const uint32_t edge_count)
{
    if (vertex_count < 2U)
    {
        return 0.0;
    }
    const uint64_t max_possible_edges =
        static_cast<uint64_t>(vertex_count) * (vertex_count - 1U) / 2U;
    if (max_possible_edges == 0U)
    {
        return 0.0;
    }
    return static_cast<double>(edge_count) / static_cast<double>(max_possible_edges);
}

void PatternUtils::add_edge(const bool is_directed, BoostGraph& graph, const uint32_t source,
                            const uint32_t target)
{
    boost::add_edge(source, target, graph);
    if (!is_directed)
    {
        boost::add_edge(target, source, graph);
    }
}
}  // namespace sgf
