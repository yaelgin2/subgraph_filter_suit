#include "CudaMotifBackend.h"

#ifdef SGF_CUDA_ENABLED

// Include MotifPreprocessor AFTER CudaMotifBackend.h so GpuKavoshContext is already
// defined when the SGF_HD templates in MotifPreprocessor.h are instantiated here.
#include "MotifPreprocessor.h"

#include "Constants.h"
#include "DeviceGraph.h"
#include "Int128.h"
#include "MotifMap.h"

#include <cuda_runtime.h>
#include <cuco/static_map.cuh>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace sgf
{

namespace
{

/// Block dimension used for x and y axes in the 2D kernel launch.
constexpr uint32_t BLOCK_DIM = 16U;

/**
 * @brief Integer ceiling-division.
 * @param numerator   Dividend.
 * @param denominator Divisor (non-zero).
 * @return ceil(numerator / denominator).
 */
__host__ __device__ uint32_t ceil_div(const uint32_t numerator,
                                       const uint32_t denominator) noexcept
{
    return (numerator + denominator - 1U) / denominator;
}

}  // namespace

// ── GpuKavoshContext device method implementations ────────────────────────────

__device__ bool GpuKavoshContext::has_fwd_edge(const uint32_t src,
                                                const uint32_t dest) const noexcept
{
    const uint32_t first = m_graph.d_fwd_offsets[src];
    const uint32_t last  = m_graph.d_fwd_offsets[src + 1U];
    uint32_t low  = first;
    uint32_t high = last;
    while (low < high)
    {
        const uint32_t mid = low + (high - low) / 2U;
        if (m_graph.d_fwd_neighbors[mid] == dest)
        {
            return true;
        }
        if (m_graph.d_fwd_neighbors[mid] < dest)
        {
            low = mid + 1U;
        }
        else
        {
            high = mid;
        }
    }
    return false;
}

__device__ void GpuKavoshContext::count_group_by_ids(const uint32_t n1,
                                                      const uint32_t n2,
                                                      const uint32_t n3) const noexcept
{
    const uint32_t group[4] = {m_root, n1, n2, n3};
    const uint32_t desc = MotifPreprocessor::compute_motif_descriptor(
        group,
        m_graph.is_directed,
        [this] __device__(const uint32_t src, const uint32_t dest) noexcept
        {
            return has_fwd_edge(src, dest);
        });

    const uint32_t colors[4] = {
        m_graph.d_colors[m_root],
        m_graph.d_colors[n1],
        m_graph.d_colors[n2],
        m_graph.d_colors[n3]
    };

    const UInt128 motif_id = MotifPreprocessor::calculate_motif_number_from_arrays(
        desc, colors, m_canonical, m_canonical_size);

    m_map_ref.insert_or_apply(
        motif_id, 1U,
        [] __device__(uint32_t& existing, const uint32_t increment) noexcept
        {
            atomicAdd(&existing, increment);
        });
}

// ── GPU first-layer driver functions ─────────────────────────────────────────
//
// Each driver strides over root's forward CSR neighbours as n1 (the first
// depth-1 vertex), then constructs GpuNeighbourRange objects and delegates to
// the shared SGF_HD BFS templates defined in MotifPreprocessor.h.
// The NeighIter template parameter is deduced as const uint32_t*.

/**
 * @brief GPU driver for (1,1,1) groups.
 *
 * @param ctx           GPU BFS run context for the current root.
 * @param thread_y_offset Y-dimension thread offset for striding over n1.
 * @param stride_y      Total y-dimension stride across all threads.
 */
__device__ void MotifPreprocessor::emit_depth_1_1_1_groups_gpu(
    GpuKavoshContext& ctx,
    const uint32_t thread_y_offset,
    const uint32_t stride_y) const
{
    const uint32_t root_start = ctx.m_graph.d_fwd_offsets[ctx.m_root];
    const uint32_t root_end   = ctx.m_graph.d_fwd_offsets[ctx.m_root + 1U];
    const uint32_t* const nbr = ctx.m_graph.d_fwd_neighbors;

    const GpuNeighbourRange depth_one{nbr + root_start, nbr + root_end,
                                       nbr + root_end,   nbr + root_end};

    mark_depth_one_neighbours(ctx, depth_one);

    for (uint32_t n1_idx = root_start + thread_y_offset; n1_idx < root_end; n1_idx += stride_y)
    {
        const uint32_t n1 = nbr[n1_idx];
        if (ctx.m_order_index[n1] < ctx.m_order_index[ctx.m_root])
        {
            continue;
        }
        emit_depth_1_1_1_groups_first_vertex_chosen(ctx, depth_one, nbr + n1_idx, false);
    }
    // Directed: reverse edges handled via d_rev_* CSR (future work).
}

/**
 * @brief GPU driver for (1,1,2) and (1,2,2) groups.
 *
 * @param ctx           GPU BFS run context.
 * @param thread_y_offset Y-dimension thread offset.
 * @param stride_y      Total y-dimension stride.
 */
// NOLINTNEXTLINE(readability-function-size)
__device__ void MotifPreprocessor::emit_depth_1_1_2_and_1_2_2_groups_gpu(
    GpuKavoshContext& ctx,
    const uint32_t thread_y_offset,
    const uint32_t stride_y) const
{
    const uint32_t root_start = ctx.m_graph.d_fwd_offsets[ctx.m_root];
    const uint32_t root_end   = ctx.m_graph.d_fwd_offsets[ctx.m_root + 1U];
    const uint32_t* const nbr = ctx.m_graph.d_fwd_neighbors;

    const GpuNeighbourRange depth_one{nbr + root_start, nbr + root_end,
                                       nbr + root_end,   nbr + root_end};

    for (uint32_t n1_idx = root_start + thread_y_offset; n1_idx < root_end; n1_idx += stride_y)
    {
        const uint32_t n1 = nbr[n1_idx];
        if (ctx.m_order_index[n1] < ctx.m_order_index[ctx.m_root])
        {
            continue;
        }

        const uint32_t n2_start = ctx.m_graph.d_fwd_offsets[n1];
        const uint32_t n2_end   = ctx.m_graph.d_fwd_offsets[n1 + 1U];
        const GpuNeighbourRange depth_two{nbr + n2_start, nbr + n2_end,
                                           nbr + n2_end,   nbr + n2_end};

        mark_depth_two_neighbours(ctx, depth_two);
        emit_depth_1_1_2_for_first_vertex(ctx, nbr + n1_idx, depth_one, depth_two);
        emit_depth_1_2_2_for_first_vertex(ctx, nbr + n1_idx, depth_two);
    }
}

/**
 * @brief GPU driver for (1,2,3) groups.
 *
 * @param ctx           GPU BFS run context.
 * @param thread_y_offset Y-dimension thread offset.
 * @param stride_y      Total y-dimension stride.
 */
__device__ void MotifPreprocessor::emit_depth_1_2_3_groups_gpu(
    GpuKavoshContext& ctx,
    const uint32_t thread_y_offset,
    const uint32_t stride_y) const
{
    const uint32_t root_start = ctx.m_graph.d_fwd_offsets[ctx.m_root];
    const uint32_t root_end   = ctx.m_graph.d_fwd_offsets[ctx.m_root + 1U];
    const uint32_t* const nbr = ctx.m_graph.d_fwd_neighbors;

    for (uint32_t n1_idx = root_start + thread_y_offset; n1_idx < root_end; n1_idx += stride_y)
    {
        const uint32_t n1 = nbr[n1_idx];
        if (ctx.m_order_index[n1] < ctx.m_order_index[ctx.m_root])
        {
            continue;
        }

        const uint32_t n2_start = ctx.m_graph.d_fwd_offsets[n1];
        const uint32_t n2_end   = ctx.m_graph.d_fwd_offsets[n1 + 1U];
        const GpuNeighbourRange depth_two{nbr + n2_start, nbr + n2_end,
                                           nbr + n2_end,   nbr + n2_end};

        // Iterate depth-2 neighbours manually (emit_depth_1_2_3_for_first_vertex
        // calls m_graph.get_neighbours() which is CPU-only; we replicate its logic here).
        for (const uint32_t* n2_ptr = nbr + n2_start; n2_ptr != nbr + n2_end; ++n2_ptr)
        {
            const uint32_t n2 = *n2_ptr;
            if (ctx.m_order_index[n2] < ctx.m_order_index[ctx.m_root] ||
                !ctx.is_at_depth(n2, static_cast<int64_t>(BFS_DEPTH_TWO_OFFSET)))
            {
                continue;
            }
            const uint32_t n3_start = ctx.m_graph.d_fwd_offsets[n2];
            const uint32_t n3_end   = ctx.m_graph.d_fwd_offsets[n2 + 1U];
            const GpuNeighbourRange depth_three{nbr + n3_start, nbr + n3_end,
                                                 nbr + n3_end,   nbr + n3_end};
            emit_depth_1_2_3_for_second_vertex(ctx, n1, n2, depth_three);
        }
    }
}

// ── Kernel ────────────────────────────────────────────────────────────────────

/**
 * @brief 2-D Kavosh motif kernel.
 *
 * x-threads cover roots; y-threads stripe depth-1 neighbours. Each thread
 * processes its assigned (root, n1) pairs via the three GPU driver functions.
 *
 * @param graph          Graph arrays in managed memory (raw pointers, copied into
 *                       registers at launch — not per-thread heap allocation).
 * @param map_ref        Device-side cuco map reference for atomic updates.
 * @param canonical      Flat canonical array built from UNDIRECTED/DIRECTED maps.
 * @param canonical_size Number of entries in @p canonical.
 * @param sorted_nodes   Vertices in degree-sorted order (m_node_order), managed memory.
 * @param order_index    Position of each vertex in degree-sorted order, managed memory.
 */
template <typename MapRef>
__global__ void motif4_kernel(const DeviceGraph graph,
                               MapRef map_ref,
                               const MotifCanonical* const canonical,
                               const uint32_t canonical_size,
                               const uint32_t* const sorted_nodes,
                               const uint32_t* const order_index)
{
    const uint32_t thread_x  = blockIdx.x * blockDim.x + threadIdx.x;
    const uint32_t thread_y  = blockIdx.y * blockDim.y + threadIdx.y;
    const uint32_t stride_x  = blockDim.x * gridDim.x;
    const uint32_t stride_y  = blockDim.y * gridDim.y;
    const uint32_t thread_id = thread_x + thread_y * stride_x;

    for (uint32_t idx = thread_x; idx < graph.num_nodes; idx += stride_x)
    {
        const uint32_t root = sorted_nodes[idx];
        const int64_t run_id =
            static_cast<int64_t>(static_cast<uint64_t>(root)
                                  << GpuKavoshContext::BFS_VERTEX_RUN_SHIFT);

        GpuKavoshContext ctx{graph, run_id, root, canonical, canonical_size, map_ref,
                              order_index};

        // Reuse shared templates via GPU driver functions.
        emit_depth_1_1_1_groups_gpu(ctx, thread_y, stride_y);
        emit_depth_1_1_2_and_1_2_2_groups_gpu(ctx, thread_y, stride_y);
        emit_depth_1_2_3_groups_gpu(ctx, thread_y, stride_y);
    }
}

// ── calculate_gpu ─────────────────────────────────────────────────────────────

EnumerationResult MotifPreprocessor::calculate_gpu()
{
    DeviceGraph device_graph = DeviceGraphBuilder::build(m_graph);

    uint32_t* d_order_index = nullptr;
    uint32_t* d_sorted_nodes = nullptr;
    const std::size_t vertex_bytes =
        static_cast<std::size_t>(device_graph.num_nodes) * sizeof(uint32_t);
    cudaMallocManaged(&d_order_index, vertex_bytes);
    cudaMallocManaged(&d_sorted_nodes, vertex_bytes);
    std::memcpy(d_order_index, m_order_index.data(), vertex_bytes);
    std::memcpy(d_sorted_nodes, m_node_order.data(), vertex_bytes);

    // Build flat MotifCanonical array in managed memory (MotifCanonical now uses
    // fixed arrays so it is trivially copyable and device-readable).
    const std::unordered_map<uint32_t, MotifCanonical>& canonical_map =
        m_graph.is_directed() ? DIRECTED_MOTIF_CANONICAL_MAP : UNDIRECTED_MOTIF_CANONICAL_MAP;

    uint32_t max_descriptor = 0U;
    for (const auto& [descriptor, entry] : canonical_map)
    {
        if (descriptor > max_descriptor)
        {
            max_descriptor = descriptor;
        }
    }
    const uint32_t canonical_size = max_descriptor + 1U;

    MotifCanonical* device_canonical = nullptr;
    cudaMallocManaged(&device_canonical, canonical_size * sizeof(MotifCanonical));
    for (const auto& [descriptor, entry] : canonical_map)
    {
        device_canonical[descriptor] = entry;
    }

    CucoMotifMap motif_map = make_cuco_motif_map(device_graph.num_nodes);
    CucoMotifMapRef map_ref = motif_map.ref(cuco::op::insert_or_apply{});

    const dim3 block_size(BLOCK_DIM, BLOCK_DIM);
    const dim3 num_blocks(ceil_div(device_graph.num_nodes, BLOCK_DIM),
                           ceil_div(device_graph.num_nodes, BLOCK_DIM));
    const uint32_t total_threads = num_blocks.x * BLOCK_DIM * num_blocks.y * BLOCK_DIM;

    // Allocate per-thread bfs_visited buffers.
    int64_t* bfs_buffer = nullptr;
    cudaMallocManaged(&bfs_buffer,
                       static_cast<std::size_t>(total_threads) *
                       static_cast<std::size_t>(device_graph.num_nodes) * sizeof(int64_t));

    motif4_kernel<<<num_blocks, block_size>>>(device_graph, map_ref,
                                               device_canonical, canonical_size,
                                               bfs_buffer, d_sorted_nodes, d_order_index);
    cudaDeviceSynchronize();

    cudaFree(bfs_buffer);
    cudaFree(d_order_index);
    cudaFree(d_sorted_nodes);
    cudaFree(device_canonical);
    DeviceGraphBuilder::free_graph(device_graph);

    return cuco_map_to_enumeration_result(motif_map);
}

// ── Helper implementations ────────────────────────────────────────────────────

CucoMotifMap make_cuco_motif_map(const uint32_t num_nodes)
{
    const std::size_t capacity =
        static_cast<std::size_t>(num_nodes) *
        static_cast<std::size_t>(num_nodes) * 2UL;
    return CucoMotifMap{cuco::extent<std::size_t>(capacity),
                         cuco::empty_key{UInt128{}},
                         cuco::empty_value{uint32_t{0U}},
                         thrust::equal_to<UInt128>{},
                         cuco::linear_probing<4U, UInt128DeviceHash>{}};
}

EnumerationResult cuco_map_to_enumeration_result(const CucoMotifMap& motif_map)
{
    EnumerationResult result;
    motif_map.for_each(
        [&result] __host__(const UInt128& key, const uint32_t value)
        {
            result[key] += value;
        });
    return result;
}

}  // namespace sgf

#endif  // SGF_CUDA_ENABLED
