#pragma once

#include "ColoredGraph.h"
#include "BoostGraph.h"

#include <vector>
#include <map>
#include <cstdint>

namespace sgf
{
/**
 * @brief Pure-static utilities shared by MultiGraphPatternFinder and
 *        SingleGraphPatternFinder.
 *
 * Extracted here so neither finder has to depend on the other's header.
 */
class PatternUtils {
public:
    /**
     * @brief Remap vertex colours in a single graph to compact
     *        zero-based IDs, modifying the graph in-place.
     *
     * @return color_map  color_map[new_id] == original colour value.
     */
    static std::vector<int32_t> map_colors(ColoredGraph& graph);

    /**
     * @brief Remap vertex colours across two graphs to a shared
     *        compact zero-based ID space, modifying both in-place.
     *
     * @return color_map  color_map[new_id] == original colour value.
     */
    static std::vector<int32_t> map_colors(ColoredGraph& a, ColoredGraph& b);

    /**
     * @brief Remap vertex colours across multiple graphs to a shared
     *        compact zero-based ID space, modifying all graphs in-place.
     *
     * @return color_map  color_map[new_id] == original colour value.
     */
    static std::vector<int32_t> map_colors(std::vector<ColoredGraph>& graphs);

    /**
     * @brief Remap pattern vertex colours from compact IDs back to the
     *        original colour values using the map returned by map_colors.
     */
    static void recolor_pattern(BoostGraph&                  pattern,
                                const std::vector<int32_t>&  color_map);

    /**
     * @brief Return the index of every S-vertex whose colour equals @p color.
     */
    static std::vector<uint32_t> find_initial_matches(const ColoredGraph& s,
                                                      uint32_t     color);

    /**
     * @brief Get all matches for all colors in one pass through the graph.
     * @return Vector where result[color] = vector of vertices with that color.
     */
    static std::vector<std::vector<uint32_t>> get_all_color_matches(const ColoredGraph& s, uint32_t color_count);

    /**
     * @brief Compute per-colour probability distribution across all S-graphs.
     *
     * @return Vector of size @p color_number where element c is the fraction
     *         of all vertices (across all S-graphs) that have colour c.
     */
    static std::vector<double> compute_color_distribution(
        uint32_t                    color_number,
        int32_t                     s_size,
        const std::vector<ColoredGraph>&   s_list);

    /**
     * @brief Compute per-colour probability distribution from a single graph.
     */
    static std::vector<double> compute_color_distribution(
        uint32_t       color_number,
        const ColoredGraph&   g);

    /**
     * @brief Compute edge density = edge_count / (vertex_count*(vertex_count-1)/2).
     *
     * @return density in [0,1], or 0.0 when vertex_count < 2.
     */
    static double compute_density(uint32_t vertex_count, uint32_t edge_count);

    /*
    * Adds edge to boost graph, both sides if the graph is undirected, just one if it is directed.
    */
    static void add_edge(bool is_directed, BoostGraph& graph, uint32_t source, uint32_t target); 

private:
    static void scan_graph_colors(const ColoredGraph& graph,
                                  std::map<int32_t, uint32_t>& old_to_new,
                                  std::vector<int32_t>& color_map);

    static void recolor_graph(const std::map<int32_t, uint32_t>& old_to_new,
                              ColoredGraph& graph);

    static void count_vertex_colors(const ColoredGraph& graph,
                                    std::vector<uint32_t>& counts,
                                    uint64_t& total_vertices);

    static std::vector<double> counts_to_probability(
        const std::vector<uint32_t>& counts,
        uint64_t total_vertices);

};

}
