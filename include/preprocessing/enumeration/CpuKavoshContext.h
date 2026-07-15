#pragma once

#include "ColoredGraph.h"
#include "CpuNeighbourRange.h"
#include "IGraphPreprocessor.h"
#include "IKavoshContext.h"
#include "Int128.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace sgf
{

/**
 * @brief Kavosh BFS context for CPU enumeration, rooted at a single vertex.
 *
 * Inherits scalar run state from IKavoshContext. Adds a ColoredGraph reference
 * for neighbour iteration, a per-thread BFS visited-depth array, a group-emission
 * callback, and a pointer to the backend-specific motif recording function set by
 * MotifPreprocessor at construction time.
 *
 * Edge checks use binary search on the sorted ColoredGraph neighbour lists —
 * identical O(log d) complexity to the GPU CSR path.
 */
struct CpuKavoshContext : IKavoshContext
{
    /** @brief Function pointer type for backend-specific motif recording. */
    using AddMotifFn = void (*)(CpuKavoshContext& ctx, UInt128 motif_id);

    const ColoredGraph& m_graph;  ///< Graph for neighbour/edge lookups.
    EnumerationResult& m_result;  ///< Per-thread result map; updated by cpu_add_motif_to_count.
    std::vector<int64_t>& m_bfs_visited;         ///< BFS depth-encoding array.
    const std::vector<uint32_t>& m_order_index;  ///< Vertex position in degree-sorted order.
    AddMotifFn m_add_motif_fn;  ///< Set by MotifPreprocessor to cpu_add_motif_to_count.

    /**
     * @brief Construct a context, binding all reference members.
     * @param run_id         Root-unique run identifier.
     * @param root           Current root vertex id.
     * @param canonical      Flat canonical array (owned by caller).
     * @param canonical_size Number of entries in canonical.
     * @param graph          Graph for neighbour/edge lookups.
     * @param result         Per-thread result map.
     * @param bfs_visited    BFS depth-encoding array.
     * @param order_index    Vertex position in degree-sorted order.
     * @param add_motif_fn   Backend motif recording callback.
     */
    CpuKavoshContext(const int64_t run_id, const uint32_t root, const MotifCanonical* canonical,
                     const uint32_t canonical_size, const ColoredGraph& graph,
                     EnumerationResult& result, std::vector<int64_t>& bfs_visited,
                     const std::vector<uint32_t>& order_index, const AddMotifFn add_motif_fn)
        : IKavoshContext{run_id, root, canonical, canonical_size}
        , m_graph(graph)
        , m_result(result)
        , m_bfs_visited(bfs_visited)
        , m_order_index(order_index)
        , m_add_motif_fn(add_motif_fn)
    {
    }

    /**
     * @brief Return true if a forward edge exists from @p src to @p dest.
     */
    SGF_HD bool has_fwd_edge(const uint32_t src, const uint32_t dest) const noexcept override
    {
        const auto [begin, end] = m_graph.get_neighbours(src);
        return std::binary_search(begin, end, dest);
    }

    /**
     * @brief Return true if @p vertex was marked at @p depth in this run.
     */
    SGF_HD bool is_not_at_depth_one(const uint32_t vertex) const noexcept override
    {
        return (m_root != vertex) && (m_bfs_visited[vertex] != m_run_id + 1);
    }

    /**
     * @brief Return true if @p vertex was visited at any depth in this run.
     */
    SGF_HD bool is_visited_in_run(const uint32_t vertex) const noexcept override
    {
        return (static_cast<uint64_t>(m_bfs_visited[vertex]) >> BFS_VERTEX_RUN_SHIFT) ==
               static_cast<uint64_t>(m_root);
    }

    /**
     * @brief Mark @p vertex at @p depth for this run.
     */
    SGF_HD void mark_at_depth(const uint32_t vertex, const int64_t depth) noexcept override
    {
        if (!is_visited_in_run(vertex))
        {
            m_bfs_visited[vertex] = m_run_id + depth;
        }
    }

    /**
     * @brief Build a CpuNeighbourRange covering fwd and rev neighbours of @p vertex.
     * @param vertex Vertex id to look up.
     */
    CpuNeighbourRange get_neighbour_range(const uint32_t vertex) const
    {
        return sgf::get_neighbour_range(m_graph, vertex);
    }

    /**
     * @brief Mark all vertices in @p neighbours_range at @p depth.
     */
    void mark_neighbours(const CpuNeighbourRange& neighbours_range, const uint32_t depth)
    {
        for (auto it = neighbours_range.m_begin; it != neighbours_range.m_end; ++it)
        {
            if (!is_visited_in_run(*it))
            {
                mark_at_depth(*it, depth);
            }
        }
        if (m_graph.is_directed())
        {
            for (auto it = neighbours_range.m_rev_begin; it != neighbours_range.m_rev_end; ++it)
            {
                if (!is_visited_in_run(*it))
                {
                    mark_at_depth(*it, depth);
                }
            }
        }
    }
};

}  // namespace sgf
