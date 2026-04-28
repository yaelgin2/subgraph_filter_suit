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
     * @brief Compute the raw edge-structure descriptor for a 4-node group.
     *
     * Encodes the presence or absence of each directed or undirected edge
     * among the four vertices into a compact bitmask used as the map key.
     *
     * @param group Four vertex identifiers forming the candidate group.
     * @param graph_adjacency_matrix Dense boolean adjacency matrix.
     * @return Raw edge-structure bitmask for the group.
     */
    uint32_t compute_motif_descriptor(const std::vector<uint32_t>& group,
                                      const std::vector<std::vector<bool>>& graph_adjacency_matrix) const;

    /**
     * @brief Encode a canonical motif number and a permuted color sequence into a 128-bit key.
     *
     * Packs the canonical motif number and the four remapped color values into
     * a single __uint128_t so that color-equivalent groups hash to the same key.
     *
     * @param canonical_motif_num The minimal motif number from the canonicalization map.
     * @param permuted_colors Color values reordered by the canonical permutation.
     * @return 128-bit motif identifier.
     */
    static __uint128_t encode_motif_key(uint32_t canonical_motif_num,
                                        const std::vector<uint32_t>& permuted_colors);
};

}  // namespace sgf
