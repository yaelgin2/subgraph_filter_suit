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
 * Extends GroupEnumerationPreprocessor to enumerate all 4-node induced
 * subgraphs, canonicalize each via color permutation using the precomputed
 * DIRECTED_MOTIF_CANONICAL_MAP / UNDIRECTED_MOTIF_CANONICAL_MAP, and count
 * occurrences per canonical motif identifier.
 *
 * The canonical motif identifier encodes both the edge structure and the
 * canonical color assignment, making it invariant to color permutations that
 * preserve the motif shape.
 */
class MotifPreprocessor : public GroupEnumerationPreprocessor
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
                                  const GroupCounterCallback& count_group) const override;

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
    UInt128 calculate_motif_number(uint32_t motif_descriptor,
                                  const std::vector<uint32_t>& node_colors) const override;

private:
    /**
     * @brief Shared mutable state for one Kavosh BFS run rooted at a single vertex.
     *
     * Groups all arguments needed by every enumeration helper so they can be
     * forwarded as a single parameter rather than repeated on every call.
     */
    struct KavoshContext
    {
        const std::vector<std::vector<bool>>& m_adjacency_matrix;  ///< Full graph adjacency matrix.
        const GroupCounterCallback& m_count_group;   ///< Callback for emitting groups.
        const std::vector<bool>& m_ignore_vertices;  ///< Already-processed vertex mask.
        std::vector<int64_t>& m_bfs_visited;         ///< BFS depth-encoding array.
        int64_t m_run_id;                            ///< Root-unique run identifier.
        uint32_t m_root;                             ///< Current root vertex.
    };

    /**
     * @brief A half-open iterator range over a vertex's sorted neighbour list.
     *
     * For directed graphs, @p rev_begin / @p rev_end carry the incoming neighbours so
     * callers can traverse both directions without extra allocation.
     * Set rev_begin == rev_end (empty) for undirected graphs.
     */
    struct NeighbourRange
    {
        std::vector<uint32_t>::const_iterator m_begin;  ///< First outgoing neighbour.
        std::vector<uint32_t>::const_iterator m_end;    ///< One past last outgoing neighbour.
        std::vector<uint32_t>::const_iterator
            m_rev_begin;  ///< First incoming neighbour (directed only).
        std::vector<uint32_t>::const_iterator m_rev_end;  ///< One past last incoming neighbour.
    };

    /// Low 2 bits of each bfs_visited entry encode BFS depth (0–3); upper bits hold run_id.
    static constexpr uint64_t BFS_DEPTH_TWO_OFFSET = 2;
    /// Encodes depth-3 in the low 2 bits of a bfs_visited entry.
    static constexpr uint64_t BFS_DEPTH_THREE_OFFSET = 3;
    /// Right-shift applied to a bfs_visited entry to recover the run identifier (= root vertex id).
    static constexpr uint64_t BFS_VERTEX_RUN_SHIFT = 2;

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
    uint32_t
    compute_motif_descriptor(const std::vector<uint32_t>& group,
                             const std::vector<std::vector<bool>>& graph_adjacency_matrix) const;

    /**
     * @brief Enumerate one root vertex's 4-node groups across all Kavosh BFS depth variations.
     *
     * Entry point for all four (1,1,1), (1,1,2), (1,2,2), (1,2,3) sub-enumerations.
     * Marks the root vertex in @p bfs_visited_vertices and then delegates to
     * specialised helpers for each depth variation.
     *
     * @param graph_adjacency_matrix Dense boolean adjacency matrix of the graph.
     * @param count_group Callback invoked for each discovered group.
     * @param visited_vertices_to_ignore Vertices already fully processed (ignored as group
     * members).
     * @param bfs_visited_vertices Depth-encoding array shared across all root iterations.
     * @param root The vertex currently acting as root for BFS enumeration.
     */
    void stream_groups_to_counter_for_vertex(
        const std::vector<std::vector<bool>>& graph_adjacency_matrix,
        const GroupCounterCallback& count_group,
        const std::vector<bool>& visited_vertices_to_ignore,
        std::vector<int64_t>& bfs_visited_vertices, uint32_t root) const;

    /**
     * @brief Mark every depth-1 neighbour of root in the BFS-visited array.
     *
     * @param ctx Shared run context; bfs_visited is updated in place.
     * @param depth_one Iterator range over root's direct neighbours.
     */
    void mark_depth_one_neighbours(KavoshContext& ctx, const NeighbourRange& depth_one) const;

    /**
     * @brief Emit all groups formed by root and three distinct depth-1 neighbours.
     *
     * Implements the (1, 1, 1) Kavosh depth variation.
     *
     * @param ctx Shared run context.
     * @param depth_one Iterator range over root's direct neighbours.
     */
    void emit_depth_1_1_1_groups(const KavoshContext& ctx, const NeighbourRange& depth_one) const;

    /**
     * @brief Emit (1,1,1) groups for a fixed first depth-1 neighbour, iterating remaining pairs.
     *
     * @param ctx Shared run context.
     * @param depth_one Iterator range over root's direct neighbours.
     * @param first_neighbour The chosen first depth-1 vertex.
     * @param is_first_neighbour_reversed True if first_neighbour was reached via a reverse edge.
     */
    void emit_depth_1_1_1_groups_first_vertex_chosen(
        const KavoshContext& ctx, const NeighbourRange& depth_one,
        std::vector<uint32_t>::const_iterator first_neighbour,
        bool is_first_neighbour_reversed) const;

    /**
     * @brief Emit (1,1,1) groups for fixed first and second depth-1 neighbours.
     *
     * @param ctx Shared run context.
     * @param depth_one Iterator range over root's direct neighbours.
     * @param first_neighbour The chosen first depth-1 vertex.
     * @param second_neighbour The chosen second depth-1 vertex.
     * @param is_second_neighbour_reversed True if second_neighbour was reached via a reverse edge.
     */
    void emit_depth_1_1_1_groups_second_vertex_chosen(
        const KavoshContext& ctx, const NeighbourRange& depth_one,
        std::vector<uint32_t>::const_iterator first_neighbour,
        std::vector<uint32_t>::const_iterator second_neighbour,
        bool is_second_neighbour_reversed) const;

    /**
     * @brief Mark neighbours of a depth-1 vertex as BFS depth-2 if not yet seen in this run.
     *
     * @param ctx Shared run context; bfs_visited is updated in place.
     * @param depth_two Iterator range over the depth-1 vertex's neighbours.
     */
    void mark_depth_two_neighbours(KavoshContext& ctx, const NeighbourRange& depth_two) const;

    /**
     * @brief Emit groups: root + first_neighbour (depth-1) + n11 (depth-1) + n2 (depth-2).
     *
     * Implements the (1, 1, 2) Kavosh depth variation for a fixed depth-1 anchor.
     * Double-counting when an n11–n2 edge exists is resolved by emitting only
     * when no such edge exists, or when first_neighbour < n11.
     *
     * @param ctx Shared run context.
     * @param first_neighbour The depth-1 anchor vertex (n1).
     * @param depth_one All of root's depth-1 neighbours (candidates for n11).
     * @param depth_two All neighbours of first_neighbour (depth-2 candidates for n2).
     */
    void emit_depth_1_1_2_for_first_vertex(const KavoshContext& ctx,
                                           std::vector<uint32_t>::const_iterator first_neighbour,
                                           const NeighbourRange& depth_one,
                                           const NeighbourRange& depth_two) const;

    /**
     * @brief Emit (1,1,2) groups for a single fixed n2 vertex against all n11 candidates.
     *
     * Iterates both forward and reverse halves of @p depth_one to cover all combined
     * depth-1 neighbours without allocating an intermediate list.
     *
     * @param ctx Shared run context.
     * @param first_neighbour The depth-1 anchor (n1).
     * @param depth_one Combined depth-1 range (fwd + rev).
     * @param second_neighbour The fixed depth-2 vertex.
     */
    void emit_depth_1_1_2_for_second_vertex(
        const KavoshContext& ctx, std::vector<uint32_t>::const_iterator first_neighbour,
        const NeighbourRange& depth_one,
        std::vector<uint32_t>::const_iterator second_neighbour) const;

    /**
     * @brief Build combined depth-2 range for @p first_neighbour and run (1,1,2)/(1,2,2) emission.
     *
     * Factored out of emit_depth_1_1_2_and_1_2_2_groups to be shared by both the forward
     * and reverse depth-1 loops without duplicating the depth-2 construction logic.
     *
     * @param ctx Shared run context; bfs_visited is updated in place.
     * @param first_neighbour The depth-1 anchor being processed.
     * @param depth_one Combined depth-1 range used as n11 candidates.
     */
    void process_first_neighbour_112_122(KavoshContext& ctx,
                                         std::vector<uint32_t>::const_iterator first_neighbour,
                                         const NeighbourRange& depth_one) const;

    /**
     * @brief Emit groups: root + first_neighbour (depth-1) + two distinct depth-2 vertices.
     *
     * Implements the (1, 2, 2) Kavosh depth variation for a fixed depth-1 anchor.
     * Only vertices marked as BFS depth-2 in this run are selected as the pair.
     *
     * @param ctx Shared run context.
     * @param first_neighbour The depth-1 anchor vertex (n1).
     * @param depth_two All neighbours of first_neighbour (pool for depth-2 pair selection).
     */
    void emit_depth_1_2_2_for_first_vertex(const KavoshContext& ctx,
                                           std::vector<uint32_t>::const_iterator first_neighbour,
                                           const NeighbourRange& depth_two) const;

    /**
     * @brief Emit (1,2,2) groups for a fixed first depth-2 vertex against remaining candidates.
     *
     * @param ctx Shared run context.
     * @param first_neighbour The depth-1 anchor.
     * @param depth_two Combined depth-2 range (fwd + rev) for pair selection.
     * @param second_neighbour The chosen first depth-2 vertex.
     * @param is_second_vertex_reversed True if second_neighbour was reached via a reverse edge.
     */
    void emit_depth_1_2_2_for_second_vertex(const KavoshContext& ctx,
                                            std::vector<uint32_t>::const_iterator first_neighbour,
                                            const NeighbourRange& depth_two,
                                            std::vector<uint32_t>::const_iterator second_neighbour,
                                            bool is_second_vertex_reversed) const;

    /**
     * @brief For each depth-1 anchor, mark depth-2 reachability then emit (1,1,2) and (1,2,2)
     * groups.
     *
     * Outer driver for both depth variations that share the same depth-1 anchor loop.
     *
     * @param ctx Shared run context; bfs_visited is updated in place.
     * @param depth_one Iterator range over root's direct neighbours.
     */
    void emit_depth_1_1_2_and_1_2_2_groups(KavoshContext& ctx,
                                           const NeighbourRange& depth_one) const;

    /**
     * @brief Enumerate BFS-depth-2 neighbours of n1 and delegate per-n2 group emission.
     *
     * Middle loop of the (1, 2, 3) Kavosh variation for a fixed depth-1 vertex.
     *
     * @param ctx Shared run context; bfs_visited may be updated.
     * @param first_degree_vertex The depth-1 anchor (n1).
     * @param second_degree Iterator range over n1's neighbours (depth-2 candidates).
     */
    void emit_depth_1_2_3_for_first_vertex(KavoshContext& ctx, uint32_t first_degree_vertex,
                                           const NeighbourRange& second_degree) const;

    /**
     * @brief Emit groups: root + n1 + n2 + n3 for each candidate third-degree vertex.
     *
     * Innermost loop of the (1, 2, 3) Kavosh variation for fixed n1 and n2.
     * Marks new vertices as depth-3; emits for new, depth-2-without-back-edge, and
     * depth-3 candidates; skips depth-1 back-edges.
     *
     * @param ctx Shared run context; bfs_visited may be updated.
     * @param first_degree_vertex The depth-1 anchor (n1).
     * @param second_degree_vertex The depth-2 anchor (n2).
     * @param third_degree Iterator range over n2's neighbours (candidates for n3).
     */
    void emit_depth_1_2_3_for_second_vertex(KavoshContext& ctx, uint32_t first_degree_vertex,
                                            uint32_t second_degree_vertex,
                                            const NeighbourRange& third_degree) const;

    /**
     * @brief Emit one (1,2,3) group for a single n3 candidate, updating bfs_visited as needed.
     *
     * Shared by both the forward and reverse n3 loops in emit_depth_1_2_3_for_second_vertex.
     *
     * @param ctx Shared run context; bfs_visited may be updated.
     * @param first_degree_vertex The depth-1 anchor.
     * @param second_degree_vertex The depth-2 anchor.
     * @param third_degree_vertex The candidate depth-3 vertex.
     */
    void emit_depth_1_2_3_for_third_vertex(KavoshContext& ctx, uint32_t first_degree_vertex,
                                           uint32_t second_degree_vertex,
                                           uint32_t third_degree_vertex) const;

    /**
     * @brief Outermost driver for the (1, 2, 3) Kavosh depth variation.
     *
     * Iterates non-ignored depth-1 neighbours of root and delegates to
     * emit_depth_1_2_3_for_first_vertex for each.
     *
     * @param ctx Shared run context; bfs_visited may be updated.
     * @param depth_one Iterator range over root's direct neighbours.
     */
    void emit_depth_1_2_3_groups(KavoshContext& ctx, const NeighbourRange& depth_one) const;
};

}  // namespace sgf
