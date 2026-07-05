#pragma once

#ifdef __CUDACC__

#include "Constants.h"
#include "DeviceGraph.h"
#include "IGraphPreprocessor.h"
#include "IKavoshContext.h"
#include "Int128.h"
#include "MotifMap.h"

#include <cstdint>
#include <cuda/std/functional>
#include <cuco/static_map.cuh>
#include <thrust/functional.h>

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

// ── MurmurHash3 64-bit finaliser (UInt128 → uint64_t) ────────────────────────

/**
 * @brief Seeded 64-bit hash of a UInt128 key using MurmurHash3_x64_128.
 *
 * Hashes each 64-bit half independently with cuco::detail::MurmurHash3_x64_128
 * seeded by @p seed, takes the first 64-bit output half of each, and XORs them.
 * The seed separates the primary key (seed=0) from the double-hash probe step
 * (seed=1), keeping probe sequences deterministic and independent per UInt128.
 */
struct UInt128MurmurHash
{
    /**
     * @brief Hash @p key with @p seed: MurmurHash3(high, seed)[0] ^ MurmurHash3(low, seed)[0].
     * @param key  128-bit canonical motif identifier.
     * @param seed 0 for the primary slot key; 1 for the double-hash probe step.
     * @return 64-bit hash value.
     */
    __host__ __device__
    static uint64_t hash(const UInt128& key, const uint64_t seed) noexcept
    {
        const cuco::detail::MurmurHash3_x64_128<uint64_t> hasher{seed};
        return hasher(key.m_high)[0] ^ hasher(key.m_low)[0];
    }

    __host__ __device__
    uint64_t operator()(const UInt128& key) const noexcept
    {
        return hash(key, 0ULL);
    }
};

/** @brief MurmurHash3 hasher for uint64_t slot keys used by cuco linear probing. */
struct UInt64Hash
{
    __host__ __device__
    uint32_t operator()(const uint64_t key) const noexcept
    {
        const cuco::detail::MurmurHash3_x64_128<uint64_t> hasher{0ULL};
        return static_cast<uint32_t>(hasher(key)[0]);
    }
};

// ── cuco map type aliases ─────────────────────────────────────────────────────

/** @brief Count map: MurmurHash3(UInt128) → occurrence count. */
using CucoMotifMap = cuco::static_map<
    uint64_t,
    uint32_t,
    cuco::extent<std::size_t>,
    cuda::thread_scope_device,
    thrust::equal_to<uint64_t>,
    cuco::linear_probing<1U, UInt64Hash>>;

/** @brief Auxiliary map: MurmurHash3(UInt128) → one 64-bit half of the original key. */
using CucoAuxMap = cuco::static_map<
    uint64_t,
    uint64_t,
    cuco::extent<std::size_t>,
    cuda::thread_scope_device,
    thrust::equal_to<uint64_t>,
    cuco::linear_probing<1U, UInt64Hash>>;

/** @brief Device-side ref type for count updates (insert_or_apply). */
using CucoMotifMapRef =
    decltype(std::declval<CucoMotifMap&>().ref(cuco::op::insert_or_apply_tag{}));

/** @brief Device-side ref type for auxiliary half-key storage (insert_and_find for collision check). */
using CucoAuxMapRef =
    decltype(std::declval<CucoAuxMap&>().ref(cuco::op::insert_and_find_tag{}));

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

    const DeviceGraph&   m_graph;          ///< Graph arrays (CSR, colors).
    CucoMotifMapRef      m_count_ref;      ///< Count map ref (insert_or_apply).
    CucoAuxMapRef        m_high_ref;       ///< High-half map ref (insert only).
    CucoAuxMapRef        m_low_ref;        ///< Low-half map ref (insert only).
    const uint32_t*      m_order_index;    ///< Position of each vertex in degree-sorted order.
    AddMotifFn           m_add_motif_fn;   ///< Set by MotifPreprocessor to gpu_add_motif_to_count.

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
     * @param order_index    Device pointer to degree-sorted order array.
     * @param add_motif_fn   Backend motif recording callback.
     */
    __host__ __device__ GpuKavoshContext(const int64_t run_id,
                                const uint32_t root,
                                const MotifCanonical* canonical,
                                const uint32_t canonical_size,
                                const DeviceGraph& graph,
                                const CucoMotifMapRef count_ref,
                                const CucoAuxMapRef high_ref,
                                const CucoAuxMapRef low_ref,
                                const uint32_t* order_index,
                                const AddMotifFn add_motif_fn)
        : IKavoshContext(run_id, root, canonical, canonical_size),
          m_graph(graph),
          m_count_ref(count_ref),
          m_high_ref(high_ref),
          m_low_ref(low_ref),
          m_order_index(order_index),
          m_add_motif_fn(add_motif_fn)
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
    __host__ __device__ void mark_at_depth(uint32_t vertex, int64_t depth) noexcept override {}

    /**
     * @brief Build a GpuNeighbourRange covering fwd and rev neighbours of @p vertex.
     * @param vertex Vertex id to look up.
     */
    __host__ __device__ GpuNeighbourRange get_neighbour_range(const uint32_t vertex) const noexcept
    {
        const uint32_t* const nbr     = m_graph.d_fwd_neighbors;
        const uint32_t fwd_start      = m_graph.d_fwd_offsets[vertex];
        const uint32_t fwd_end        = m_graph.d_fwd_offsets[vertex + 1U];
        const bool directed           = m_graph.is_directed();
        const uint32_t* const rev_nbr = directed ? m_graph.d_rev_neighbors : nbr;
        const uint32_t rev_start      = directed ? m_graph.d_rev_offsets[vertex]      : fwd_end;
        const uint32_t rev_end        = directed ? m_graph.d_rev_offsets[vertex + 1U] : fwd_end;
        return GpuNeighbourRange{nbr + fwd_start, nbr + fwd_end,
                                  rev_nbr + rev_start, rev_nbr + rev_end};
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

// ── Helper declarations ───────────────────────────────────────────────────────

/**
 * @brief Allocate a cuco count map sized for @p num_nodes vertices.
 * @param num_nodes Number of vertices in the graph.
 */
CucoMotifMap make_cuco_motif_map(uint32_t num_nodes);

/**
 * @brief Allocate a cuco auxiliary half-key map sized for @p num_nodes vertices.
 * @param num_nodes Number of vertices in the graph.
 */
CucoAuxMap make_cuco_aux_map(uint32_t num_nodes);

/**
 * @brief Reconstruct EnumerationResult from the three device maps.
 *
 * Iterates the count map; for each (hash_key, count), looks up hash_key in
 * high_map and low_map to reconstruct the original UInt128 motif id.
 *
 * @param count_map Populated count map.
 * @param high_map  Map from hash key to high 64 bits of UInt128.
 * @param low_map   Map from hash key to low 64 bits of UInt128.
 */
EnumerationResult cuco_maps_to_enumeration_result(const CucoMotifMap& count_map,
                                                   const CucoAuxMap& high_map,
                                                   const CucoAuxMap& low_map);

}  // namespace sgf

#endif  // __CUDACC__
