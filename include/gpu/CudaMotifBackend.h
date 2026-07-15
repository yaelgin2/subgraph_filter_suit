#pragma once

#ifdef __CUDACC__

#include "Constants.h"
#include "CucoUInt128CountMap.h"
#include "DeviceGraph.h"
#include "IGraphPreprocessor.h"
#include "IKavoshContext.h"
#include "Int128.h"
#include "MotifMap.h"

#include <cstdint>

namespace sgf
{

// ── GpuKavoshContext ──────────────────────────────────────────────────────────

/**
 * @brief GPU mirror of CpuKavoshContext — same interface, device-side data.
 *
 * Inherits scalar run state from IKavoshContext. Constructed once per root inside
 * the kernel body (stack-allocated, no heap). Holds refs to three cuco maps:
 *   - m_count_ref : uint64_t hash → uint32_t count (atomically incremented)
 *   - m_high_ref  : uint64_t hash → uint64_t (high 64 bits of UInt128)
 *   - m_low_ref   : uint64_t hash → uint64_t (low 64 bits of UInt128)
 *
 * The high/low maps allow the host to reconstruct the full UInt128 after the
 * kernel finishes, even though only a 64-bit hash key is stored on the device.
 */
struct GpuKavoshContext : IKavoshContext
{
    /** @brief Function pointer type for backend-specific motif recording. */
    using AddMotifFn = void (*)(GpuKavoshContext& ctx, UInt128 motif_id);

    const DeviceGraph& m_graph;     ///< Graph arrays (CSR, colors).
    CucoMotifMapRef m_count_ref;    ///< Count map ref (insert_or_apply).
    CucoAuxMapRef m_high_ref;       ///< High-half map ref (insert only).
    CucoAuxMapRef m_low_ref;        ///< Low-half map ref (insert only).
    uint32_t* m_overflow_flag;      ///< Managed-memory flag set to 1 if a map fills up.
    const uint32_t* m_order_index;  ///< Position of each vertex in degree-sorted order.
    AddMotifFn m_add_motif_fn;      ///< Set by MotifPreprocessor to gpu_add_motif_to_count.

    /**
     * @brief Construct context, binding the DeviceGraph reference and all scalar/map fields.
     * @param run_id         Root-unique run identifier.
     * @param root           Current root vertex id.
     * @param canonical      Flat canonical array (owned by caller).
     * @param canonical_size Number of entries in canonical.
     * @param graph          Device CSR graph reference.
     * @param count_ref      Count map device ref.
     * @param high_ref       High-half auxiliary map device ref.
     * @param low_ref        Low-half auxiliary map device ref.
     * @param overflow_flag  Managed-memory flag set to 1 if a map fills up.
     * @param order_index    Device pointer to degree-sorted order array.
     * @param add_motif_fn   Backend motif recording callback.
     */
    // NOLINTNEXTLINE(readability-function-size)
    __host__ __device__ GpuKavoshContext(const int64_t run_id, const uint32_t root,
                                         const MotifCanonical* canonical,
                                         const uint32_t canonical_size, const DeviceGraph& graph,
                                         const CucoMotifMapRef count_ref,
                                         const CucoAuxMapRef high_ref, const CucoAuxMapRef low_ref,
                                         uint32_t* const overflow_flag, const uint32_t* order_index,
                                         const AddMotifFn add_motif_fn)
        : IKavoshContext(run_id, root, canonical, canonical_size)
        , m_graph(graph)
        , m_count_ref(count_ref)
        , m_high_ref(high_ref)
        , m_low_ref(low_ref)
        , m_overflow_flag(overflow_flag)
        , m_order_index(order_index)
        , m_add_motif_fn(add_motif_fn)
    {
    }

    /**
     * @brief Return true if @p dest is a forward CSR neighbour of @p src.
     * @param src  Source vertex.
     * @param dest Destination vertex.
     */
    __host__ __device__ bool has_fwd_edge(uint32_t src, uint32_t dest) const noexcept override;

    /**
     * @brief Return true if @p vertex is not a BFS depth-1 neighbour of root.
     *
     * GPU has no BFS visited array.  Depth-1 from root means adjacent via any
     * edge: forward (root→vertex) OR, for directed graphs, reverse (vertex→root).
     *
     * @param vertex Vertex id to query.
     */
    __host__ __device__ bool is_not_at_depth_one(const uint32_t vertex) const noexcept override
    {
        if (vertex == m_root || has_fwd_edge(m_root, vertex))
        {
            return false;
        }
        return !m_graph.is_directed() || !has_fwd_edge(vertex, m_root);
    }

    /**
     * @brief Return true if @p vertex was visited at any depth in this run.
     *
     * GPU has no BFS visited array; always returns false. The (1,2,3) GPU path
     * does not rely on this method.
     *
     * @param vertex Vertex id to query.
     */
    __host__ __device__ bool is_visited_in_run(const uint32_t /*vertex*/) const noexcept override
    {
        return false;
    }

    /**
     * @brief Mark @p vertex at @p depth for this run.
     * @param vertex Vertex id to mark.
     * @param depth  BFS depth offset.
     */
    __host__ __device__ void mark_at_depth(uint32_t vertex, int64_t depth) noexcept override
    {
    }

    /**
     * @brief Build a GpuNeighbourRange covering fwd and rev neighbours of @p vertex.
     * @param vertex Vertex id to look up.
     */
    __host__ __device__ GpuNeighbourRange get_neighbour_range(const uint32_t vertex) const noexcept
    {
        return sgf::get_neighbour_range(m_graph, vertex);
    }

    /**
     * @brief Mark all vertices in @p neighbours_range at @p depth.
     * @param neighbours_range Forward and reverse neighbour range to mark.
     * @param depth            BFS depth offset.
     */
    __host__ __device__ void mark_neighbours(const GpuNeighbourRange& neighbours_range,
                                             const uint32_t depth) noexcept
    {
    }
};

}  // namespace sgf

#endif  // __CUDACC__
