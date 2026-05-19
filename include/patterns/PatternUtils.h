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
 * ### Colour remapping
 *
 * The scoring pipeline requires compact zero-based colour IDs so that
 * probability vectors can be indexed directly.  The map_colors overloads
 * build this mapping and rewrite vertex colours in-place.  The returned
 * color_map vector is the inverse: color_map[compact_id] == original_value.
 * Call recolor_pattern at the end to restore original colours in the output.
 */
class PatternUtils
{
public:
    /**
     * @brief Remap vertex colours in a single graph to compact zero-based IDs.
     *
     * Colours are assigned new IDs in first-encounter order (i.e. the colour
     * of vertex 0 becomes 0, the next distinct colour becomes 1, etc.).
     * The graph is modified in-place.
     *
     * @return color_map where color_map[new_id] == original colour value.
     */
    static std::vector<int32_t> map_colors(ColoredGraph& graph);

    /**
     * @brief Remap vertex colours across two graphs to a shared compact ID space.
     *
     * Both graphs are scanned together so they share the same compact IDs.
     * This is required when scoring patterns against a background graph:
     * the same colour must have the same ID in both graphs.
     *
     * @return color_map where color_map[new_id] == original colour value.
     */
    static std::vector<int32_t> map_colors(ColoredGraph& first_graph, ColoredGraph& second_graph);

    /**
     * @brief Remap vertex colours across multiple graphs to a shared compact ID space.
     *
     * All graphs are scanned first (building one global colour map), then all
     * are recoloured in a second pass.  This two-pass approach ensures the
     * returned color_map covers every colour seen across all graphs.
     *
     * @return color_map where color_map[new_id] == original colour value.
     */
    static std::vector<int32_t> map_colors(std::vector<ColoredGraph>& graphs);

    /**
     * @brief Remap pattern vertex colours from compact IDs back to original values.
     *
     * Should be called on the output pattern just before returning it to the
     * caller, using the color_map produced by the corresponding map_colors call.
     *
     * @param pattern    BoostGraph whose vertex m_color fields hold compact IDs.
     * @param color_map  color_map[compact_id] == original colour value.
     */
    static void recolor_pattern(BoostGraph&                 pattern,
                                const std::vector<int32_t>& color_map);

    /**
     * @brief Return all S-graph vertices whose colour equals @p color.
     *
     * @param graph  S-graph to search (colours must already be remapped).
     * @param color  Compact colour id to match.
     * @return Vertex indices in ascending order.
     */
    static std::vector<uint32_t> find_initial_matches(const ColoredGraph& graph,
                                                      uint32_t            color);

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
                                                                     uint32_t            color_count);

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
    static std::vector<double> compute_color_distribution(
        uint32_t                         color_count,
        int32_t                          search_graph_count,
        const std::vector<ColoredGraph>& search_graphs);

    /**
     * @brief Compute per-colour probability distribution from a single graph.
     *
     * @param color_count   Number of distinct (remapped) colours.
     * @param source_graph  Graph whose vertex colours are tallied.
     * @return Vector of length color_count; element c = P(vertex has colour c).
     */
    static std::vector<double> compute_color_distribution(
        uint32_t            color_count,
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
     * @brief Scan one graph's vertex colours and extend the compact colour mapping.
     *
     * For each vertex whose colour has not been seen before, a new compact ID
     * is assigned (next available slot) and the original colour is appended to
     * color_map.  Already-seen colours are skipped.
     *
     * @param graph                     Graph to scan.
     * @param original_to_remapped_color  Map updated in-place: original_color → compact_id.
     * @param color_map                 Inverse map updated in-place: compact_id → original_color.
     */
    static void scan_graph_colors(const ColoredGraph&          graph,
                                  std::map<int32_t, uint32_t>& original_to_remapped_color,
                                  std::vector<int32_t>&        color_map);

    /**
     * @brief Rewrite vertex colours in a graph using the compact mapping.
     *
     * Replaces each vertex's original colour with its compact ID.
     * Throws std::out_of_range if a vertex colour is absent from the map.
     *
     * @param original_to_remapped_color  original_color → compact_id.
     * @param graph                       Graph to recolor in-place.
     */
    static void recolor_graph(const std::map<int32_t, uint32_t>& original_to_remapped_color,
                              ColoredGraph&                       graph);

    /**
     * @brief Tally vertex colours in one graph and accumulate totals.
     *
     * @param graph           Graph to tally.
     * @param counts          Per-colour count vector, indexed by compact colour id.
     *                        Updated in-place (not reset — caller must initialise).
     * @param total_vertices  Running grand total, incremented by graph.vertex_count().
     */
    static void count_vertex_colors(const ColoredGraph&    graph,
                                    std::vector<uint32_t>& counts,
                                    uint64_t&              total_vertices);

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
                                                     uint64_t                     total_vertices);
};

} // namespace sgf
