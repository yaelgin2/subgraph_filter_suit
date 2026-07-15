#pragma once

#ifdef __CUDACC__

#include "CucoUInt128CountMap.h"
#include "DeviceGraph.h"
#include "IPathContext.h"
#include "Int128.h"

#include <cstdint>

namespace sgf
{

/**
 * @brief GPU mirror of CpuPathContext — same interface, device-side data.
 *
 * Inherits scalar run state from IPathContext. Constructed once per (root,
 * thread_y) pair inside the kernel body (stack-allocated, no heap). Holds refs
 * to the same three cuco maps MotifPreprocessor's GPU backend uses:
 *   - m_count_ref : uint64_t hash → uint32_t count (atomically incremented)
 *   - m_high_ref  : uint64_t hash → uint64_t (high 64 bits of UInt128)
 *   - m_low_ref   : uint64_t hash → uint64_t (low 64 bits of UInt128)
 *
 * The high/low maps allow the host to reconstruct the full UInt128 after the
 * kernel finishes, even though only a 64-bit hash key is stored on the device.
 */
struct GpuPathContext : IPathContext
{
    /** @brief Function pointer type for backend-specific path recording. */
    using AddPathFn = void (*)(GpuPathContext& ctx, UInt128 motif_id);

    const DeviceGraph& m_graph;   ///< Graph arrays (CSR, colors).
    CucoMotifMapRef m_count_ref;  ///< Count map ref (insert_or_apply).
    CucoAuxMapRef m_high_ref;     ///< High-half map ref (insert only).
    CucoAuxMapRef m_low_ref;      ///< Low-half map ref (insert only).
    uint32_t* m_overflow_flag;    ///< Managed-memory flag set to 1 if a map fills up.
    AddPathFn m_add_path_fn;      ///< Set by PathProcessor to gpu_add_path_to_count.

    /**
     * @brief Construct context, binding the DeviceGraph reference and all scalar/map fields.
     * @param root           Middle vertex of every path enumerated in this context.
     * @param graph          Device CSR graph reference.
     * @param count_ref      Count map device ref.
     * @param high_ref       High-half auxiliary map device ref.
     * @param low_ref        Low-half auxiliary map device ref.
     * @param overflow_flag  Managed-memory flag set to 1 if a map fills up.
     * @param add_path_fn    Backend path recording callback.
     */
    __host__ __device__ GpuPathContext(const uint32_t root, const DeviceGraph& graph,
                                       const CucoMotifMapRef count_ref,
                                       const CucoAuxMapRef high_ref, const CucoAuxMapRef low_ref,
                                       uint32_t* const overflow_flag, const AddPathFn add_path_fn)
        : IPathContext(root)
        , m_graph(graph)
        , m_count_ref(count_ref)
        , m_high_ref(high_ref)
        , m_low_ref(low_ref)
        , m_overflow_flag(overflow_flag)
        , m_add_path_fn(add_path_fn)
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
};

}  // namespace sgf

#endif  // __CUDACC__
