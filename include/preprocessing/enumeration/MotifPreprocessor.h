#pragma once

#include "ColoredGraph.h"
#include "GroupEnumerationPreprocessor.h"
#include "LoggerHandler.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace sgf
{

/**
 * @class MotifPreprocessor
 * @brief Computes 4-node motif frequency signatures for a colored graph.
 *
 * Extends GroupEnmerationPreprocessor to enumerate all 4-node induced
 * subgraphs, canonicalize each via color permutation using the precomputed
 * DIRECTED_MOTIF_CANONICAL_MAP / UNDIRECTED_MOTIF_CANONICAL_MAP, and count
 * occurrences per canonical motif identifier.
 *
 * The canonical motif identifier encodes both the edge structure and the
 * canonical color assignment, making it invariant to color permutations that
 * preserve the motif shape.
 */
class MotifPreprocessor : public GroupEnmerationPreprocessor
{
public:
    /**
     * @brief Construct a MotifPreprocessor for the given graph.
     *
     * @param graph The colored graph to preprocess.
     * @param logger Logger handler for status and debug output.
     */
    MotifPreprocessor(const ColoredGraph& graph, LoggerHandler logger);

    MotifPreprocessor() = delete;
    MotifPreprocessor(const MotifPreprocessor&) = delete;
    MotifPreprocessor& operator=(const MotifPreprocessor&) = delete;
    MotifPreprocessor(MotifPreprocessor&&) = delete;
    MotifPreprocessor& operator=(MotifPreprocessor&&) = delete;

    /**
     * @brief Default destructor.
     */
    ~MotifPreprocessor() override = default;

protected:
    /**
     * @brief Sort graph nodes by degree (descending) to improve enumeration pruning.
     *
     * Populates m_node_order with vertex indices ordered by decreasing degree.
     * Higher-degree nodes are processed first, which reduces the number of
     * candidate groups that need full evaluation.
     */
    void sort_nodes() override;

    /**
     * @brief Enumerate all 4-node induced subgraphs and report each via callback.
     *
     * Iterates over ordered vertex combinations, checks connectivity of each
     * induced subgraph, and invokes @p count_group once per valid group.
     * No group collection is materialized in memory.
     *
     * @param graph_adjacency_matrix Dense boolean adjacency matrix of the graph.
     * @param count_group Callback invoked for each discovered group.
     */
    void stream_groups_to_counter(const std::vector<std::vector<bool>>& graph_adjacency_matrix,
                                  const GroupCounterCallback& count_group) override;

    /**
     * @brief Canonicalize a 4-node group into a unique motif identifier.
     *
     * Looks up @p motif_descriptor in the appropriate canonicalization map
     * (directed or undirected), applies each stored color permutation to
     * @p node_colors, and encodes the result into a 128-bit identifier that
     * is invariant to equivalent color relabelings.
     *
     * @param motif_descriptor Raw edge-structure number for the group.
     * @param node_colors Color labels of the four vertices in group order.
     * @return Canonical 128-bit motif identifier.
     */
    __uint128_t calculate_motif_number(uint32_t motif_descriptor,
                                       const std::vector<uint32_t>& node_colors) override;

private:
    /**
     * @brief Reference to the graph being processed.
     */
    const ColoredGraph& m_graph;

    /**
     * @brief Vertex traversal order, populated by sort_nodes().
     */
    std::vector<uint32_t> m_node_order;

    /**
     * @brief Encode the edge structure of a 4-node group as an integer bitmask.
     *
     * Reads the group's induced adjacency matrix row by row (row 0 first, column
     * 0 leftmost) and concatenates the edge-presence bits into a single integer,
     * with the first bit read becoming the MSB.
     *
     * The diagonal is always skipped (self-loops are not supported).
     * For undirected graphs the lower triangle is also skipped, so each pair
     * is represented by exactly one bit.
     *
     * Directed example — 4 nodes, edge (1→0) only:
     * @verbatim
     *   read order: (0,1)(0,2)(0,3)(1,0)(1,2)(1,3)(2,0)(2,1)(2,3)(3,0)(3,1)(3,2)
     *   bits:        0    0    0    1    0    0    0    0    0    0    0    0
     *   result:      0b000100000000 = 256
     * @endverbatim
     *
     * Undirected example — 4 nodes, edge (0,1) only:
     * @verbatim
     *   read order: (0,1)(0,2)(0,3)(1,2)(1,3)(2,3)
     *   bits:        1    0    0    0    0    0
     *   result:      0b100000 = 32
     * @endverbatim
     *
     * @param group Global vertex IDs of the four group members in traversal order.
     * @param graph_adjacency_matrix Dense boolean adjacency matrix of the full graph.
     * @return Integer whose bits encode edge presence, MSB = first pair read.
     */
    uint32_t compute_motif_descriptor(const std::vector<uint32_t>& group,
                                      const std::vector<std::vector<bool>>& graph_adjacency_matrix) const;

    void stream_groups_to_counter_for_vertex(const std::vector<std::vector<bool>>& graph_adjacency_matrix,
        const GroupCounterCallback& count_group,
        const std::vector<bool>& visited_vertices_to_ignore,
        std::vector<uint64_t>& bfs_visited_vertices,
        uint32_t root);

    };
}  // namespace sgf
