#pragma once

#include "BoostGraph.h"
#include "ColoredGraph.h"

#include <cstdint>
#include <vector>

namespace sgf

{
/**
 * @brief Pure-static utilities shared by MultiGraphPatternFinder and
 *        SingleGraphPatternFinder.
 *
 * Colour remapping (compacting sparse colour values to dense 0..N-1 IDs) is
 * handled upstream by ColorRemapper and is performed once by the preprocessors
 * before graphs are handed to these finders.  PatternUtils covers the remaining
 * pattern-search operations: restoring original colours, matching, distribution
 * computation, and edge construction.
 */
class PatternUtils
{
public:
    /**
     * @brief Remap pattern vertex colours from compact IDs back to original values.
     *
     * Should be called on the output pattern just before returning it to the
     * caller, using the color_map produced by the corresponding map_colors call.
     *
     * @param pattern    BoostGraph whose vertex m_color fields hold compact IDs.
     * @param color_map  color_map[compact_id] == original colour value.
     */
    static void recolor_pattern(BoostGraph& pattern, const std::vector<int32_t>& color_map);

    /**
     * @brief Return all S-graph vertices whose colour equals @p color.
     *
     * @param graph  S-graph to search (colours must already be remapped).
     * @param color  Compact colour id to match.
     * @return Vertex indices in ascending order.
     */
    static std::vector<uint32_t> find_initial_matches(const ColoredGraph& graph, uint32_t color);

    /**
     * @brief Partition all S-graph vertices by colour in a single pass.
     *
     * More efficient than calling find_initial_matches for each colour
     * separately when many colours are needed at once.
     *
     * @param graph        S-graph to partition (colours must already be remapped).
     * @param color_count  Number of distinct compact colour IDs expected.
     * @return Vector of length color_count; result[c] = vertices with compact colour c.
     */
    static std::vector<std::vector<uint32_t>> get_all_color_matches(const ColoredGraph& graph,
                                                                    uint32_t color_count);

    /**
     * @brief Compute per-colour probability distribution across a list of graphs.
     *
     * Counts the total number of vertices of each colour across all graphs
     * and divides by the grand total.  Colours absent from all graphs get 0.0.
     *
     * @param color_count         Number of distinct (remapped) colours.
     * @param search_graph_count  Number of graphs in search_graphs to process.
     * @param search_graphs       Graphs whose vertex colours are tallied.
     * @return Vector of length color_count; element c = P(vertex has colour c).
     */
    static std::vector<double>
    compute_color_distribution(uint32_t color_count, int32_t search_graph_count,
                               const std::vector<ColoredGraph>& search_graphs);

    /**
     * @brief Compute per-colour probability distribution from a single graph.
     *
     * @param color_count   Number of distinct (remapped) colours.
     * @param source_graph  Graph whose vertex colours are tallied.
     * @return Vector of length color_count; element c = P(vertex has colour c).
     */
    static std::vector<double> compute_color_distribution(uint32_t color_count,
                                                          const ColoredGraph& source_graph);

    /**
     * @brief Compute undirected edge density = edge_count / (n*(n-1)/2).
     *
     * @param vertex_count  Number of vertices.
     * @param edge_count    Number of undirected edges.
     * @return Density in [0, 1], or 0.0 when vertex_count < 2.
     */
    static double compute_density(uint32_t vertex_count, uint32_t edge_count);

    /**
     * @brief Add an edge to a BoostGraph, respecting directedness.
     *
     * BoostGraph uses directed Boost adjacency_list internally.  For undirected
     * patterns both directions (source→target and target→source) must be added
     * explicitly; for directed patterns only the forward direction is added.
     *
     * @param is_directed  When false, adds both directions.
     * @param graph        BoostGraph to modify.
     * @param source       Source vertex descriptor.
     * @param target       Target vertex descriptor.
     */
    static void add_edge(bool is_directed, BoostGraph& graph, uint32_t source, uint32_t target);

private:
    /**
     * @brief Tally vertex colours in one graph and accumulate totals.
     *
     * @param graph           Graph to tally.
     * @param counts          Per-colour count vector, indexed by compact colour id.
     *                        Updated in-place (not reset — caller must initialise).
     * @param total_vertices  Running grand total, incremented by graph.vertex_count().
     */
    static void count_vertex_colors(const ColoredGraph& graph, std::vector<uint32_t>& counts,
                                    uint64_t& total_vertices);

    /**
     * @brief Convert per-colour vertex counts to a probability distribution.
     *
     * Each element becomes count[c] / total_vertices.  Returns 0.0 for any
     * colour whose count is zero or when total_vertices is zero.
     *
     * @param counts          Per-colour counts (indexed by compact colour id).
     * @param total_vertices  Total number of vertices across all tallied graphs.
     * @return Probability vector of the same length as counts.
     */
    static std::vector<double> counts_to_probability(const std::vector<uint32_t>& counts,
                                                     uint64_t total_vertices);
};

}  // namespace sgf
