#pragma once

#include "ColoredGraph.h"
#include "CpuNeighbourRange.h"
#include "IGraphPreprocessor.h"
#include "IPathContext.h"
#include "Int128.h"

#include <cstdint>

namespace sgf
{

/**
 * @brief CPU path enumeration context, rooted at a single vertex.
 *
 * Inherits scalar run state from IPathContext. Adds a ColoredGraph reference
 * for neighbour iteration, a per-thread result map, and a function pointer to
 * the backend-specific path recording function set by PathProcessor at
 * construction time — the same shape as CpuKavoshContext, minus the BFS
 * visited-vertex bookkeeping PathProcessor's algorithm does not need.
 */
struct CpuPathContext : IPathContext
{
    /** @brief Function pointer type for backend-specific path recording. */
    using AddPathFn = void (*)(CpuPathContext& ctx, UInt128 motif_id);

    const ColoredGraph& m_graph;  ///< Graph for neighbour/color lookups.
    EnumerationResult& m_result;  ///< Per-thread result map; updated by cpu_add_path_to_count.
    AddPathFn m_add_path_fn;      ///< Set by PathProcessor to cpu_add_path_to_count.

    /**
     * @brief Construct a context, binding all reference members.
     * @param root         Middle vertex of every path enumerated in this context.
     * @param graph        Graph for neighbour/color lookups.
     * @param result       Per-thread result map.
     * @param add_path_fn  Backend path recording callback.
     */
    CpuPathContext(const uint32_t root, const ColoredGraph& graph, EnumerationResult& result,
                   const AddPathFn add_path_fn)
        : IPathContext(root)
        , m_graph(graph)
        , m_result(result)
        , m_add_path_fn(add_path_fn)
    {
    }

    /**
     * @brief Build a CpuNeighbourRange covering fwd and rev neighbours of @p vertex.
     * @param vertex Vertex id to look up.
     */
    CpuNeighbourRange get_neighbour_range(const uint32_t vertex) const
    {
        return sgf::get_neighbour_range(m_graph, vertex);
    }
};

}  // namespace sgf
