#pragma once

#ifdef SGF_CUDA_ENABLED

#include "Constants.h"
#include "DeviceGraph.h"
#include "IGraphPreprocessor.h"
#include "Int128.h"
#include "MotifMap.h"

#include <cstdint>
#include <cuda/std/functional>
#include <cuco/static_map.cuh>
#include <thrust/equal_to.h>

namespace sgf
{

// ── GpuNeighbourRange ─────────────────────────────────────────────────────────

/**
 * @brief Raw-pointer neighbour range for GPU Kavosh BFS traversal.
 *
 * GPU equivalent of CpuNeighbourRange but uses const uint32_t* pointers into
 * the DeviceGraph CSR arrays. Template parameter NeighIter in the shared BFS
 * templates is deduced as const uint32_t* when a GpuNeighbourRange is passed.
 *
 * For undirected graphs set m_rev_begin == m_rev_end (both nullptr or same value).
 */
struct GpuNeighbourRange
{
    const uint32_t* m_begin;     ///< Pointer to first forward neighbour in CSR slice.
    const uint32_t* m_end;       ///< One past last forward neighbour.
    const uint32_t* m_rev_begin; ///< Pointer to first reverse neighbour (directed only).
    const uint32_t* m_rev_end;   ///< One past last reverse neighbour.
};

// ── Custom device hasher for UInt128 ─────────────────────────────────────────

/**
 * @brief FNV-1a–inspired device-safe hash for UInt128 keys.
 */
struct UInt128DeviceHash
{
    __host__ __device__
    static inline uint64_t fmix64(uint64_t x)
    {
        x ^= x >> 33;
        x *= 0xff51afd7ed558ccdULL;
        x ^= x >> 33;
        x *= 0xc4ceb9fe1a85ec53ULL;
        x ^= x >> 33;
        return x;
    }

    __host__ __device__
    uint32_t operator()(const UInt128& key) const noexcept
    {
        uint64_t h = key.m_low;
        h ^= fmix64(key.m_high + 0x9e3779b97f4a7c15ULL);
        h = fmix64(h);
        return static_cast<uint32_t>(h ^ (h >> 32));
    }
};

// ── cuco map type aliases ─────────────────────────────────────────────────────

/** @brief GPU hash map from canonical motif id (UInt128) to occurrence count. */
using CucoMotifMap = cuco::static_map<
    UInt128,
    uint32_t,
    cuco::extent<std::size_t>,
    cuda::thread_scope_device,
    thrust::equal_to<UInt128>,
    cuco::linear_probing<4U, UInt128DeviceHash>>;

/** @brief Device-side reference type used inside kernels. */
using CucoMotifMapRef = CucoMotifMap::ref_type<cuco::op::insert_or_apply_tag>;

// ── GpuKavoshContext ──────────────────────────────────────────────────────────

/**
 * @brief GPU mirror of CpuKavoshContext — same interface, device-side data.
 *
 * Constructed once per root inside the kernel body (stack-allocated, no heap).
 * Provides the same has_fwd_edge / is_at_depth / mark_at_depth / is_visited_in_run /
 * count_group_by_ids interface as CpuKavoshContext so the shared SGF_HD BFS
 * templates (emit_depth_1_1_1_*, emit_depth_1_1_2_*, etc.) compile and run
 * correctly for both CPU and GPU without duplication.
 *
 * Edge existence: binary search on sorted CSR neighbour slice (same O(log d) as CPU).
 * Depth tracking: per-thread m_bfs_visited array allocated in global memory.
 */
struct GpuKavoshContext
{
    const DeviceGraph& m_graph;          ///< Graph arrays (CSR, colors).
    int64_t            m_run_id;         ///< Root-unique run identifier (upper bits = root).
    uint32_t           m_root;           ///< Current root vertex id.
    const MotifCanonical* m_canonical;   ///< Flat canonical array from calculate_gpu().
    uint32_t           m_canonical_size; ///< Number of entries in m_canonical.
    CucoMotifMapRef    m_map_ref;        ///< Atomic motif count map.
    const uint32_t*    m_order_index;    ///< Position of each vertex in degree-sorted order.

    /// Must match MotifPreprocessor::BFS_VERTEX_RUN_SHIFT.
    static constexpr uint64_t BFS_VERTEX_RUN_SHIFT = 2U;

    /**
     * @brief Return true if @p dest is a forward CSR neighbour of @p src.
     *
     * Binary search on sorted CSR slice [d_fwd_offsets[src], d_fwd_offsets[src+1]).
     *
     * @param src  Source vertex.
     * @param dest Destination vertex.
     * @return True if (src, dest) is a forward edge.
     */
    __device__ bool has_fwd_edge(uint32_t src, uint32_t dest) const noexcept;

    /**
     * @brief Return true if @p vertex was marked at @p depth in this run.
     *
     * @param vertex Vertex id to query.
     * @param depth  BFS depth offset (e.g. BFS_DEPTH_TWO_OFFSET).
     */
    __device__ bool is_at_depth(uint32_t vertex, int64_t depth) const noexcept
    {
        
    }

    /**
     * @brief Return true if @p vertex was visited at any depth in this run.
     *
     * @param vertex Vertex id to query.
     */
    __device__ bool is_visited_in_run(uint32_t vertex) const noexcept
    {
        
    }

    /**
     * @brief Mark @p vertex at @p depth for this run.
     *
     * @param vertex Vertex id to mark.
     * @param depth  BFS depth offset.
     */
    __device__ void mark_at_depth(uint32_t vertex, int64_t depth) noexcept
    { 

    }

    void mark_neighbours(const GpuNeighbourRange& neighbours_range, const uint32_t depth) {
    }

    /**
     * @brief Compute descriptor and atomically count the group (root, n1, n2, n3).
     *
     * Calls MotifPreprocessor::compute_motif_descriptor (SGF_HD) with a CSR-based
     * edge checker, then MotifPreprocessor::calculate_motif_number_from_arrays
     * (SGF_HD), and finally inserts into the cuco map with an atomic increment.
     *
     * @param n1 First non-root group member.
     * @param n2 Second non-root group member.
     * @param n3 Third non-root group member.
     */
    __device__ void count_group_by_ids(uint32_t n1, uint32_t n2, uint32_t n3) const noexcept;
};

// ── Helper declarations ───────────────────────────────────────────────────────

/**
 * @brief Allocate a cuco motif map sized for @p num_nodes vertices.
 * @param num_nodes Number of vertices in the graph.
 * @return Heap-allocated CucoMotifMap with sufficient capacity.
 */
CucoMotifMap make_cuco_motif_map(uint32_t num_nodes);

/**
 * @brief Convert a populated CucoMotifMap to an EnumerationResult.
 * @param motif_map Populated device map to drain.
 * @return Host-side map from canonical motif id to occurrence count.
 */
EnumerationResult cuco_map_to_enumeration_result(const CucoMotifMap& motif_map);

}  // namespace sgf

#endif  // SGF_CUDA_ENABLED
