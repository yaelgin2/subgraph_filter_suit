#include "CudaMotifBackend.h"

#ifdef __CUDACC__

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
#include <limits>
#include <unordered_map>
#include <vector>

namespace sgf
{

namespace
{

/// Block dimension used for x and y axes in the 2D kernel launch.
constexpr uint32_t BLOCK_DIM = 16U;

/// Sentinel key for empty cuco map slots (max uint64_t, never a valid MurmurHash3 output).
constexpr uint64_t EMPTY_HASH_KEY = std::numeric_limits<uint64_t>::max();

/// Fixed capacity for all cuco motif maps (distinct canonical motif count upper bound).
constexpr std::size_t MOTIF_MAP_CAPACITY = 1'000'000UL;

/// Seed for the primary slot key: hash(motif, PRIMARY_HASH_SEED).
constexpr uint64_t PRIMARY_HASH_SEED = 0ULL;

/// Seed for the double-hash probe step: hash(motif, STEP_HASH_SEED) | 1.
constexpr uint64_t STEP_HASH_SEED = 1ULL;

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

/**
 * @brief Hash a pre-computed motif id and atomically record it in the cuco maps.
 *
 * Uses double hashing to resolve collisions when two distinct UInt128 values
 * produce the same 64-bit key.  For each probe attempt the sequence is:
 *   key_0 = hash(motif, PRIMARY_HASH_SEED)
 *   step  = hash(motif, STEP_HASH_SEED) | 1   (odd → coprime with any power-of-2 capacity)
 *   key_n = key_0 + n * step
 *
 * At each candidate key, insert_and_find atomically claims the slot (if empty)
 * or returns the already-stored value (if occupied).  We only proceed when both
 * high_ref and low_ref at that key contain our own UInt128 halves, guaranteeing
 * no mix-up with a colliding UInt128.
 *
 * @param ctx      GPU run context owning the cuco map refs.
 * @param motif_id Canonical 128-bit motif identifier.
 */
__device__ void MotifPreprocessor::gpu_add_motif_to_count(GpuKavoshContext& ctx,
                                                           const UInt128 motif_id) noexcept
{
    auto tile = cooperative_groups::tiled_partition<1>(
        cooperative_groups::this_thread_block());

    uint64_t key = UInt128MurmurHash::hash(motif_id, PRIMARY_HASH_SEED);
    if (key == EMPTY_HASH_KEY)
    {
        key ^= 1ULL;
    }
    const uint64_t step = UInt128MurmurHash::hash(motif_id, STEP_HASH_SEED) | 1ULL;

    for (uint32_t probe = 0U; probe < static_cast<uint32_t>(MOTIF_MAP_CAPACITY); ++probe)
    {
        const auto [high_it, high_inserted] =
            ctx.m_high_ref.insert_and_find(tile, cuco::pair<uint64_t, uint64_t>{key, motif_id.m_high});
        const auto [low_it, low_inserted] =
            ctx.m_low_ref.insert_and_find(tile, cuco::pair<uint64_t, uint64_t>{key, motif_id.m_low});

        const bool matches =
            (high_it->second == motif_id.m_high) && (low_it->second == motif_id.m_low);

        if (matches)
        {
            ctx.m_count_ref.insert_or_apply(
                tile,
                cuco::pair<uint64_t, uint32_t>{key, 1U},
                [] __device__(
                    cuda::atomic_ref<uint32_t, cuda::thread_scope_device> existing,
                    uint32_t increment) noexcept
                {
                    existing.fetch_add(increment, cuda::memory_order_relaxed);
                });
            return;
        }

        key += step;
        if (key == EMPTY_HASH_KEY)
        {
            key ^= 1ULL;
        }
    }
    // Map is full — silently discard this motif to avoid infinite looping.
}

// ── GPU first-layer driver functions ─────────────────────────────────────────

/**
 * @brief GPU driver for (1,1,1) groups.
 * @param ctx             GPU BFS run context for the current root.
 * @param thread_y_offset Y-dimension thread offset for striding over n1.
 * @param stride_y        Total y-dimension stride across all threads.
 */
__device__ void MotifPreprocessor::emit_depth_1_1_1_groups_gpu(
    GpuKavoshContext& ctx,
    const uint32_t thread_y_offset,
    const uint32_t stride_y)
{
    const GpuNeighbourRange depth_one = ctx.get_neighbour_range(ctx.m_root);
    ctx.mark_neighbours(depth_one, static_cast<uint32_t>(BFS_DEPTH_ONE_OFFSET));

    for (const uint32_t* n1_ptr = depth_one.m_begin + thread_y_offset;
         n1_ptr < depth_one.m_end; n1_ptr += stride_y)
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
    GpuKavoshContext& ctx,
    const uint32_t thread_y_offset,
    const uint32_t stride_y)
{
    const GpuNeighbourRange depth_one = ctx.get_neighbour_range(ctx.m_root);

    for (const uint32_t* n1_ptr = depth_one.m_begin + thread_y_offset;
         n1_ptr < depth_one.m_end; n1_ptr += stride_y)
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
__device__ void MotifPreprocessor::emit_depth_1_2_3_groups_gpu(
    GpuKavoshContext& ctx,
    const uint32_t thread_y_offset,
    const uint32_t stride_y)
{
    const GpuNeighbourRange depth_one = ctx.get_neighbour_range(ctx.m_root);

    for (const uint32_t* n1_ptr = depth_one.m_begin + thread_y_offset;
         n1_ptr < depth_one.m_end; n1_ptr += stride_y)
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
 * @param canonical      Flat canonical array built from UNDIRECTED/DIRECTED maps.
 * @param canonical_size Number of entries in @p canonical.
 * @param sorted_nodes   Vertices in degree-sorted order, managed memory.
 * @param order_index    Position of each vertex in degree-sorted order, managed memory.
 */
__global__ void motif4_kernel(const DeviceGraph graph,
                               CucoMotifMapRef count_ref,
                               CucoAuxMapRef high_ref,
                               CucoAuxMapRef low_ref,
                               const MotifCanonical* const canonical,
                               const uint32_t canonical_size,
                               const uint32_t* const sorted_nodes,
                               const uint32_t* const order_index)
{
    const uint32_t thread_x = blockIdx.x * blockDim.x + threadIdx.x;
    const uint32_t thread_y = blockIdx.y * blockDim.y + threadIdx.y;
    const uint32_t stride_x = blockDim.x * gridDim.x;
    const uint32_t stride_y = blockDim.y * gridDim.y;

    for (uint32_t idx = thread_x; idx < graph.num_nodes; idx += stride_x)
    {
        const uint32_t root = sorted_nodes[idx];
        const int64_t run_id =
            static_cast<int64_t>(static_cast<uint64_t>(root)
                                  << GpuKavoshContext::BFS_VERTEX_RUN_SHIFT);

        GpuKavoshContext ctx{run_id, root, canonical, canonical_size,
                             graph, count_ref, high_ref, low_ref, order_index,
                             &MotifPreprocessor::gpu_add_motif_to_count};

        MotifPreprocessor::emit_depth_1_1_1_groups_gpu(ctx, thread_y, stride_y);
        MotifPreprocessor::emit_depth_1_1_2_and_1_2_2_groups_gpu(ctx, thread_y, stride_y);
        MotifPreprocessor::emit_depth_1_2_3_groups_gpu(ctx, thread_y, stride_y);
    }
}

// ── calculate_gpu ─────────────────────────────────────────────────────────────

EnumerationResult MotifPreprocessor::calculate_gpu()
{
    constexpr uint32_t MIN_VERTICES_FOR_MOTIF = 4U;
    if (m_graph.vertex_count() < MIN_VERTICES_FOR_MOTIF)
    {
        return {};
    }

    DeviceGraph device_graph = DeviceGraphBuilder::build(m_graph);

    uint32_t* d_order_index  = nullptr;
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
    cudaMemPrefetchAsync(d_order_index,                vertex_bytes,                              cuda_device, nullptr);
    cudaMemPrefetchAsync(d_sorted_nodes,               vertex_bytes,                              cuda_device, nullptr);
    cudaMemPrefetchAsync(device_canonical,             canonical_bytes,                            cuda_device, nullptr);
    cudaMemPrefetchAsync(device_graph.d_fwd_offsets,   (device_graph.num_nodes + 1U) * sizeof(uint32_t), cuda_device, nullptr);
    cudaMemPrefetchAsync(device_graph.d_fwd_neighbors, device_graph.num_fwd_edges * sizeof(uint32_t),    cuda_device, nullptr);
    cudaMemPrefetchAsync(device_graph.d_colors,        device_graph.num_nodes * sizeof(uint32_t),        cuda_device, nullptr);
    if (device_graph.d_rev_offsets != nullptr)
    {
        cudaMemPrefetchAsync(device_graph.d_rev_offsets,   (device_graph.num_nodes + 1U) * sizeof(uint32_t), cuda_device, nullptr);
        cudaMemPrefetchAsync(device_graph.d_rev_neighbors, device_graph.num_rev_edges * sizeof(uint32_t),    cuda_device, nullptr);
    }

    CucoMotifMap count_map = make_cuco_motif_map(device_graph.num_nodes);
    CucoAuxMap high_map    = make_cuco_aux_map(device_graph.num_nodes);
    CucoAuxMap low_map     = make_cuco_aux_map(device_graph.num_nodes);

    CucoMotifMapRef count_ref = count_map.ref(cuco::op::insert_or_apply_tag{});
    CucoAuxMapRef high_ref    = high_map.ref(cuco::op::insert_and_find_tag{});
    CucoAuxMapRef low_ref     = low_map.ref(cuco::op::insert_and_find_tag{});

    const dim3 block_size(BLOCK_DIM, BLOCK_DIM);
    const dim3 num_blocks(ceil_div(device_graph.num_nodes, BLOCK_DIM),
                           std::max(1U, ceil_div(max_fwd_degree, BLOCK_DIM)));

    motif4_kernel<<<num_blocks, block_size>>>(device_graph, count_ref, high_ref, low_ref,
                                               device_canonical, canonical_size,
                                               d_sorted_nodes, d_order_index);
    cudaDeviceSynchronize();

    cudaFree(d_order_index);
    cudaFree(d_sorted_nodes);
    cudaFree(device_canonical);
    DeviceGraphBuilder::free_graph(device_graph);

    return cuco_maps_to_enumeration_result(count_map, high_map, low_map);
}

// ── Helper implementations ────────────────────────────────────────────────────

CucoMotifMap make_cuco_motif_map(const uint32_t /*num_nodes*/)
{
    return CucoMotifMap{cuco::extent<std::size_t>(MOTIF_MAP_CAPACITY),
                         cuco::empty_key{EMPTY_HASH_KEY},
                         cuco::empty_value{uint32_t{0U}},
                         thrust::equal_to<uint64_t>{},
                         cuco::linear_probing<1U, UInt64Hash>{}};
}

CucoAuxMap make_cuco_aux_map(const uint32_t /*num_nodes*/)
{
    // empty_value must differ from every valid stored motif half.
    // UINT64_MAX is safe: m_high encodes (min_motif_num<<32)|color_bits; with
    // at most ~100 distinct 4-node motifs, min_motif_num << 32 << UINT64_MAX.
    // Using 0 as sentinel deadlocked via maybe_wait_for_payload when m_low == 0
    // (all-zero vertex colors), so UINT64_MAX is used instead.
    return CucoAuxMap{cuco::extent<std::size_t>(MOTIF_MAP_CAPACITY),
                       cuco::empty_key{EMPTY_HASH_KEY},
                       cuco::empty_value{~uint64_t{0U}},
                       thrust::equal_to<uint64_t>{},
                       cuco::linear_probing<1U, UInt64Hash>{}};
}

// NOLINTNEXTLINE(readability-function-size)
EnumerationResult cuco_maps_to_enumeration_result(const CucoMotifMap& count_map,
                                                   const CucoAuxMap& high_map,
                                                   const CucoAuxMap& low_map)
{
    const std::size_t entry_count = static_cast<std::size_t>(count_map.size());
    if (entry_count == 0)
    {
        return {};
    }

    // Allocate managed memory for retrieve_all/find output — accessible from both GPU and CPU.
    uint64_t* count_keys = nullptr;
    uint32_t* count_vals = nullptr;
    uint64_t* high_vals  = nullptr;
    uint64_t* low_vals   = nullptr;
    cudaMallocManaged(&count_keys, entry_count * sizeof(uint64_t));
    cudaMallocManaged(&count_vals, entry_count * sizeof(uint32_t));
    cudaMallocManaged(&high_vals,  entry_count * sizeof(uint64_t));
    cudaMallocManaged(&low_vals,   entry_count * sizeof(uint64_t));

    // Prefetch to device so retrieve_all and find kernels can write without page faults.
    int cuda_device = -1;
    cudaGetDevice(&cuda_device);
    cudaMemPrefetchAsync(count_keys, entry_count * sizeof(uint64_t), cuda_device, nullptr);
    cudaMemPrefetchAsync(count_vals, entry_count * sizeof(uint32_t), cuda_device, nullptr);
    cudaMemPrefetchAsync(high_vals,  entry_count * sizeof(uint64_t), cuda_device, nullptr);
    cudaMemPrefetchAsync(low_vals,   entry_count * sizeof(uint64_t), cuda_device, nullptr);

    // Dump count_map to managed memory, then look up high/low halves using those same keys.
    count_map.retrieve_all(count_keys, count_vals);
    high_map.find(count_keys, count_keys + entry_count, high_vals);
    low_map.find(count_keys, count_keys + entry_count, low_vals);
    cudaDeviceSynchronize();

    // Prefetch back to CPU for host-side result assembly.
    cudaMemPrefetchAsync(count_keys, entry_count * sizeof(uint64_t), cudaCpuDeviceId, nullptr);
    cudaMemPrefetchAsync(count_vals, entry_count * sizeof(uint32_t), cudaCpuDeviceId, nullptr);
    cudaMemPrefetchAsync(high_vals,  entry_count * sizeof(uint64_t), cudaCpuDeviceId, nullptr);
    cudaMemPrefetchAsync(low_vals,   entry_count * sizeof(uint64_t), cudaCpuDeviceId, nullptr);
    cudaDeviceSynchronize();

    constexpr uint64_t AUX_NOT_FOUND = ~uint64_t{0U};
    EnumerationResult result;
    for (std::size_t idx = std::size_t{0}; idx < entry_count; ++idx)
    {
        if (high_vals[idx] == AUX_NOT_FOUND || low_vals[idx] == AUX_NOT_FOUND)
        {
            continue;
        }
        result[UInt128{high_vals[idx], low_vals[idx]}] += count_vals[idx];
    }

    cudaFree(count_keys);
    cudaFree(count_vals);
    cudaFree(high_vals);
    cudaFree(low_vals);

    return result;
}

}  // namespace sgf

#endif  // __CUDACC__
