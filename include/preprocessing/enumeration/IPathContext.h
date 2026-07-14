#pragma once

#ifndef SGF_HD
#ifdef __CUDACC__
#define SGF_HD __host__ __device__
#else
#define SGF_HD
#endif
#endif

#include <cstdint>

namespace sgf
{

/**
 * @brief Abstract interface for path enumeration contexts.
 *
 * Holds the single scalar field common to all backends (the root vertex).
 * Neighbour lookup (`get_neighbours`), edge direction handling, and motif
 * recording are intentionally omitted from this interface: CPU and GPU
 * backends return different neighbour-range types and take different
 * function-pointer types for their recording callback, so they cannot share a
 * single virtual signature (see CpuPathContext/GpuPathContext, which each
 * expose these as same-named, non-virtual members instead — the same pattern
 * IKavoshContext uses for get_neighbour_range/mark_neighbours).
 */
struct IPathContext
{
    uint32_t m_root;  ///< Middle vertex of every path enumerated in this context.

    /**
     * @brief Construct a context rooted at @p root.
     * @param root Middle vertex of every path enumerated in this context.
     */
    SGF_HD explicit IPathContext(const uint32_t root) noexcept
        : m_root(root)
    {
    }

    /**
     * @brief Virtual destructor.
     */
    virtual SGF_HD ~IPathContext() noexcept = default;

    IPathContext(const IPathContext&) = delete;
    IPathContext& operator=(const IPathContext&) = delete;
    IPathContext(IPathContext&&) = delete;
    IPathContext& operator=(IPathContext&&) = delete;
};

}  // namespace sgf
