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
#include "MotifMap.h"

#include <algorithm>
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
 * Edge existence is checked via binary search on sorted neighbour lists —
 * no adjacency matrix is built. This keeps the same O(log d) cost per lookup
 * on both CPU (ColoredGraph iterators) and GPU (CSR arrays).
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

    // ── SGF_HD shared computation ─────────────────────────────────────────────

    /**
     * @brief Compute the edge-structure descriptor for a 4-node group.
     *
     * Works on both CPU and GPU. Encodes the induced subgraph edge pattern as a
     * bitmask using the same read order as the canonical maps: upper triangle for
     * undirected, all pairs for directed.
     *
     * @tparam EdgeChecker Callable with signature `bool(uint32_t src, uint32_t dest)`.
     * @param group         4 vertex IDs: {root, n1, n2, n3}.
     * @param is_directed   Whether the graph is directed.
     * @param has_edge      Edge-existence predicate (binary search on CPU/GPU).
     * @return Integer bitmask encoding edge presence (MSB = first pair read).
     */
    template <typename EdgeChecker>
    SGF_HD static uint32_t compute_motif_descriptor(const uint32_t* group,
                                                     bool is_directed,
                                                     EdgeChecker has_edge) noexcept;

    /**
     * @brief Canonicalize a motif descriptor into a 128-bit identifier.
     *
     * Works on both CPU and GPU. Looks up the canonical entry in a flat
     * MotifCanonical array (indexed by descriptor), applies all color permutations,
     * and returns the minimal encoded form.
     *
     * @param descriptor      Raw edge-structure descriptor from compute_motif_descriptor.
     * @param node_colors     4 color values: {color(root), color(n1), color(n2), color(n3)}.
     * @param canonical_array Flat array of MotifCanonical entries, indexed by descriptor.
     * @param canonical_size  Number of entries in canonical_array.
     * @return Canonical 128-bit motif identifier.
     */
    SGF_HD static UInt128 calculate_motif_number_from_arrays(
        uint32_t descriptor,
        const uint32_t* node_colors,
        const MotifCanonical* canonical_array,
        uint32_t canonical_size) noexcept;

protected:
    /**
     * @brief Enumerate all 4-node induced subgraphs using Kavosh BFS.
     *
     * Uses binary search on sorted neighbour lists for edge existence checks —
     * no adjacency matrix is built. Thread-parallel over root vertices.
     *
     * @return Map from canonical motif identifier to occurrence count.
     */
    EnumerationResult stream_groups_to_counter() const override;

    /**
     * @brief Canonicalize a 4-node group into a unique motif identifier.
     *
     * Wraps calculate_motif_number_from_arrays using the precomputed flat
     * canonical array built once at the start of stream_groups_to_counter.
     *
     * @param motif_descriptor Raw edge-structure number for the group.
     * @param node_colors Color labels of the four vertices in group order.
     * @return Canonical 128-bit motif identifier.
     */
    UInt128 calculate_motif_number(uint32_t motif_descriptor,
                                   const std::vector<uint32_t>& node_colors) const override;

private:
    // ── CpuNeighbourRange ─────────────────────────────────────────────────────

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

    // ── CpuKavoshContext ──────────────────────────────────────────────────────

    /**
     * @brief Shared mutable state for one Kavosh BFS run rooted at a single vertex.
     *
     * Edge checks use binary search on the sorted ColoredGraph neighbour lists —
     * identical O(log d) complexity to the GPU CSR path.
     */
    struct CpuKavoshContext
    {
        const ColoredGraph& m_graph;          ///< Graph for neighbour/edge lookups.
        const GroupCounterCallback& m_count_group;  ///< Callback for emitting groups.
        std::vector<int64_t>& m_bfs_visited;  ///< BFS depth-encoding array.
        int64_t  m_run_id;                    ///< Root-unique run identifier.
        uint32_t m_root;                      ///< Current root vertex.
        const MotifCanonical* m_canonical;    ///< Flat canonical array (owned by caller).
        uint32_t m_canonical_size;            ///< Number of entries in m_canonical.
        const std::vector<uint32_t>& m_order_index;        ///< Vertex position in degree-sorted order (m_order_index.data()).

        /**
         * @brief Return true if @p dest is a forward neighbour of @p src.
         *
         * Binary search on the sorted neighbour list returned by get_neighbours.
         */
        bool has_fwd_edge(const uint32_t src, const uint32_t dest) const
        {
            const auto [begin, end] = m_graph.get_neighbours(src);
            return std::binary_search(begin, end, dest);
        }

        /**
         * @brief Return true if @p vertex was marked at @p depth in this run.
         */
        bool is_at_depth(const uint32_t vertex, const int64_t depth) const
        {
            return m_bfs_visited[vertex] == m_run_id + depth;
        }

        /**
         * @brief Return true if @p vertex was visited at any depth in this run.
         */
        bool is_visited_in_run(const uint32_t vertex) const
        {
            return (static_cast<uint64_t>(m_bfs_visited[vertex]) >> BFS_VERTEX_RUN_SHIFT) ==
                   static_cast<uint64_t>(m_root);
        }

        /**
         * @brief Mark @p vertex at @p depth for this run.
         */
        void mark_at_depth(const uint32_t vertex, const int64_t depth)
        {
            m_bfs_visited[vertex] = m_run_id + depth;
        }

        void mark_neighbours(const CpuNeighbourRange& neighbours_range, const uint32_t depth) {
            for (auto it = neighbours_range.m_begin; it != neighbours_range.m_end; ++it) {
                if (!is_visited_in_run(*it)) {
                    mark_at_depth(*it, depth);
                }
            }
            if (m_graph.is_directed()) {
                for (auto it = neighbours_range.m_rev_begin; it != neighbours_range.m_rev_end; ++it) {
                    if (!is_visited_in_run(*it)) {
                        mark_at_depth(*it, depth);
                    }
                }
            }
        }
        /**
         * @brief Compute descriptor and emit a (root, n1, n2, n3) group.
         *
         * Computes the motif descriptor via binary-search edge checks, then calls
         * m_count_group which internally calls calculate_motif_number_from_arrays
         * via the stored canonical array.
         */
        void count_group_by_ids(const uint32_t n1, const uint32_t n2, const uint32_t n3) const
        {
            const uint32_t group[4] = {m_root, n1, n2, n3};
            const uint32_t desc = compute_motif_descriptor(
                group, m_graph.is_directed(),
                [this](const uint32_t src, const uint32_t dest)
                { return has_fwd_edge(src, dest); });
            const std::vector<uint32_t> group_vec(group, group + 4U);
            m_count_group(desc, group_vec);
        }

        // ── Constant shift (replicated from MotifPreprocessor outer class) ──
        static constexpr uint64_t BFS_VERTEX_RUN_SHIFT = 2U;
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
    static constexpr uint64_t BFS_DEPTH_ONE_OFFSET   = 1U;
    static constexpr uint64_t BFS_DEPTH_TWO_OFFSET   = 2U;
    static constexpr uint64_t BFS_DEPTH_THREE_OFFSET = 3U;
    static constexpr uint64_t BFS_VERTEX_RUN_SHIFT   = 2U;

    /**
     * @brief Returns "motifs" to label the finished-enumeration log line.
     */
    [[nodiscard]] std::string entity_name() const override;

    /**
     * @brief Build a flat MotifCanonical array from the given canonical map.
     *
     * The array is indexed by motif descriptor (max descriptor + 1 entries).
     * Only descriptors present in the map are populated; all others remain
     * default-constructed (m_permutation_count == 0).
     *
     * @param canonical_map Source map (directed or undirected).
     * @param out_array     Output flat array (resized by this function).
     * @return Number of entries in out_array.
     */
    static uint32_t build_canonical_array(
        const std::unordered_map<uint32_t, MotifCanonical>& canonical_map,
        std::vector<MotifCanonical>& out_array);

    /**
     * @brief Enumerate one root vertex's 4-node groups across all Kavosh BFS depth variations.
     */
    void stream_groups_to_counter_for_vertex(
        const GroupCounterCallback& count_group,
        std::vector<int64_t>& bfs_visited_vertices,
        const MotifCanonical* canonical,
        uint32_t canonical_size,
        uint32_t root) const;

    // ── CPU-only outer driver declarations ────────────────────────────────────

    /**
     * @brief Emit all groups formed by root and three distinct depth-1 neighbours.
     */
    void emit_depth_1_1_1_groups_cpu(const CpuKavoshContext& ctx,
                                     const CpuNeighbourRange& depth_one) const;

    /**
     * @brief For each depth-1 anchor, mark depth-2 then emit (1,1,2) and (1,2,2) groups.
     */
    void emit_depth_1_1_2_and_1_2_2_groups_cpu(CpuKavoshContext& ctx,
                                                const CpuNeighbourRange& depth_one) const;

    /**
     * @brief Outermost driver for the (1,2,3) Kavosh depth variation.
     */
    void emit_depth_1_2_3_groups_cpu(CpuKavoshContext& ctx,
                                     const CpuNeighbourRange& depth_one) const;

    /**
     * @brief Build combined depth-2 range for first_neighbour and run (1,1,2)/(1,2,2) emission.
     *
     * Calls m_graph.get_neighbours() — CPU path only.
     */
    template <typename Ctx, typename NeighIter, typename NeighRange>
    void process_first_neighbour_112_122(Ctx& ctx,
                                         NeighIter first_neighbour,
                                         const NeighRange& depth_one) const;

    // ── Template declarations (SGF_HD bodies defined after class) ───────────────

    /**
     * @brief Emit (1,1,1) groups for a fixed first depth-1 neighbour, iterating remaining pairs.
     *
     * @tparam NeighIter                Random-access iterator or pointer over neighbour IDs.
     *                                  CPU: std::vector<uint32_t>::const_iterator.
     *                                  GPU: const uint32_t* into a CSR array.
     * @param ctx                       Shared run context.
     * @param depth_one                 Full depth-1 neighbour range.
     * @param first_neighbour           Iterator/pointer to the chosen first depth-1 vertex.
     * @param is_first_neighbour_reversed True if first_neighbour was reached via a reverse edge.
     */
    template <typename KavoshContext, typename NeighbourRange, typename NeighIter>
    // NOLINTNEXTLINE(readability-function-size)
    static SGF_HD void emit_depth_1_1_1_groups_first_vertex_chosen(
        const KavoshContext& ctx,
        const NeighbourRange& depth_one,
        NeighIter first_neighbour,
        bool is_first_neighbour_reversed);

    /**
     * @brief Emit (1,1,1) groups for fixed first and second depth-1 neighbours.
     *
     * @tparam NeighIter                Random-access iterator or pointer over neighbour IDs.
     * @param ctx                        Shared run context.
     * @param depth_one                  Full depth-1 neighbour range.
     * @param first_neighbour            Iterator/pointer to the chosen first depth-1 vertex.
     * @param second_neighbour           Iterator/pointer to the chosen second depth-1 vertex.
     * @param is_second_neighbour_reversed True if second_neighbour was reached via a reverse edge.
     */
    template <typename KavoshContext, typename NeighbourRange, typename NeighIter>
    // NOLINTNEXTLINE(readability-function-size)
    static SGF_HD void emit_depth_1_1_1_groups_second_vertex_chosen(
        const KavoshContext& ctx,
        const NeighbourRange& depth_one,
        NeighIter first_neighbour,
        NeighIter second_neighbour,
        bool is_second_neighbour_reversed);

    /**
     * @brief Emit groups: root + first_neighbour (depth-1) + n11 (depth-1) + n2 (depth-2).
     *
     * @tparam NeighIter       Random-access iterator or pointer over neighbour IDs.
     * @param ctx              Shared run context.
     * @param first_neighbour  Iterator/pointer to the depth-1 anchor vertex (n1).
     * @param depth_one        All of root's depth-1 neighbours (candidates for n11).
     * @param depth_two        All neighbours of first_neighbour (depth-2 candidates for n2).
     */
    template <typename KavoshContext, typename NeighbourRange, typename NeighIter>
    static SGF_HD void emit_depth_1_1_2_for_first_vertex(
        const KavoshContext& ctx,
        NeighIter first_neighbour,
        const NeighbourRange& depth_one,
        const NeighbourRange& depth_two);

    /**
     * @brief Emit (1,1,2) groups for a single fixed n2 vertex against all n11 candidates.
     *
     * @tparam NeighIter       Random-access iterator or pointer over neighbour IDs.
     * @param ctx              Shared run context.
     * @param first_neighbour  Iterator/pointer to the depth-1 anchor (n1).
     * @param depth_one        Combined depth-1 range (fwd + rev).
     * @param second_neighbour Iterator/pointer to the fixed depth-2 vertex (n2).
     */
    template <typename KavoshContext, typename NeighbourRange, typename NeighIter>
    // NOLINTNEXTLINE(readability-function-size)
    static SGF_HD void emit_depth_1_1_2_for_second_vertex(
        const KavoshContext& ctx,
        NeighIter first_neighbour,
        const NeighbourRange& depth_one,
        NeighIter second_neighbour);

    /**
     * @brief Emit groups: root + first_neighbour (depth-1) + two distinct depth-2 vertices.
     *
     * @tparam NeighIter       Random-access iterator or pointer over neighbour IDs.
     * @param ctx              Shared run context.
     * @param first_neighbour  Iterator/pointer to the depth-1 anchor vertex (n1).
     * @param depth_two        All neighbours of first_neighbour (pool for depth-2 pair selection).
     */
    template <typename KavoshContext, typename NeighbourRange, typename NeighIter>
    static SGF_HD void emit_depth_1_2_2_for_first_vertex(
        const KavoshContext& ctx,
        NeighIter first_neighbour,
        const NeighbourRange& depth_two);

    /**
     * @brief Emit (1,2,2) groups for a fixed first depth-2 vertex against remaining candidates.
     *
     * @tparam NeighIter                Random-access iterator or pointer over neighbour IDs.
     * @param ctx                      Shared run context.
     * @param first_neighbour          Iterator/pointer to the depth-1 anchor.
     * @param depth_two                Combined depth-2 range (fwd + rev).
     * @param second_neighbour         Iterator/pointer to the chosen first depth-2 vertex.
     * @param is_second_vertex_reversed True if second_neighbour was reached via a reverse edge.
     */
    template <typename KavoshContext, typename NeighbourRange, typename NeighIter>
    // NOLINTNEXTLINE(readability-function-size)
    static SGF_HD void emit_depth_1_2_2_for_second_vertex(
        const KavoshContext& ctx,
        NeighIter first_neighbour,
        const NeighbourRange& depth_two,
        NeighIter second_neighbour,
        bool is_second_vertex_reversed);

    /**
     * @brief Enumerate BFS-depth-2 neighbours of n1 and delegate per-n2 group emission.
     *
     * Calls m_graph.get_neighbours() internally — CPU path only.
     *
     * @param ctx                 Shared run context; bfs_visited may be updated.
     * @param first_degree_vertex The depth-1 anchor (n1).
     * @param second_degree       Iterator range over n1's neighbours (depth-2 candidates).
     */
    template <typename KavoshContext, typename NeighbourRange>
    static void emit_depth_1_2_3_for_first_vertex(KavoshContext& ctx,
                                            uint32_t first_degree_vertex,
                                            const NeighbourRange& second_degree);

    /**
     * @brief Emit groups: root + n1 + n2 + n3 for each candidate third-degree vertex.
     *
     * @param ctx                  Shared run context; bfs_visited may be updated.
     * @param first_degree_vertex  The depth-1 anchor (n1).
     * @param second_degree_vertex The depth-2 anchor (n2).
     * @param third_degree         Iterator range over n2's neighbours (candidates for n3).
     */
    template <typename KavoshContext, typename NeighbourRange>
    static SGF_HD void emit_depth_1_2_3_for_second_vertex(KavoshContext& ctx,
                                                           uint32_t first_degree_vertex,
                                                           uint32_t second_degree_vertex,
                                                           const NeighbourRange& third_degree);

    /**
     * @brief Emit one (1,2,3) group for a single n3 candidate, updating bfs_visited as needed.
     *
     * @param ctx                  Shared run context; bfs_visited may be updated.
     * @param first_degree_vertex  The depth-1 anchor (n1).
     * @param second_degree_vertex The depth-2 anchor (n2).
     * @param third_degree_vertex  The candidate depth-3 vertex (n3).
     */
    template <typename KavoshContext>
    static SGF_HD void emit_depth_1_2_3_for_third_vertex(KavoshContext& ctx,
                                                          uint32_t first_degree_vertex,
                                                          uint32_t second_degree_vertex,
                                                          uint32_t third_degree_vertex);

#ifdef SGF_CUDA_ENABLED
    // ── GPU overrides and drivers ─────────────────────────────────────────────

    /**
     * @brief GPU override: full motif enumeration via CUDA kernel.
     */
    EnumerationResult calculate_gpu() override;

    /**
     * @brief GPU driver for (1,1,1) groups; strides depth-1 neighbours by @p stride_y.
     */
    static __device__ void emit_depth_1_1_1_groups_gpu(GpuKavoshContext& ctx,
                                                 uint32_t thread_y_offset,
                                                 uint32_t stride_y);

    /**
     * @brief GPU driver for (1,1,2)/(1,2,2) groups; strides by @p stride_y.
     */
    static __device__ void emit_depth_1_1_2_and_1_2_2_groups_gpu(GpuKavoshContext& ctx,
                                                            uint32_t thread_y_offset,
                                                            uint32_t stride_y);

    /**
     * @brief GPU driver for (1,2,3) groups; strides by @p stride_y.
     */
    static __device__ void emit_depth_1_2_3_groups_gpu(GpuKavoshContext& ctx,
                                                 uint32_t thread_y_offset,
                                                 uint32_t stride_y);
#endif
};

// ============================================================================
// SGF_HD static compute_motif_descriptor — template definition
// ============================================================================

template <typename EdgeChecker>
SGF_HD uint32_t MotifPreprocessor::compute_motif_descriptor(const uint32_t* const group,
                                                              const bool is_directed,
                                                              EdgeChecker has_edge) noexcept
{
    constexpr uint32_t MOTIF_SIZE = 4U;
    uint32_t descriptor = 0U;
    for (uint32_t row = 0U; row < MOTIF_SIZE; ++row)
    {
        const uint32_t col_start = is_directed ? 0U : row + 1U;
        for (uint32_t col = col_start; col < MOTIF_SIZE; ++col)
        {
            if (row == col)
            {
                continue;
            }
            descriptor <<= 1U;
            descriptor += has_edge(group[row], group[col]) ? 1U : 0U;
        }
    }
    return descriptor;
}

// ============================================================================
// Template definitions — CPU Kavosh BFS helpers
// ============================================================================

template <typename KavoshContext, typename NeighbourRange, typename NeighIter>
// NOLINTNEXTLINE(readability-function-size)
SGF_HD void MotifPreprocessor::emit_depth_1_1_1_groups_first_vertex_chosen(
    const KavoshContext& ctx,
    const NeighbourRange& depth_one,
    const NeighIter first_neighbour,
    const bool is_first_neighbour_reversed)
{
    if (ctx.m_order_index[*first_neighbour] < ctx.m_order_index[ctx.m_root])
    {
        return;
    }
    if (!is_first_neighbour_reversed)
    {
        for (auto second = first_neighbour + 1; second != depth_one.m_end; ++second)
        {
            if (ctx.m_order_index[*second] < ctx.m_order_index[ctx.m_root])
            {
                continue;
            }
            emit_depth_1_1_1_groups_second_vertex_chosen(ctx, depth_one, first_neighbour, second,
                                                         false);
        }
    }
    if (ctx.m_graph.is_directed())
    {
        auto second = is_first_neighbour_reversed ? first_neighbour + 1 : depth_one.m_rev_begin;
        for (; second != depth_one.m_rev_end; ++second)
        {
            if (ctx.m_order_index[*second] < ctx.m_order_index[ctx.m_root] ||
                ctx.has_fwd_edge(ctx.m_root, *second))
            {
                continue;
            }
            emit_depth_1_1_1_groups_second_vertex_chosen(ctx, depth_one, first_neighbour, second,
                                                         true);
        }
    }
}

template <typename KavoshContext, typename NeighbourRange, typename NeighIter>
// NOLINTNEXTLINE(readability-function-size)
SGF_HD void MotifPreprocessor::emit_depth_1_1_1_groups_second_vertex_chosen(
    const KavoshContext& ctx,
    const NeighbourRange& depth_one,
    const NeighIter first_neighbour,
    const NeighIter second_neighbour,
    const bool is_second_neighbour_reversed)
{
    if (!is_second_neighbour_reversed)
    {
        for (auto third = second_neighbour + 1; third != depth_one.m_end; ++third)
        {
            if (ctx.m_order_index[*third] < ctx.m_order_index[ctx.m_root])
            {
                continue;
            }
            ctx.count_group_by_ids(*first_neighbour, *second_neighbour, *third);
        }
    }
    if (ctx.m_graph.is_directed())
    {
        auto third = is_second_neighbour_reversed ? second_neighbour + 1 : depth_one.m_rev_begin;
        for (; third != depth_one.m_rev_end; ++third)
        {
            if (ctx.m_order_index[*third] < ctx.m_order_index[ctx.m_root] ||
                ctx.has_fwd_edge(ctx.m_root, *third))
            {
                continue;
            }
            ctx.count_group_by_ids(*first_neighbour, *second_neighbour, *third);
        }
    }
}

template <typename Ctx, typename NeighIter, typename NeighRange>
void MotifPreprocessor::process_first_neighbour_112_122(Ctx& ctx,
                                                         const NeighIter first_neighbour,
                                                         const NeighRange& depth_one) const
{
    const NeighbourIteratorPair two_fwd = ctx.m_graph.get_neighbours(*first_neighbour);
    const NeighbourIteratorPair two_rev =
        ctx.m_graph.is_directed() ? ctx.m_graph.get_neighbours(*first_neighbour, true)
                              : std::make_pair(two_fwd.second, two_fwd.second);
    const CpuNeighbourRange depth_two{two_fwd.first, two_fwd.second, two_rev.first, two_rev.second};
    ctx.mark_neighbours(depth_two, BFS_DEPTH_TWO_OFFSET);
    emit_depth_1_1_2_for_first_vertex(ctx, first_neighbour, depth_one, depth_two);
    emit_depth_1_2_2_for_first_vertex(ctx, first_neighbour, depth_two);
}

template <typename KavoshContext, typename NeighbourRange, typename NeighIter>
SGF_HD void MotifPreprocessor::emit_depth_1_1_2_for_first_vertex(
    const KavoshContext& ctx,
    const NeighIter first_neighbour,
    const NeighbourRange& depth_one,
    const NeighbourRange& depth_two)
{
    for (auto second_degree_neighbour = depth_two.m_begin;
         second_degree_neighbour != depth_two.m_end; ++second_degree_neighbour)
    {
        if (ctx.m_order_index[*second_degree_neighbour] < ctx.m_order_index[ctx.m_root] ||
            !ctx.is_at_depth(*second_degree_neighbour, static_cast<int64_t>(BFS_DEPTH_TWO_OFFSET)))
        {
            continue;
        }
        emit_depth_1_1_2_for_second_vertex(ctx, first_neighbour, depth_one,
                                           second_degree_neighbour);
    }
    if (ctx.m_graph.is_directed())
    {
        for (auto second_degree_neighbour = depth_two.m_rev_begin;
             second_degree_neighbour != depth_two.m_rev_end; ++second_degree_neighbour)
        {
            if (ctx.m_order_index[*second_degree_neighbour] < ctx.m_order_index[ctx.m_root] ||
                !ctx.is_at_depth(*second_degree_neighbour,
                                 static_cast<int64_t>(BFS_DEPTH_TWO_OFFSET)) ||
                ctx.has_fwd_edge(*first_neighbour, *second_degree_neighbour))
            {
                continue;
            }
            emit_depth_1_1_2_for_second_vertex(ctx, first_neighbour, depth_one,
                                               second_degree_neighbour);
        }
    }
}

template <typename KavoshContext, typename NeighbourRange, typename NeighIter>
// NOLINTNEXTLINE(readability-function-size)
SGF_HD void MotifPreprocessor::emit_depth_1_1_2_for_second_vertex(
    const KavoshContext& ctx,
    const NeighIter first_neighbour,
    const NeighbourRange& depth_one,
    const NeighIter second_neighbour)
{
    for (auto n11 = depth_one.m_begin; n11 != depth_one.m_end; ++n11)
    {
        if (ctx.m_order_index[*n11] < ctx.m_order_index[ctx.m_root] || *first_neighbour == *n11)
        {
            continue;
        }
        const bool edge_exists = ctx.has_fwd_edge(*n11, *second_neighbour) ||
                                 ctx.has_fwd_edge(*second_neighbour, *n11);
        // avoid double-counting due to two paths from root to n2 - from n1 and from n11.
        if (!edge_exists || (edge_exists && *first_neighbour < *n11))
        {
            ctx.count_group_by_ids(*first_neighbour, *n11, *second_neighbour);
        }
    }
    if (ctx.m_graph.is_directed())
    {
        for (auto n11 = depth_one.m_rev_begin; n11 != depth_one.m_rev_end; ++n11)
        {
            if (ctx.m_order_index[*n11] < ctx.m_order_index[ctx.m_root] ||
                *first_neighbour == *n11 ||
                ctx.has_fwd_edge(ctx.m_root, *n11))
            {
                continue;
            }
            const bool edge_exists = ctx.has_fwd_edge(*n11, *second_neighbour) ||
                                     ctx.has_fwd_edge(*second_neighbour, *n11);
            // avoid double-counting due to two paths from root to n2 - from n1 and from n11.
            if (!edge_exists || (edge_exists && *first_neighbour < *n11))
            {
                ctx.count_group_by_ids(*first_neighbour, *n11, *second_neighbour);
            }
        }
    }
}

template <typename KavoshContext, typename NeighbourRange, typename NeighIter>
SGF_HD void MotifPreprocessor::emit_depth_1_2_2_for_first_vertex(
    const KavoshContext& ctx,
    const NeighIter first_neighbour,
    const NeighbourRange& depth_two)
{
    for (auto first_n2 = depth_two.m_begin; first_n2 != depth_two.m_end; ++first_n2)
    {
        if (ctx.m_order_index[*first_n2] < ctx.m_order_index[ctx.m_root] ||
            !ctx.is_at_depth(*first_n2, static_cast<int64_t>(BFS_DEPTH_TWO_OFFSET)))
        {
            continue;
        }
        emit_depth_1_2_2_for_second_vertex(ctx, first_neighbour, depth_two, first_n2, false);
    }
    if (ctx.m_graph.is_directed())
    {
        for (auto first_n2 = depth_two.m_rev_begin; first_n2 != depth_two.m_rev_end; ++first_n2)
        {
            if (ctx.m_order_index[*first_n2] < ctx.m_order_index[ctx.m_root] ||
                !ctx.is_at_depth(*first_n2, static_cast<int64_t>(BFS_DEPTH_TWO_OFFSET)) ||
                ctx.has_fwd_edge(*first_neighbour, *first_n2))
            {
                continue;
            }
            emit_depth_1_2_2_for_second_vertex(ctx, first_neighbour, depth_two, first_n2, true);
        }
    }
}

template <typename KavoshContext, typename NeighbourRange, typename NeighIter>
// NOLINTNEXTLINE(readability-function-size)
SGF_HD void MotifPreprocessor::emit_depth_1_2_2_for_second_vertex(
    const KavoshContext& ctx,
    const NeighIter first_neighbour,
    const NeighbourRange& depth_two,
    const NeighIter second_neighbour,
    const bool is_second_vertex_reversed)
{
    if (!is_second_vertex_reversed)
    {
        for (auto second_n2 = second_neighbour + 1; second_n2 != depth_two.m_end; ++second_n2)
        {
            if (ctx.m_order_index[*second_n2] < ctx.m_order_index[ctx.m_root] ||
                !ctx.is_at_depth(*second_n2, static_cast<int64_t>(BFS_DEPTH_TWO_OFFSET)))
            {
                continue;
            }
            ctx.count_group_by_ids(*first_neighbour, *second_neighbour, *second_n2);
        }
    }
    if (ctx.m_graph.is_directed())
    {
        auto second_n2 = is_second_vertex_reversed ? second_neighbour + 1 : depth_two.m_rev_begin;
        for (; second_n2 != depth_two.m_rev_end; ++second_n2)
        {
            if (ctx.m_order_index[*second_n2] < ctx.m_order_index[ctx.m_root] ||
                !ctx.is_at_depth(*second_n2, static_cast<int64_t>(BFS_DEPTH_TWO_OFFSET)) ||
                ctx.has_fwd_edge(*first_neighbour, *second_n2))
            {
                continue;
            }
            ctx.count_group_by_ids(*first_neighbour, *second_neighbour, *second_n2);
        }
    }
}

template <typename KavoshContext, typename NeighbourRange>
void MotifPreprocessor::emit_depth_1_2_3_for_first_vertex(
    KavoshContext& ctx,
    const uint32_t first_degree_vertex,
    const NeighbourRange& second_degree)
{
    for (auto second_vertex = second_degree.m_begin; second_vertex != second_degree.m_end;
         ++second_vertex)
    {
        if (ctx.m_order_index[*second_vertex] < ctx.m_order_index[ctx.m_root] ||
            !ctx.is_at_depth(*second_vertex, static_cast<int64_t>(BFS_DEPTH_TWO_OFFSET)))
        {
            continue;
        }
        const NeighbourIteratorPair three_fwd = ctx.m_graph.get_neighbours(*second_vertex);
        const NeighbourIteratorPair three_rev =
            ctx.m_graph.is_directed() ? ctx.m_graph.get_neighbours(*second_vertex, true)
                                  : std::make_pair(three_fwd.second, three_fwd.second);
        emit_depth_1_2_3_for_second_vertex(
            ctx, first_degree_vertex, *second_vertex,
            NeighbourRange{three_fwd.first, three_fwd.second, three_rev.first, three_rev.second});
    }
    if (ctx.m_graph.is_directed())
    {
        for (auto second_vertex = second_degree.m_rev_begin;
             second_vertex != second_degree.m_rev_end; ++second_vertex)
        {
            if (ctx.m_order_index[*second_vertex] < ctx.m_order_index[ctx.m_root] ||
                !ctx.is_at_depth(*second_vertex, static_cast<int64_t>(BFS_DEPTH_TWO_OFFSET)) ||
                ctx.has_fwd_edge(first_degree_vertex, *second_vertex))
            {
                continue;
            }
            const NeighbourIteratorPair three_fwd = ctx.m_graph.get_neighbours(*second_vertex);
            const NeighbourIteratorPair three_rev =
                ctx.m_graph.is_directed() ? ctx.m_graph.get_neighbours(*second_vertex, true)
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
    const NeighbourRange& third_degree)
{
    for (auto third_vertex = third_degree.m_begin; third_vertex != third_degree.m_end;
         ++third_vertex)
    {
        if (ctx.m_order_index[*third_vertex] < ctx.m_order_index[ctx.m_root])
        {
            continue;
        }
        emit_depth_1_2_3_for_third_vertex(ctx, first_degree_vertex, second_degree_vertex,
                                          *third_vertex);
    }
    if (ctx.m_graph.is_directed())
    {
        for (auto third_vertex = third_degree.m_rev_begin;
             third_vertex != third_degree.m_rev_end; ++third_vertex)
        {
            if (ctx.m_order_index[*third_vertex] < ctx.m_order_index[ctx.m_root] ||
                ctx.has_fwd_edge(second_degree_vertex, *third_vertex))
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
    const uint32_t third_degree_vertex)
{
    const bool is_new            = !ctx.is_visited_in_run(third_degree_vertex);
    const bool is_depth_two_no_back_edge =
        ctx.is_at_depth(third_degree_vertex, static_cast<int64_t>(BFS_DEPTH_TWO_OFFSET)) &&
        !ctx.has_fwd_edge(first_degree_vertex, third_degree_vertex) &&
        !ctx.has_fwd_edge(third_degree_vertex, first_degree_vertex);
    const bool is_depth_three =
        ctx.is_at_depth(third_degree_vertex, static_cast<int64_t>(BFS_DEPTH_THREE_OFFSET));
    if (is_new)
    {
        ctx.mark_at_depth(third_degree_vertex, static_cast<int64_t>(BFS_DEPTH_THREE_OFFSET));
    }
    if (is_new || is_depth_two_no_back_edge || is_depth_three)
    {
        ctx.count_group_by_ids(first_degree_vertex, second_degree_vertex, third_degree_vertex);
    }
}

}  // namespace sgf
