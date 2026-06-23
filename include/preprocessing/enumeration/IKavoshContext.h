#pragma once

#ifndef SGF_HD
#ifdef __CUDACC__
#define SGF_HD __host__ __device__
#else
#define SGF_HD
#endif
#endif

#include "MotifMap.h"

#include <cstdint>

namespace sgf
{

/**
 * @brief Abstract interface for Kavosh BFS motif enumeration contexts.
 *
 * Holds the scalar fields common to all backends and declares the pure-virtual
 * operations the shared SGF_HD BFS helpers call through the context. Derived
 * structs add the backend-specific graph reference, visited-vertex bookkeeping,
 * and output storage (callback on CPU, cuco maps on GPU).
 *
 * mark_neighbours is intentionally omitted from the interface: the CPU and GPU
 * backends take different neighbour-range types (CpuNeighbourRange vs
 * GpuNeighbourRange) and therefore cannot share a single virtual signature.
 */
struct IKavoshContext
{
    int64_t               m_run_id;         ///< Root-unique run identifier (upper bits = root id).
    uint32_t              m_root;           ///< Current root vertex id.
    const MotifCanonical* m_canonical;      ///< Flat canonical array (owned by caller).
    uint32_t              m_canonical_size; ///< Number of entries in m_canonical.

    /// Low 2 bits of each bfs_visited entry encode BFS depth; upper bits hold run_id.
    static constexpr uint64_t BFS_VERTEX_RUN_SHIFT = 2U;

    /**
     * @brief Construct a context with all scalar fields.
     * @param run_id         Root-unique run identifier.
     * @param root           Current root vertex id.
     * @param canonical      Flat canonical array (owned by caller).
     * @param canonical_size Number of entries in canonical.
     */
    SGF_HD IKavoshContext(const int64_t run_id,
                          const uint32_t root,
                          const MotifCanonical* canonical,
                          const uint32_t canonical_size) noexcept
        : m_run_id(run_id), m_root(root), m_canonical(canonical), m_canonical_size(canonical_size)
    {
    }

    /**
     * @brief Virtual destructor.
     */
    virtual SGF_HD ~IKavoshContext() noexcept = default;

    /**
     * @brief Return true if a forward edge exists from @p src to @p dest.
     * @param src  Source vertex id.
     * @param dest Destination vertex id.
     */
    virtual SGF_HD bool has_fwd_edge(uint32_t src, uint32_t dest) const noexcept = 0;

    /**
     * @brief Return true if @p vertex was marked at @p depth in this run.
     * @param vertex Vertex id to query.
     * @param depth  BFS depth offset.
     */
    virtual SGF_HD bool is_at_depth(uint32_t vertex, int64_t depth) const noexcept = 0;

    /**
     * @brief Return true if @p vertex was visited at any depth in this run.
     * @param vertex Vertex id to query.
     */
    virtual SGF_HD bool is_visited_in_run(uint32_t vertex) const noexcept = 0;

    /**
     * @brief Mark @p vertex at @p depth for this run.
     * @param vertex Vertex id to mark.
     * @param depth  BFS depth offset.
     */
    virtual SGF_HD void mark_at_depth(uint32_t vertex, int64_t depth) noexcept = 0;

};

}  // namespace sgf
