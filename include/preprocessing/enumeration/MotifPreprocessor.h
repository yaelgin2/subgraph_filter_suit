#pragma once

#ifdef SGF_CUDA_ENABLED
#define SGF_HD __host__ __device__
#else
#define SGF_HD
#endif

#include "ColoredGraph.h"
#include "Constants.h"
#include "GroupEnumerationPreprocessor.h"
#include "LoggerHandler.h"

#include <cstdint>
#include <utility>
#include <vector>

#ifdef SGF_CUDA_ENABLED
#include "CudaMotifBackend.h"
#endif

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
     * @param thread_number Maximum number of threads to use during enumeration.
     */
    MotifPreprocessor(const ColoredGraph& graph, LoggerHandler logger,
                      uint32_t thread_number = SgfConstants::DEFAULT_THREAD_NUMBER);

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
     * @param graph_adjacency_matrix Dense boolean adjacency matrix of the graph.
     */
    EnumerationResult stream_groups_to_counter(
        const std::vector<std::vector<bool>>& graph_adjacency_matrix) const override;

    /**
     * @brief Canonicalize a 4-node group into a unique motif identifier.
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
     */
    struct CpuKavoshContext
    {
        const std::vector<std::vector<bool>>& m_adjacency_matrix;  ///< Full graph adjacency matrix.
        const GroupCounterCallback& m_count_group;  ///< Callback for emitting groups.
        std::vector<int64_t>& m_bfs_visited;        ///< BFS depth-encoding array.
        int64_t m_run_id;                           ///< Root-unique run identifier.
        uint32_t m_root;                            ///< Current root vertex.

        /**
         * @brief Return true if @p dest is a forward neighbor of @p src.
         */
        bool has_fwd_edge(const uint32_t src, const uint32_t dest) const
        {
            return m_adjacency_matrix[src][dest];
        }

        /**
         * @brief Return true if @p vertex was marked at @p depth in this run.
         */
        bool is_at_depth(const uint32_t vertex, const int64_t depth) const
        {
            return m_bfs_visited[vertex] == m_run_id + depth;
        }

        /**
         * @brief Mark @p vertex at @p depth for this run.
         */
        void mark_at_depth(const uint32_t vertex, const int64_t depth)
        {
            m_bfs_visited[vertex] = m_run_id + depth;
        }
    };

    /**
     * @brief A half-open iterator range over a vertex's sorted neighbour list.
     *
     * For directed graphs, @p rev_begin / @p rev_end carry incoming neighbours.
     * Set rev_begin == rev_end for undirected graphs.
     */
    struct CpuNeighbourRange
    {
        std::vector<uint32_t>::const_iterator m_begin;     ///< First outgoing neighbour.
        std::vector<uint32_t>::const_iterator m_end;       ///< One past last outgoing neighbour.
        std::vector<uint32_t>::const_iterator m_rev_begin; ///< First incoming neighbour (directed).
        std::vector<uint32_t>::const_iterator m_rev_end;   ///< One past last incoming neighbour.
    };

    /** @brief Iterator pair returned by ColoredGraph::get_neighbours(). */
    using NeighbourIteratorPair = std::pair<std::vector<uint32_t>::const_iterator,
                                            std::vector<uint32_t>::const_iterator>;

    /**
     * @brief Maps each vertex to its position in @c m_node_order.
     */
    std::vector<uint32_t> m_order_index;

    /**
     * @brief Build @c m_node_order via the base sort, then derive @c m_order_index.
     */
    void sort_nodes() override;

    /// Low 2 bits of each bfs_visited entry encode BFS depth; upper bits hold run_id.
    static constexpr uint64_t BFS_DEPTH_TWO_OFFSET = 2;
    /// Encodes depth-3 in the low 2 bits of a bfs_visited entry.
    static constexpr uint64_t BFS_DEPTH_THREE_OFFSET = 3;
    /// Right-shift to recover the run identifier from a bfs_visited entry.
    static constexpr uint64_t BFS_VERTEX_RUN_SHIFT = 2;

    /**
     * @brief Returns "motifs" to label the finished-enumeration log line.
     * @return The string "motifs".
     */
    [[nodiscard]] std::string entity_name() const override;

    /**
     * @brief Encode the edge structure of a 4-node group as an integer bitmask.
     *
     * @param group Global vertex IDs of the four group members in traversal order.
     * @param graph_adjacency_matrix Dense boolean adjacency matrix of the full graph.
     * @return Integer whose bits encode edge presence, MSB = first pair read.
     */
    uint32_t compute_motif_descriptor(
        const std::vector<uint32_t>& group,
        const std::vector<std::vector<bool>>& graph_adjacency_matrix) const;

    /**
     * @brief Enumerate one root vertex's 4-node groups across all Kavosh BFS depth variations.
     *
     * @param graph_adjacency_matrix Dense boolean adjacency matrix of the graph.
     * @param count_group Callback invoked for each discovered group.
     * @param bfs_visited_vertices Depth-encoding array shared across all root iterations.
     * @param root The vertex currently acting as root for BFS enumeration.
     */
    void stream_groups_to_counter_for_vertex(
        const std::vector<std::vector<bool>>& graph_adjacency_matrix,
        const GroupCounterCallback& count_group,
        std::vector<int64_t>& bfs_visited_vertices,
        uint32_t root) const;

    /**
     * @brief Mark every depth-1 neighbour of root in the BFS-visited array.
     *
     * @param ctx Shared run context; bfs_visited is updated in place.
     * @param depth_one Iterator range over root's direct neighbours.
     */
    void mark_depth_one_neighbours(CpuKavoshContext& ctx,
                                   const CpuNeighbourRange& depth_one) const;

    /**
     * @brief Mark neighbours of a depth-1 vertex as BFS depth-2 if not yet seen in this run.
     *
     * @param ctx Shared run context; bfs_visited is updated in place.
     * @param depth_two Iterator range over the depth-1 vertex's neighbours.
     */
    void mark_depth_two_neighbours(CpuKavoshContext& ctx,
                                   const CpuNeighbourRange& depth_two) const;

    // ── CPU-only outer driver declarations ──────────────────────────────────

    /**
     * @brief Emit all groups formed by root and three distinct depth-1 neighbours.
     *
     * @param ctx Shared run context.
     * @param depth_one Iterator range over root's direct neighbours.
     */
    void emit_depth_1_1_1_groups_cpu(const CpuKavoshContext& ctx,
                                     const CpuNeighbourRange& depth_one) const;

    /**
     * @brief For each depth-1 anchor, mark depth-2 reachability then emit (1,1,2) and (1,2,2) groups.
     *
     * @param ctx Shared run context; bfs_visited is updated in place.
     * @param depth_one Iterator range over root's direct neighbours.
     */
    void emit_depth_1_1_2_and_1_2_2_groups_cpu(CpuKavoshContext& ctx,
                                                const CpuNeighbourRange& depth_one) const;

    /**
     * @brief Outermost driver for the (1,2,3) Kavosh depth variation.
     *
     * @param ctx Shared run context; bfs_visited may be updated.
     * @param depth_one Iterator range over root's direct neighbours.
     */
    void emit_depth_1_2_3_groups_cpu(CpuKavoshContext& ctx,
                                     const CpuNeighbourRange& depth_one) const;

    // ── Template declarations (bodies defined after class) ───────────────────

    /**
     * @brief Emit (1,1,1) groups for a fixed first depth-1 neighbour, iterating remaining pairs.
     *
     * @param ctx Shared run context.
     * @param depth_one Full depth-1 range.
     * @param first_neighbour The chosen first depth-1 vertex.
     * @param is_first_neighbour_reversed True if first_neighbour was reached via a reverse edge.
     */
    template <typename KavoshContext, typename NeighbourRange>
    // NOLINTNEXTLINE(readability-function-size)
    SGF_HD void emit_depth_1_1_1_groups_first_vertex_chosen(
        const KavoshContext& ctx,
        const NeighbourRange& depth_one,
        std::vector<uint32_t>::const_iterator first_neighbour,
        bool is_first_neighbour_reversed) const;

    /**
     * @brief Emit (1,1,1) groups for fixed first and second depth-1 neighbours.
     *
     * @param ctx Shared run context.
     * @param depth_one Full depth-1 range.
     * @param first_neighbour The chosen first depth-1 vertex.
     * @param second_neighbour The chosen second depth-1 vertex.
     * @param is_second_neighbour_reversed True if second_neighbour was reached via a reverse edge.
     */
    template <typename KavoshContext, typename NeighbourRange>
    // NOLINTNEXTLINE(readability-function-size)
    SGF_HD void emit_depth_1_1_1_groups_second_vertex_chosen(
        const KavoshContext& ctx,
        const NeighbourRange& depth_one,
        std::vector<uint32_t>::const_iterator first_neighbour,
        std::vector<uint32_t>::const_iterator second_neighbour,
        bool is_second_neighbour_reversed) const;

    /**
     * @brief Build combined depth-2 range for first_neighbour and run (1,1,2)/(1,2,2) emission.
     *
     * Calls m_graph.get_neighbours() — CPU-only. GPU first-layer drivers bypass this function
     * and call the inner templates directly with CSR pointer ranges.
     *
     * @param ctx Run context (CPU: CpuKavoshContext).
     * @param first_neighbour The depth-1 anchor being processed.
     * @param depth_one Combined depth-1 range used as n11 candidates.
     */
    template <typename Ctx, typename NeighIter, typename NeighRange>
    void process_first_neighbour_112_122(Ctx& ctx,
                                         NeighIter first_neighbour,
                                         const NeighRange& depth_one) const;

    /**
     * @brief Emit groups: root + first_neighbour (depth-1) + n11 (depth-1) + n2 (depth-2).
     *
     * @param ctx Shared run context.
     * @param first_neighbour The depth-1 anchor vertex (n1).
     * @param depth_one All of root's depth-1 neighbours (candidates for n11).
     * @param depth_two All neighbours of first_neighbour (depth-2 candidates for n2).
     */
    template <typename KavoshContext, typename NeighbourRange>
    SGF_HD void emit_depth_1_1_2_for_first_vertex(
        const KavoshContext& ctx,
        std::vector<uint32_t>::const_iterator first_neighbour,
        const NeighbourRange& depth_one,
        const NeighbourRange& depth_two) const;

    /**
     * @brief Emit (1,1,2) groups for a single fixed n2 vertex against all n11 candidates.
     *
     * @param ctx Shared run context.
     * @param first_neighbour The depth-1 anchor (n1).
     * @param depth_one Combined depth-1 range (fwd + rev).
     * @param second_neighbour The fixed depth-2 vertex.
     */
    template <typename KavoshContext, typename NeighbourRange>
    // NOLINTNEXTLINE(readability-function-size)
    SGF_HD void emit_depth_1_1_2_for_second_vertex(
        const KavoshContext& ctx,
        std::vector<uint32_t>::const_iterator first_neighbour,
        const NeighbourRange& depth_one,
        std::vector<uint32_t>::const_iterator second_neighbour) const;

    /**
     * @brief Emit groups: root + first_neighbour (depth-1) + two distinct depth-2 vertices.
     *
     * @param ctx Shared run context.
     * @param first_neighbour The depth-1 anchor vertex (n1).
     * @param depth_two All neighbours of first_neighbour (pool for depth-2 pair selection).
     */
    template <typename KavoshContext, typename NeighbourRange>
    SGF_HD void emit_depth_1_2_2_for_first_vertex(
        const KavoshContext& ctx,
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
    template <typename KavoshContext, typename NeighbourRange>
    // NOLINTNEXTLINE(readability-function-size)
    SGF_HD void emit_depth_1_2_2_for_second_vertex(
        const KavoshContext& ctx,
        std::vector<uint32_t>::const_iterator first_neighbour,
        const NeighbourRange& depth_two,
        std::vector<uint32_t>::const_iterator second_neighbour,
        bool is_second_vertex_reversed) const;

    /**
     * @brief Enumerate BFS-depth-2 neighbours of n1 and delegate per-n2 group emission.
     *
     * Internally calls m_graph.get_neighbours() to fetch the third-degree range — CPU-only.
     *
     * @param ctx Shared run context; bfs_visited may be updated.
     * @param first_degree_vertex The depth-1 anchor (n1).
     * @param second_degree Iterator range over n1's neighbours (depth-2 candidates).
     */
    template <typename KavoshContext, typename NeighbourRange>
    SGF_HD void emit_depth_1_2_3_for_first_vertex(KavoshContext& ctx,
                                                   uint32_t first_degree_vertex,
                                                   const NeighbourRange& second_degree) const;

    /**
     * @brief Emit groups: root + n1 + n2 + n3 for each candidate third-degree vertex.
     *
     * @param ctx Shared run context; bfs_visited may be updated.
     * @param first_degree_vertex The depth-1 anchor (n1).
     * @param second_degree_vertex The depth-2 anchor (n2).
     * @param third_degree Iterator range over n2's neighbours (candidates for n3).
     */
    template <typename KavoshContext, typename NeighbourRange>
    SGF_HD void emit_depth_1_2_3_for_second_vertex(KavoshContext& ctx,
                                                    uint32_t first_degree_vertex,
                                                    uint32_t second_degree_vertex,
                                                    const NeighbourRange& third_degree) const;

    /**
     * @brief Emit one (1,2,3) group for a single n3 candidate, updating bfs_visited as needed.
     *
     * @param ctx Shared run context; bfs_visited may be updated.
     * @param first_degree_vertex The depth-1 anchor.
     * @param second_degree_vertex The depth-2 anchor.
     * @param third_degree_vertex The candidate depth-3 vertex.
     */
    template <typename KavoshContext>
    SGF_HD void emit_depth_1_2_3_for_third_vertex(KavoshContext& ctx,
                                                   uint32_t first_degree_vertex,
                                                   uint32_t second_degree_vertex,
                                                   uint32_t third_degree_vertex) const;

#ifdef SGF_CUDA_ENABLED
    // ── GPU overrides and drivers ────────────────────────────────────────────

    /**
     * @brief GPU override for full motif enumeration via CUDA kernel.
     * @return EnumerationResult aggregated from all GPU threads.
     */
    EnumerationResult calculate_gpu() override;

    /**
     * @brief GPU driver for (1,1,1) groups; strides over depth-1 neighbours by @p stride_y.
     *
     * @param ctx GPU BFS run context.
     * @param thread_y_offset This thread's y-dimension offset within the stride.
     * @param stride_y Total y-dimension stride across all threads.
     */
    __device__ void emit_depth_1_1_1_groups_gpu(GpuKavoshContext& ctx,
                                                 uint32_t thread_y_offset,
                                                 uint32_t stride_y) const;

    /**
     * @brief GPU driver for (1,1,2)/(1,2,2) groups; strides over depth-1 neighbours by @p stride_y.
     *
     * @param ctx GPU BFS run context.
     * @param thread_y_offset This thread's y-dimension offset within the stride.
     * @param stride_y Total y-dimension stride across all threads.
     */
    __device__ void emit_depth_1_1_2_and_1_2_2_groups_gpu(GpuKavoshContext& ctx,
                                                            uint32_t thread_y_offset,
                                                            uint32_t stride_y) const;

    /**
     * @brief GPU driver for (1,2,3) groups; strides over depth-1 neighbours by @p stride_y.
     *
     * @param ctx GPU BFS run context.
     * @param thread_y_offset This thread's y-dimension offset within the stride.
     * @param stride_y Total y-dimension stride across all threads.
     */
    __device__ void emit_depth_1_2_3_groups_gpu(GpuKavoshContext& ctx,
                                                 uint32_t thread_y_offset,
                                                 uint32_t stride_y) const;
#endif
};

// ============================================================================
// Template definitions
// ============================================================================

template <typename KavoshContext, typename NeighbourRange>
// NOLINTNEXTLINE(readability-function-size)
SGF_HD void MotifPreprocessor::emit_depth_1_1_1_groups_first_vertex_chosen(
    const KavoshContext& ctx,
    const NeighbourRange& depth_one,
    std::vector<uint32_t>::const_iterator first_neighbour,
    const bool is_first_neighbour_reversed) const
{
    if (m_order_index[*first_neighbour] < m_order_index[ctx.m_root])
    {
        return;
    }
    if (!is_first_neighbour_reversed)
    {
        for (auto second = first_neighbour + 1; second != depth_one.m_end; ++second)
        {
            if (m_order_index[*second] < m_order_index[ctx.m_root])
            {
                continue;
            }
            emit_depth_1_1_1_groups_second_vertex_chosen(ctx, depth_one, first_neighbour, second,
                                                         false);
        }
    }
    if (m_graph.is_directed())
    {
        auto second = is_first_neighbour_reversed ? first_neighbour + 1 : depth_one.m_rev_begin;
        for (; second != depth_one.m_rev_end; ++second)
        {
            if (m_order_index[*second] < m_order_index[ctx.m_root] ||
                ctx.m_adjacency_matrix[ctx.m_root][*second])
            {
                continue;
            }
            emit_depth_1_1_1_groups_second_vertex_chosen(ctx, depth_one, first_neighbour, second,
                                                         true);
        }
    }
}

template <typename KavoshContext, typename NeighbourRange>
// NOLINTNEXTLINE(readability-function-size)
SGF_HD void MotifPreprocessor::emit_depth_1_1_1_groups_second_vertex_chosen(
    const KavoshContext& ctx,
    const NeighbourRange& depth_one,
    std::vector<uint32_t>::const_iterator first_neighbour,
    std::vector<uint32_t>::const_iterator second_neighbour,
    const bool is_second_neighbour_reversed) const
{
    if (!is_second_neighbour_reversed)
    {
        for (auto third = second_neighbour + 1; third != depth_one.m_end; ++third)
        {
            if (m_order_index[*third] < m_order_index[ctx.m_root])
            {
                continue;
            }
            const std::vector<uint32_t> group = {ctx.m_root, *first_neighbour, *second_neighbour,
                                                 *third};
            ctx.m_count_group(compute_motif_descriptor(group, ctx.m_adjacency_matrix), group);
        }
    }
    if (m_graph.is_directed())
    {
        auto third = is_second_neighbour_reversed ? second_neighbour + 1 : depth_one.m_rev_begin;
        for (; third != depth_one.m_rev_end; ++third)
        {
            if (m_order_index[*third] < m_order_index[ctx.m_root] ||
                ctx.m_adjacency_matrix[ctx.m_root][*third])
            {
                continue;
            }
            const std::vector<uint32_t> group = {ctx.m_root, *first_neighbour, *second_neighbour,
                                                 *third};
            ctx.m_count_group(compute_motif_descriptor(group, ctx.m_adjacency_matrix), group);
        }
    }
}

template <typename Ctx, typename NeighIter, typename NeighRange>
void MotifPreprocessor::process_first_neighbour_112_122(Ctx& ctx,
                                                         const NeighIter first_neighbour,
                                                         const NeighRange& depth_one) const
{
    const NeighbourIteratorPair two_fwd = m_graph.get_neighbours(*first_neighbour);
    const NeighbourIteratorPair two_rev =
        m_graph.is_directed() ? m_graph.get_neighbours(*first_neighbour, true)
                              : std::make_pair(two_fwd.second, two_fwd.second);
    const CpuNeighbourRange depth_two{two_fwd.first, two_fwd.second, two_rev.first, two_rev.second};
    mark_depth_two_neighbours(ctx, depth_two);
    emit_depth_1_1_2_for_first_vertex(ctx, first_neighbour, depth_one, depth_two);
    emit_depth_1_2_2_for_first_vertex(ctx, first_neighbour, depth_two);
}

template <typename KavoshContext, typename NeighbourRange>
SGF_HD void MotifPreprocessor::emit_depth_1_1_2_for_first_vertex(
    const KavoshContext& ctx,
    std::vector<uint32_t>::const_iterator first_neighbour,
    const NeighbourRange& depth_one,
    const NeighbourRange& depth_two) const
{
    for (auto second_degree_neighbour = depth_two.m_begin;
         second_degree_neighbour != depth_two.m_end; ++second_degree_neighbour)
    {
        if (m_order_index[*second_degree_neighbour] < m_order_index[ctx.m_root] ||
            ctx.m_bfs_visited[*second_degree_neighbour] !=
                static_cast<int64_t>(ctx.m_run_id + BFS_DEPTH_TWO_OFFSET))
        {
            continue;
        }
        emit_depth_1_1_2_for_second_vertex(ctx, first_neighbour, depth_one,
                                           second_degree_neighbour);
    }
    if (m_graph.is_directed())
    {
        for (auto second_degree_neighbour = depth_two.m_rev_begin;
             second_degree_neighbour != depth_two.m_rev_end; ++second_degree_neighbour)
        {
            if (m_order_index[*second_degree_neighbour] < m_order_index[ctx.m_root] ||
                ctx.m_bfs_visited[*second_degree_neighbour] !=
                    static_cast<int64_t>(ctx.m_run_id + BFS_DEPTH_TWO_OFFSET) ||
                ctx.m_adjacency_matrix[*first_neighbour][*second_degree_neighbour])
            {
                continue;
            }
            emit_depth_1_1_2_for_second_vertex(ctx, first_neighbour, depth_one,
                                               second_degree_neighbour);
        }
    }
}

template <typename KavoshContext, typename NeighbourRange>
// NOLINTNEXTLINE(readability-function-size)
SGF_HD void MotifPreprocessor::emit_depth_1_1_2_for_second_vertex(
    const KavoshContext& ctx,
    std::vector<uint32_t>::const_iterator first_neighbour,
    const NeighbourRange& depth_one,
    std::vector<uint32_t>::const_iterator second_neighbour) const
{
    for (auto second_first_degree_neighbour = depth_one.m_begin;
         second_first_degree_neighbour != depth_one.m_end; ++second_first_degree_neighbour)
    {
        if (m_order_index[*second_first_degree_neighbour] < m_order_index[ctx.m_root] ||
            *first_neighbour == *second_first_degree_neighbour)
        {
            continue;
        }
        const bool edge_exists =
            ctx.m_adjacency_matrix[*second_first_degree_neighbour][*second_neighbour] ||
            ctx.m_adjacency_matrix[*second_neighbour][*second_first_degree_neighbour];
        // avoid double-counting due to two paths from root to n2 - from n1 and from n11.
        if (!edge_exists || (edge_exists && *first_neighbour < *second_first_degree_neighbour))
        {
            const std::vector<uint32_t> group = {ctx.m_root, *first_neighbour,
                                                 *second_first_degree_neighbour, *second_neighbour};
            ctx.m_count_group(compute_motif_descriptor(group, ctx.m_adjacency_matrix), group);
        }
    }
    if (m_graph.is_directed())
    {
        for (auto second_first_degree_neighbour = depth_one.m_rev_begin;
             second_first_degree_neighbour != depth_one.m_rev_end; ++second_first_degree_neighbour)
        {
            if (m_order_index[*second_first_degree_neighbour] < m_order_index[ctx.m_root] ||
                *first_neighbour == *second_first_degree_neighbour ||
                ctx.m_adjacency_matrix[ctx.m_root][*second_first_degree_neighbour])
            {
                continue;
            }
            const bool edge_exists =
                ctx.m_adjacency_matrix[*second_first_degree_neighbour][*second_neighbour] ||
                ctx.m_adjacency_matrix[*second_neighbour][*second_first_degree_neighbour];
            // avoid double-counting due to two paths from root to n2 - from n1 and from n11.
            if (!edge_exists || (edge_exists && *first_neighbour < *second_first_degree_neighbour))
            {
                const std::vector<uint32_t> group = {ctx.m_root, *first_neighbour,
                                                     *second_first_degree_neighbour,
                                                     *second_neighbour};
                ctx.m_count_group(compute_motif_descriptor(group, ctx.m_adjacency_matrix), group);
            }
        }
    }
}

template <typename KavoshContext, typename NeighbourRange>
SGF_HD void MotifPreprocessor::emit_depth_1_2_2_for_first_vertex(
    const KavoshContext& ctx,
    std::vector<uint32_t>::const_iterator first_neighbour,
    const NeighbourRange& depth_two) const
{
    for (auto first_second_degree_neighbour = depth_two.m_begin;
         first_second_degree_neighbour != depth_two.m_end; ++first_second_degree_neighbour)
    {
        if (m_order_index[*first_second_degree_neighbour] < m_order_index[ctx.m_root] ||
            ctx.m_bfs_visited[*first_second_degree_neighbour] !=
                static_cast<int64_t>(ctx.m_run_id + BFS_DEPTH_TWO_OFFSET))
        {
            continue;
        }
        emit_depth_1_2_2_for_second_vertex(ctx, first_neighbour, depth_two,
                                           first_second_degree_neighbour, false);
    }
    if (m_graph.is_directed())
    {
        for (auto first_second_degree_neighbour = depth_two.m_rev_begin;
             first_second_degree_neighbour != depth_two.m_rev_end; ++first_second_degree_neighbour)
        {
            if (m_order_index[*first_second_degree_neighbour] < m_order_index[ctx.m_root] ||
                ctx.m_bfs_visited[*first_second_degree_neighbour] !=
                    static_cast<int64_t>(ctx.m_run_id + BFS_DEPTH_TWO_OFFSET) ||
                ctx.m_adjacency_matrix[*first_neighbour][*first_second_degree_neighbour])
            {
                continue;
            }
            emit_depth_1_2_2_for_second_vertex(ctx, first_neighbour, depth_two,
                                               first_second_degree_neighbour, true);
        }
    }
}

template <typename KavoshContext, typename NeighbourRange>
// NOLINTNEXTLINE(readability-function-size)
SGF_HD void MotifPreprocessor::emit_depth_1_2_2_for_second_vertex(
    const KavoshContext& ctx,
    std::vector<uint32_t>::const_iterator first_neighbour,
    const NeighbourRange& depth_two,
    std::vector<uint32_t>::const_iterator second_neighbour,
    const bool is_second_vertex_reversed) const
{
    if (!is_second_vertex_reversed)
    {
        for (auto second_second_degree_neighbour = second_neighbour + 1;
             second_second_degree_neighbour != depth_two.m_end; ++second_second_degree_neighbour)
        {
            if (m_order_index[*second_second_degree_neighbour] < m_order_index[ctx.m_root] ||
                ctx.m_bfs_visited[*second_second_degree_neighbour] !=
                    static_cast<int64_t>(ctx.m_run_id + BFS_DEPTH_TWO_OFFSET))
            {
                continue;
            }
            const std::vector<uint32_t> group = {ctx.m_root, *first_neighbour, *second_neighbour,
                                                 *second_second_degree_neighbour};
            ctx.m_count_group(compute_motif_descriptor(group, ctx.m_adjacency_matrix), group);
        }
    }
    if (m_graph.is_directed())
    {
        auto second_second_degree_neighbour =
            is_second_vertex_reversed ? second_neighbour + 1 : depth_two.m_rev_begin;
        for (; second_second_degree_neighbour != depth_two.m_rev_end;
             ++second_second_degree_neighbour)
        {
            if (m_order_index[*second_second_degree_neighbour] < m_order_index[ctx.m_root] ||
                ctx.m_bfs_visited[*second_second_degree_neighbour] !=
                    static_cast<int64_t>(ctx.m_run_id + BFS_DEPTH_TWO_OFFSET) ||
                ctx.m_adjacency_matrix[*first_neighbour][*second_second_degree_neighbour])
            {
                continue;
            }
            const std::vector<uint32_t> group = {ctx.m_root, *first_neighbour, *second_neighbour,
                                                 *second_second_degree_neighbour};
            ctx.m_count_group(compute_motif_descriptor(group, ctx.m_adjacency_matrix), group);
        }
    }
}

template <typename KavoshContext, typename NeighbourRange>
SGF_HD void MotifPreprocessor::emit_depth_1_2_3_for_first_vertex(
    KavoshContext& ctx,
    const uint32_t first_degree_vertex,
    const NeighbourRange& second_degree) const
{
    for (auto second_vertex = second_degree.m_begin; second_vertex != second_degree.m_end;
         ++second_vertex)
    {
        if (m_order_index[*second_vertex] < m_order_index[ctx.m_root] ||
            ctx.m_bfs_visited[*second_vertex] !=
                static_cast<int64_t>(ctx.m_run_id + BFS_DEPTH_TWO_OFFSET))
        {
            continue;
        }
        const NeighbourIteratorPair three_fwd = m_graph.get_neighbours(*second_vertex);
        const NeighbourIteratorPair three_rev =
            m_graph.is_directed() ? m_graph.get_neighbours(*second_vertex, true)
                                  : std::make_pair(three_fwd.second, three_fwd.second);
        emit_depth_1_2_3_for_second_vertex(
            ctx, first_degree_vertex, *second_vertex,
            NeighbourRange{three_fwd.first, three_fwd.second, three_rev.first, three_rev.second});
    }
    if (m_graph.is_directed())
    {
        for (auto second_vertex = second_degree.m_rev_begin;
             second_vertex != second_degree.m_rev_end; ++second_vertex)
        {
            if (m_order_index[*second_vertex] < m_order_index[ctx.m_root] ||
                ctx.m_bfs_visited[*second_vertex] !=
                    static_cast<int64_t>(ctx.m_run_id + BFS_DEPTH_TWO_OFFSET) ||
                ctx.m_adjacency_matrix[first_degree_vertex][*second_vertex])
            {
                continue;
            }
            const NeighbourIteratorPair three_fwd = m_graph.get_neighbours(*second_vertex);
            const NeighbourIteratorPair three_rev =
                m_graph.is_directed() ? m_graph.get_neighbours(*second_vertex, true)
                                      : std::make_pair(three_fwd.second, three_fwd.second);
            emit_depth_1_2_3_for_second_vertex(
                ctx, first_degree_vertex, *second_vertex,
                NeighbourRange{three_fwd.first, three_fwd.second, three_rev.first,
                               three_rev.second});
        }
    }
}

template <typename KavoshContext, typename NeighbourRange>
SGF_HD void MotifPreprocessor::emit_depth_1_2_3_for_second_vertex(
    KavoshContext& ctx,
    const uint32_t first_degree_vertex,
    const uint32_t second_degree_vertex,
    const NeighbourRange& third_degree) const
{
    for (auto third_vertex = third_degree.m_begin; third_vertex != third_degree.m_end;
         ++third_vertex)
    {
        if (m_order_index[*third_vertex] < m_order_index[ctx.m_root])
        {
            continue;
        }
        emit_depth_1_2_3_for_third_vertex(ctx, first_degree_vertex, second_degree_vertex,
                                          *third_vertex);
    }
    if (m_graph.is_directed())
    {
        for (auto third_vertex = third_degree.m_rev_begin;
             third_vertex != third_degree.m_rev_end; ++third_vertex)
        {
            if (m_order_index[*third_vertex] < m_order_index[ctx.m_root] ||
                ctx.m_adjacency_matrix[second_degree_vertex][*third_vertex])
            {
                continue;
            }
            emit_depth_1_2_3_for_third_vertex(ctx, first_degree_vertex, second_degree_vertex,
                                              *third_vertex);
        }
    }
}

template <typename KavoshContext>
SGF_HD void MotifPreprocessor::emit_depth_1_2_3_for_third_vertex(
    KavoshContext& ctx,
    const uint32_t first_degree_vertex,
    const uint32_t second_degree_vertex,
    const uint32_t third_degree_vertex) const
{
    const std::vector<uint32_t> group = {ctx.m_root, first_degree_vertex, second_degree_vertex,
                                         third_degree_vertex};
    const bool is_new = (static_cast<uint64_t>(ctx.m_bfs_visited[third_degree_vertex]) >>
                         BFS_VERTEX_RUN_SHIFT) != static_cast<uint64_t>(ctx.m_root);
    // Depth-2 vertex reachable via n2 with no direct edge to n1: genuine (1,2,3) path.
    // Depth-2 vertex with back-edge to n1: already counted by emit_depth_1_1_2, skip.
    const bool is_depth_two_no_back_edge =
        (ctx.m_bfs_visited[third_degree_vertex] ==
         static_cast<int64_t>(ctx.m_run_id + BFS_DEPTH_TWO_OFFSET)) &&
        !ctx.m_adjacency_matrix[first_degree_vertex][third_degree_vertex] &&
        !ctx.m_adjacency_matrix[third_degree_vertex][first_degree_vertex];
    const bool is_depth_three =
        ctx.m_bfs_visited[third_degree_vertex] ==
        static_cast<int64_t>(ctx.m_run_id + BFS_DEPTH_THREE_OFFSET);
    if (is_new)
    {
        ctx.m_bfs_visited[third_degree_vertex] =
            ctx.m_run_id + static_cast<int64_t>(BFS_DEPTH_THREE_OFFSET);
    }
    if (is_new || is_depth_two_no_back_edge || is_depth_three)
    {
        ctx.m_count_group(compute_motif_descriptor(group, ctx.m_adjacency_matrix), group);
    }
}

}  // namespace sgf
