#include "CudaMotifBackend.h"

#ifdef __CUDACC__

// Include MotifPreprocessor AFTER CudaMotifBackend.h so GpuKavoshContext is already
// defined when the SGF_HD templates in MotifPreprocessor.h are instantiated here.
#include "Constants.h"
#include "CucoUInt128CountMap.h"
#include "DeviceGraph.h"
#include "Int128.h"
#include "MotifMap.h"
#include "MotifPreprocessor.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cuda_runtime.h>
#include <unordered_map>
#include <vector>

namespace sgf
{

namespace
{

/**
 * @brief Prefetches a managed buffer to a device, skipping empty buffers.
 *
 * A zero-byte cudaMallocManaged allocation returns a null pointer. Prefetching
 * that null/empty buffer fails and leaves a pending CUDA error that surfaces
 * on the next unrelated CUDA call, so empty buffers are skipped instead.
 *
 * @param managed_ptr Managed memory pointer to prefetch.
 * @param byte_count Number of bytes to prefetch.
 * @param device Destination device ordinal.
 */
void prefetch_managed_buffer(const void* const managed_ptr, const std::size_t byte_count,
                             const int32_t device)
{
    if (byte_count == 0U)
    {
        return;
    }
    cudaMemPrefetchAsync(managed_ptr, byte_count, device, nullptr);
}

}  // namespace

// ── GpuKavoshContext device method implementations ────────────────────────────

/**
 * @brief Binary-search the forward CSR adjacency to test edge existence.
 * @param src  Source vertex.
 * @param dest Destination vertex.
 * @return True iff (src, dest) is a forward edge in the graph.
 */
__device__ bool GpuKavoshContext::has_fwd_edge(const uint32_t src,
                                               const uint32_t dest) const noexcept
{
    const uint32_t first = m_graph.d_fwd_offsets[src];
    const uint32_t last = m_graph.d_fwd_offsets[src + 1U];
    uint32_t low = first;
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

/**
 * @brief Hash a pre-computed motif id and atomically record it in the cuco maps.
 * @param ctx      GPU run context owning the cuco map refs and overflow flag.
 * @param motif_id Canonical 128-bit motif identifier.
 */
__device__ void MotifPreprocessor::gpu_add_motif_to_count(GpuKavoshContext& ctx,
                                                          const UInt128 motif_id) noexcept
{
    atomic_insert_uint128_count(ctx.m_high_ref, ctx.m_low_ref, ctx.m_count_ref, ctx.m_overflow_flag,
                                motif_id);
}

// ── GPU first-layer driver functions ─────────────────────────────────────────

/**
 * @brief GPU driver for (1,1,1) groups.
 * @param ctx             GPU BFS run context for the current root.
 * @param thread_y_offset Y-dimension thread offset for striding over n1.
 * @param stride_y        Total y-dimension stride across all threads.
 */
__device__ void MotifPreprocessor::emit_depth_1_1_1_groups_gpu(GpuKavoshContext& ctx,
                                                               const uint32_t thread_y_offset,
                                                               const uint32_t stride_y)
{
    const GpuNeighbourRange depth_one = ctx.get_neighbour_range(ctx.m_root);
    ctx.mark_neighbours(depth_one, static_cast<uint32_t>(BFS_DEPTH_ONE_OFFSET));

    for (const uint32_t* n1_ptr = depth_one.m_begin + thread_y_offset; n1_ptr < depth_one.m_end;
         n1_ptr += stride_y)
    {
        if (ctx.m_order_index[*n1_ptr] < ctx.m_order_index[ctx.m_root])
        {
            continue;
        }
        emit_depth_1_1_1_groups_first_vertex_chosen(ctx, depth_one, n1_ptr, false);
    }
    if (ctx.m_graph.is_directed())
    {
        for (const uint32_t* n1_ptr = depth_one.m_rev_begin + thread_y_offset;
             n1_ptr < depth_one.m_rev_end; n1_ptr += stride_y)
        {
            if (ctx.m_order_index[*n1_ptr] < ctx.m_order_index[ctx.m_root] ||
                ctx.has_fwd_edge(ctx.m_root, *n1_ptr))
            {
                continue;
            }
            emit_depth_1_1_1_groups_first_vertex_chosen(ctx, depth_one, n1_ptr, true);
        }
    }
}

/**
 * @brief GPU driver for (1,1,2) and (1,2,2) groups.
 * @param ctx             GPU BFS run context.
 * @param thread_y_offset Y-dimension thread offset.
 * @param stride_y        Total y-dimension stride.
 */
__device__ void MotifPreprocessor::emit_depth_1_1_2_and_1_2_2_groups_gpu(
    GpuKavoshContext& ctx, const uint32_t thread_y_offset, const uint32_t stride_y)
{
    const GpuNeighbourRange depth_one = ctx.get_neighbour_range(ctx.m_root);

    for (const uint32_t* n1_ptr = depth_one.m_begin + thread_y_offset; n1_ptr < depth_one.m_end;
         n1_ptr += stride_y)
    {
        const uint32_t n1 = *n1_ptr;
        if (ctx.m_order_index[n1] < ctx.m_order_index[ctx.m_root])
        {
            continue;
        }
        const GpuNeighbourRange depth_two = ctx.get_neighbour_range(n1);
        ctx.mark_neighbours(depth_two, static_cast<uint32_t>(BFS_DEPTH_TWO_OFFSET));
        emit_depth_1_1_2_for_first_vertex(ctx, n1_ptr, depth_one, depth_two);
        emit_depth_1_2_2_for_first_vertex(ctx, n1_ptr, depth_two);
    }
    if (ctx.m_graph.is_directed())
    {
        for (const uint32_t* n1_ptr = depth_one.m_rev_begin + thread_y_offset;
             n1_ptr < depth_one.m_rev_end; n1_ptr += stride_y)
        {
            const uint32_t n1 = *n1_ptr;
            if (ctx.m_order_index[n1] < ctx.m_order_index[ctx.m_root] ||
                ctx.has_fwd_edge(ctx.m_root, n1))
            {
                continue;
            }
            const GpuNeighbourRange depth_two = ctx.get_neighbour_range(n1);
            ctx.mark_neighbours(depth_two, static_cast<uint32_t>(BFS_DEPTH_TWO_OFFSET));
            emit_depth_1_1_2_for_first_vertex(ctx, n1_ptr, depth_one, depth_two);
            emit_depth_1_2_2_for_first_vertex(ctx, n1_ptr, depth_two);
        }
    }
}

/**
 * @brief GPU driver for (1,2,3) groups.
 * @param ctx             GPU BFS run context.
 * @param thread_y_offset Y-dimension thread offset.
 * @param stride_y        Total y-dimension stride.
 */
__device__ void MotifPreprocessor::emit_depth_1_2_3_groups_gpu(GpuKavoshContext& ctx,
                                                               const uint32_t thread_y_offset,
                                                               const uint32_t stride_y)
{
    const GpuNeighbourRange depth_one = ctx.get_neighbour_range(ctx.m_root);

    for (const uint32_t* n1_ptr = depth_one.m_begin + thread_y_offset; n1_ptr < depth_one.m_end;
         n1_ptr += stride_y)
    {
        const uint32_t n1 = *n1_ptr;
        if (ctx.m_order_index[n1] < ctx.m_order_index[ctx.m_root])
        {
            continue;
        }
        emit_depth_1_2_3_for_first_vertex(ctx, n1, ctx.get_neighbour_range(n1));
    }
    if (ctx.m_graph.is_directed())
    {
        for (const uint32_t* n1_ptr = depth_one.m_rev_begin + thread_y_offset;
             n1_ptr < depth_one.m_rev_end; n1_ptr += stride_y)
        {
            const uint32_t n1 = *n1_ptr;
            if (ctx.m_order_index[n1] < ctx.m_order_index[ctx.m_root] ||
                ctx.has_fwd_edge(ctx.m_root, n1))
            {
                continue;
            }
            emit_depth_1_2_3_for_first_vertex(ctx, n1, ctx.get_neighbour_range(n1));
        }
    }
}

// ── Kernel ────────────────────────────────────────────────────────────────────

/**
 * @brief 2-D Kavosh motif kernel.
 *
 * x-threads cover roots; y-threads stripe depth-1 neighbours.
 *
 * @param graph          Graph arrays in managed memory.
 * @param count_ref      Device map ref for atomic count updates (hash → count).
 * @param high_ref       Device map ref for high-half UInt128 storage (hash → high).
 * @param low_ref        Device map ref for low-half UInt128 storage (hash → low).
 * @param overflow_flag  Managed-memory flag set to 1 if a map fills up.
 * @param canonical      Flat canonical array built from UNDIRECTED/DIRECTED maps.
 * @param canonical_size Number of entries in @p canonical.
 * @param sorted_nodes   Vertices in degree-sorted order, managed memory.
 * @param order_index    Position of each vertex in degree-sorted order, managed memory.
 */
// NOLINTNEXTLINE(readability-function-size)
__global__ void motif4_kernel(const DeviceGraph graph, CucoMotifMapRef count_ref,
                              CucoAuxMapRef high_ref, CucoAuxMapRef low_ref,
                              uint32_t* const overflow_flag, const MotifCanonical* const canonical,
                              const uint32_t canonical_size, const uint32_t* const sorted_nodes,
                              const uint32_t* const order_index)
{
    const uint32_t thread_x = blockIdx.x * blockDim.x + threadIdx.x;
    const uint32_t thread_y = blockIdx.y * blockDim.y + threadIdx.y;
    const uint32_t stride_x = blockDim.x * gridDim.x;
    const uint32_t stride_y = blockDim.y * gridDim.y;

    for (uint32_t idx = thread_x; idx < graph.num_nodes; idx += stride_x)
    {
        const uint32_t root = sorted_nodes[idx];
        const int64_t run_id = static_cast<int64_t>(static_cast<uint64_t>(root)
                                                    << GpuKavoshContext::BFS_VERTEX_RUN_SHIFT);

        GpuKavoshContext ctx{run_id,
                             root,
                             canonical,
                             canonical_size,
                             graph,
                             count_ref,
                             high_ref,
                             low_ref,
                             overflow_flag,
                             order_index,
                             &MotifPreprocessor::gpu_add_motif_to_count};

        MotifPreprocessor::emit_depth_1_1_1_groups_gpu(ctx, thread_y, stride_y);
        MotifPreprocessor::emit_depth_1_1_2_and_1_2_2_groups_gpu(ctx, thread_y, stride_y);
        MotifPreprocessor::emit_depth_1_2_3_groups_gpu(ctx, thread_y, stride_y);
    }
}

// ── calculate_gpu ─────────────────────────────────────────────────────────────

// NOLINTNEXTLINE(readability-function-size)
EnumerationResult MotifPreprocessor::calculate_gpu()
{
    constexpr uint32_t MIN_VERTICES_FOR_MOTIF = 4U;
    if (m_graph.vertex_count() < MIN_VERTICES_FOR_MOTIF)
    {
        return {};
    }

    DeviceGraph device_graph = DeviceGraphBuilder::build(m_graph);

    uint32_t* d_order_index = nullptr;
    uint32_t* d_sorted_nodes = nullptr;
    const std::size_t vertex_bytes =
        static_cast<std::size_t>(device_graph.num_nodes) * sizeof(uint32_t);
    cudaMallocManaged(&d_order_index, vertex_bytes);
    cudaMallocManaged(&d_sorted_nodes, vertex_bytes);
    std::memcpy(d_order_index, m_order_index.data(), vertex_bytes);
    std::memcpy(d_sorted_nodes, m_node_order.data(), vertex_bytes);

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
    const std::size_t canonical_bytes = canonical_size * sizeof(MotifCanonical);
    cudaMallocManaged(&device_canonical, canonical_bytes);
    // cudaMallocManaged does not call constructors — zero-init so unoccupied slots
    // have m_permutation_count == 0, which calculate_motif_number_from_arrays checks.
    std::memset(device_canonical, 0, canonical_bytes);
    for (const auto& [descriptor, entry] : canonical_map)
    {
        device_canonical[descriptor] = entry;
    }

    // Y-grid must cover max degree, not all N nodes — otherwise (N-degree)/N threads are idle.
    uint32_t max_fwd_degree = 0U;
    for (uint32_t v = 0U; v < device_graph.num_nodes; ++v)
    {
        const uint32_t deg = device_graph.d_fwd_offsets[v + 1U] - device_graph.d_fwd_offsets[v];
        if (deg > max_fwd_degree)
        {
            max_fwd_degree = deg;
        }
    }

    // Prefetch all managed arrays to device before kernel launch.
    int cuda_device = -1;
    cudaGetDevice(&cuda_device);
    prefetch_managed_buffer(d_order_index, vertex_bytes, cuda_device);
    prefetch_managed_buffer(d_sorted_nodes, vertex_bytes, cuda_device);
    prefetch_managed_buffer(device_canonical, canonical_bytes, cuda_device);
    prefetch_managed_buffer(device_graph.d_fwd_offsets,
                            (device_graph.num_nodes + 1U) * sizeof(uint32_t), cuda_device);
    prefetch_managed_buffer(device_graph.d_fwd_neighbors,
                            device_graph.num_fwd_edges * sizeof(uint32_t), cuda_device);
    prefetch_managed_buffer(device_graph.d_colors, device_graph.num_nodes * sizeof(uint32_t),
                            cuda_device);
    if (device_graph.d_rev_offsets != nullptr)
    {
        prefetch_managed_buffer(device_graph.d_rev_offsets,
                                (device_graph.num_nodes + 1U) * sizeof(uint32_t), cuda_device);
        prefetch_managed_buffer(device_graph.d_rev_neighbors,
                                device_graph.num_rev_edges * sizeof(uint32_t), cuda_device);
    }

    const dim3 block_size(SgfConstants::GPU_KERNEL_BLOCK_DIM, SgfConstants::GPU_KERNEL_BLOCK_DIM);
    const dim3 num_blocks(
        ceil_div(device_graph.num_nodes, SgfConstants::GPU_KERNEL_BLOCK_DIM),
        std::max(1U, ceil_div(max_fwd_degree, SgfConstants::GPU_KERNEL_BLOCK_DIM)));

    const EnumerationResult result = run_gpu_enumeration_with_growth(
        [&](const CucoMotifMapRef count_ref, const CucoAuxMapRef high_ref,
            const CucoAuxMapRef low_ref, uint32_t* const overflow_flag)
        {
            motif4_kernel<<<num_blocks, block_size>>>(
                device_graph, count_ref, high_ref, low_ref, overflow_flag, device_canonical,
                canonical_size, d_sorted_nodes, d_order_index);
        },
        SgfConstants::GPU_MOTIF_MAP_INITIAL_CAPACITY);

    cudaFree(d_order_index);
    cudaFree(d_sorted_nodes);
    cudaFree(device_canonical);
    DeviceGraphBuilder::free_graph(device_graph);

    return result;
}

}  // namespace sgf

#endif  // __CUDACC__
