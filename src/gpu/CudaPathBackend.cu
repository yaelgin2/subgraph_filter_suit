#include "CudaPathBackend.h"

#ifdef __CUDACC__

// Include PathProcessor AFTER CudaPathBackend.h so GpuPathContext is already
// defined when the SGF_HD templates in PathProcessor.h are instantiated here.
#include "Constants.h"
#include "CucoUInt128CountMap.h"
#include "DeviceGraph.h"
#include "Int128.h"
#include "PathProcessor.h"

#include <algorithm>
#include <cstdint>

namespace sgf
{

/**
 * @brief Hash a pre-computed canonical path id and atomically record it in the cuco maps.
 * @param ctx      GPU run context owning the cuco map refs and overflow flag.
 * @param motif_id Canonical 128-bit path identifier.
 */
__device__ void PathProcessor::gpu_add_path_to_count(GpuPathContext& ctx,
                                                     const UInt128 motif_id) noexcept
{
    atomic_insert_uint128_count(ctx.m_high_ref, ctx.m_low_ref, ctx.m_count_ref, ctx.m_overflow_flag,
                                motif_id);
}

// ── Kernel ────────────────────────────────────────────────────────────────────

/**
 * @brief 2-D path enumeration kernel.
 *
 * x-threads cover roots; y-threads stripe depth-1 neighbours (the outer loop
 * inside stream_groups_to_counter_for_vertex's out/in neighbour drivers).
 *
 * @param graph          Graph arrays in managed memory.
 * @param count_ref      Device map ref for atomic count updates (hash → count).
 * @param high_ref       Device map ref for high-half UInt128 storage (hash → high).
 * @param low_ref        Device map ref for low-half UInt128 storage (hash → low).
 * @param overflow_flag  Managed-memory flag set to 1 if a map fills up.
 */
__global__ void path_kernel(const DeviceGraph graph, CucoMotifMapRef count_ref,
                            CucoAuxMapRef high_ref, CucoAuxMapRef low_ref,
                            uint32_t* const overflow_flag)
{
    const uint32_t thread_x = blockIdx.x * blockDim.x + threadIdx.x;
    const uint32_t thread_y = blockIdx.y * blockDim.y + threadIdx.y;
    const uint32_t stride_x = blockDim.x * gridDim.x;
    const uint32_t stride_y = blockDim.y * gridDim.y;

    for (uint32_t root = thread_x; root < graph.num_nodes; root += stride_x)
    {
        GpuPathContext ctx{root,
                           graph,
                           count_ref,
                           high_ref,
                           low_ref,
                           overflow_flag,
                           &PathProcessor::gpu_add_path_to_count};

        PathProcessor::stream_groups_to_counter_for_vertex<GpuPathContext, GpuNeighbourRange,
                                                           const uint32_t*>(ctx, thread_y,
                                                                            stride_y);
    }
}

// ── calculate_gpu ─────────────────────────────────────────────────────────────

EnumerationResult PathProcessor::calculate_gpu()
{
    if (m_graph.vertex_count() < PATH_VERTEX_COUNT)
    {
        return {};
    }

    DeviceGraph device_graph = DeviceGraphBuilder::build(m_graph);

    // Y-grid must cover max degree, not all N nodes — otherwise (N-degree)/N threads are idle.
    uint32_t max_fwd_degree = 0U;
    for (uint32_t vertex = 0U; vertex < device_graph.num_nodes; ++vertex)
    {
        const uint32_t degree =
            device_graph.d_fwd_offsets[vertex + 1U] - device_graph.d_fwd_offsets[vertex];
        if (degree > max_fwd_degree)
        {
            max_fwd_degree = degree;
        }
    }

    // Prefetch all managed graph arrays to device before kernel launch.
    int cuda_device = -1;
    cudaGetDevice(&cuda_device);
    cudaMemPrefetchAsync(device_graph.d_fwd_offsets,
                         (device_graph.num_nodes + 1U) * sizeof(uint32_t), cuda_device, nullptr);
    cudaMemPrefetchAsync(device_graph.d_fwd_neighbors,
                         device_graph.num_fwd_edges * sizeof(uint32_t), cuda_device, nullptr);
    cudaMemPrefetchAsync(device_graph.d_colors, device_graph.num_nodes * sizeof(uint32_t),
                         cuda_device, nullptr);
    if (device_graph.d_rev_offsets != nullptr)
    {
        cudaMemPrefetchAsync(device_graph.d_rev_offsets,
                             (device_graph.num_nodes + 1U) * sizeof(uint32_t), cuda_device,
                             nullptr);
        cudaMemPrefetchAsync(device_graph.d_rev_neighbors,
                             device_graph.num_rev_edges * sizeof(uint32_t), cuda_device, nullptr);
    }

    const dim3 block_size(SgfConstants::GPU_KERNEL_BLOCK_DIM, SgfConstants::GPU_KERNEL_BLOCK_DIM);
    const dim3 num_blocks(
        ceil_div(device_graph.num_nodes, SgfConstants::GPU_KERNEL_BLOCK_DIM),
        std::max(1U, ceil_div(max_fwd_degree, SgfConstants::GPU_KERNEL_BLOCK_DIM)));

    const EnumerationResult result = run_gpu_enumeration_with_growth(
        [&](const CucoMotifMapRef count_ref, const CucoAuxMapRef high_ref,
            const CucoAuxMapRef low_ref, uint32_t* const overflow_flag)
        {
            path_kernel<<<num_blocks, block_size>>>(device_graph, count_ref, high_ref, low_ref,
                                                    overflow_flag);
        },
        SgfConstants::GPU_PATH_MAP_INITIAL_CAPACITY);

    DeviceGraphBuilder::free_graph(device_graph);

    return result;
}

}  // namespace sgf

#endif  // __CUDACC__
