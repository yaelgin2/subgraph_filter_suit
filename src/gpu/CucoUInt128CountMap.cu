#include "CucoUInt128CountMap.h"

#ifdef __CUDACC__

#include "Constants.h"
#include "Int128.h"

#include <cooperative_groups.h>
#include <cstdint>
#include <cuda_runtime.h>

namespace sgf
{

CucoMotifMap make_cuco_motif_map(const uint64_t capacity)
{
    return CucoMotifMap{cuco::extent<std::size_t>(static_cast<std::size_t>(capacity)),
                        cuco::empty_key{SgfConstants::GPU_EMPTY_HASH_KEY},
                        cuco::empty_value{uint32_t{0U}}, thrust::equal_to<uint64_t>{},
                        cuco::linear_probing<1U, UInt64Hash>{}};
}

CucoAuxMap make_cuco_aux_map(const uint64_t capacity)
{
    // empty_value must differ from every valid stored half-value.
    // Using 0 as sentinel deadlocked via maybe_wait_for_payload when a stored
    // low half is 0 (all-zero vertex colors), so UINT64_MAX is used instead.
    return CucoAuxMap{cuco::extent<std::size_t>(static_cast<std::size_t>(capacity)),
                      cuco::empty_key{SgfConstants::GPU_EMPTY_HASH_KEY},
                      cuco::empty_value{~uint64_t{0U}}, thrust::equal_to<uint64_t>{},
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
    uint64_t* high_vals = nullptr;
    uint64_t* low_vals = nullptr;
    cudaMallocManaged(&count_keys, entry_count * sizeof(uint64_t));
    cudaMallocManaged(&count_vals, entry_count * sizeof(uint32_t));
    cudaMallocManaged(&high_vals, entry_count * sizeof(uint64_t));
    cudaMallocManaged(&low_vals, entry_count * sizeof(uint64_t));

    // Prefetch to device so retrieve_all and find kernels can write without page faults.
    int cuda_device = -1;
    cudaGetDevice(&cuda_device);
    cudaMemPrefetchAsync(count_keys, entry_count * sizeof(uint64_t), cuda_device, nullptr);
    cudaMemPrefetchAsync(count_vals, entry_count * sizeof(uint32_t), cuda_device, nullptr);
    cudaMemPrefetchAsync(high_vals, entry_count * sizeof(uint64_t), cuda_device, nullptr);
    cudaMemPrefetchAsync(low_vals, entry_count * sizeof(uint64_t), cuda_device, nullptr);

    // Dump count_map to managed memory, then look up high/low halves using those same keys.
    count_map.retrieve_all(count_keys, count_vals);
    high_map.find(count_keys, count_keys + entry_count, high_vals);
    low_map.find(count_keys, count_keys + entry_count, low_vals);
    cudaDeviceSynchronize();

    // Prefetch back to CPU for host-side result assembly.
    cudaMemPrefetchAsync(count_keys, entry_count * sizeof(uint64_t), cudaCpuDeviceId, nullptr);
    cudaMemPrefetchAsync(count_vals, entry_count * sizeof(uint32_t), cudaCpuDeviceId, nullptr);
    cudaMemPrefetchAsync(high_vals, entry_count * sizeof(uint64_t), cudaCpuDeviceId, nullptr);
    cudaMemPrefetchAsync(low_vals, entry_count * sizeof(uint64_t), cudaCpuDeviceId, nullptr);
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

__device__ bool atomic_insert_uint128_count(CucoAuxMapRef high_ref, CucoAuxMapRef low_ref,
                                            CucoMotifMapRef count_ref,
                                            uint32_t* const overflow_flag,
                                            const UInt128 value) noexcept
{
    auto tile = cooperative_groups::tiled_partition<1>(cooperative_groups::this_thread_block());

    uint64_t key = UInt128MurmurHash::hash(value, SgfConstants::GPU_HASH_PRIMARY_SEED);
    if (key == SgfConstants::GPU_EMPTY_HASH_KEY)
    {
        key ^= 1ULL;
    }
    const uint64_t step = UInt128MurmurHash::hash(value, SgfConstants::GPU_HASH_STEP_SEED) | 1ULL;
    const uint64_t probe_bound = static_cast<uint64_t>(count_ref.capacity());

    for (uint64_t probe = 0ULL; probe < probe_bound; ++probe)
    {
        const auto [high_it, high_inserted] =
            high_ref.insert_and_find(tile, cuco::pair<uint64_t, uint64_t>{key, value.m_high});
        const auto [low_it, low_inserted] =
            low_ref.insert_and_find(tile, cuco::pair<uint64_t, uint64_t>{key, value.m_low});

        const bool matches = (high_it->second == value.m_high) && (low_it->second == value.m_low);

        if (matches)
        {
            count_ref.insert_or_apply(
                tile, cuco::pair<uint64_t, uint32_t>{key, 1U},
                [] __device__(cuda::atomic_ref<uint32_t, cuda::thread_scope_device> existing,
                              uint32_t increment) noexcept
                {
                    existing.fetch_add(increment, cuda::memory_order_relaxed);
                });
            return true;
        }

        key += step;
        if (key == SgfConstants::GPU_EMPTY_HASH_KEY)
        {
            key ^= 1ULL;
        }
    }
    // Map is full — report overflow so the host can retry with a larger capacity
    // instead of silently losing this value's count.
    const cuda::atomic_ref<uint32_t, cuda::thread_scope_device> overflow_ref(*overflow_flag);
    overflow_ref.store(1U, cuda::memory_order_relaxed);
    return false;
}

}  // namespace sgf

#endif  // __CUDACC__
