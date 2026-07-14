#pragma once

#ifdef __CUDACC__

#include "Constants.h"
#include "EnumerationOverflowException.h"
#include "IGraphPreprocessor.h"
#include "Int128.h"

#include <cstdint>
#include <cuco/static_map.cuh>
#include <cuda/std/functional>
#include <cuda_runtime.h>
#include <thrust/functional.h>
#include <utility>

namespace sgf
{

/**
 * @brief Integer ceiling-division.
 * @param numerator   Dividend.
 * @param denominator Divisor (non-zero).
 * @return ceil(numerator / denominator).
 */
__host__ __device__ inline uint32_t ceil_div(const uint32_t numerator,
                                             const uint32_t denominator) noexcept
{
    return (numerator + denominator - 1U) / denominator;
}

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
     * @param key  128-bit value to hash.
     * @param seed 0 for the primary slot key; 1 for the double-hash probe step.
     * @return 64-bit hash value.
     */
    __host__ __device__ static uint64_t hash(const UInt128& key, const uint64_t seed) noexcept
    {
        const cuco::detail::MurmurHash3_x64_128<uint64_t> hasher{seed};
        return hasher(key.m_high)[0] ^ hasher(key.m_low)[0];
    }

    __host__ __device__ uint64_t operator()(const UInt128& key) const noexcept
    {
        return hash(key, 0ULL);
    }
};

/** @brief MurmurHash3 hasher for uint64_t slot keys used by cuco linear probing. */
struct UInt64Hash
{
    __host__ __device__ uint32_t operator()(const uint64_t key) const noexcept
    {
        const cuco::detail::MurmurHash3_x64_128<uint64_t> hasher{0ULL};
        return static_cast<uint32_t>(hasher(key)[0]);
    }
};

/** @brief Count map: MurmurHash3(UInt128) → occurrence count. */
using CucoMotifMap =
    cuco::static_map<uint64_t, uint32_t, cuco::extent<std::size_t>, cuda::thread_scope_device,
                     thrust::equal_to<uint64_t>, cuco::linear_probing<1U, UInt64Hash>>;

/** @brief Auxiliary map: MurmurHash3(UInt128) → one 64-bit half of the original key. */
using CucoAuxMap =
    cuco::static_map<uint64_t, uint64_t, cuco::extent<std::size_t>, cuda::thread_scope_device,
                     thrust::equal_to<uint64_t>, cuco::linear_probing<1U, UInt64Hash>>;

/** @brief Device-side ref type for count updates (insert_or_apply). */
using CucoMotifMapRef =
    decltype(std::declval<CucoMotifMap&>().ref(cuco::op::insert_or_apply_tag{}));

/** @brief Device-side ref type for auxiliary half-key storage (insert_and_find for collision
 * check). */
using CucoAuxMapRef = decltype(std::declval<CucoAuxMap&>().ref(cuco::op::insert_and_find_tag{}));

/**
 * @brief Allocate a cuco count map with the given slot capacity.
 * @param capacity Number of slots to allocate.
 */
CucoMotifMap make_cuco_motif_map(uint64_t capacity);

/**
 * @brief Allocate a cuco auxiliary half-key map with the given slot capacity.
 * @param capacity Number of slots to allocate.
 */
CucoAuxMap make_cuco_aux_map(uint64_t capacity);

/**
 * @brief Reconstruct EnumerationResult from the three device maps.
 *
 * Iterates the count map; for each (hash_key, count), looks up hash_key in
 * high_map and low_map to reconstruct the original UInt128 value.
 *
 * @param count_map Populated count map.
 * @param high_map  Map from hash key to high 64 bits of the UInt128 value.
 * @param low_map   Map from hash key to low 64 bits of the UInt128 value.
 */
EnumerationResult cuco_maps_to_enumeration_result(const CucoMotifMap& count_map,
                                                  const CucoAuxMap& high_map,
                                                  const CucoAuxMap& low_map);

/**
 * @brief Hash a pre-computed UInt128 value and atomically record it in the cuco maps.
 *
 * Uses double hashing to resolve collisions when two distinct UInt128 values
 * produce the same 64-bit key.  For each probe attempt the sequence is:
 *   key_0 = hash(value, GPU_HASH_PRIMARY_SEED)
 *   step  = hash(value, GPU_HASH_STEP_SEED) | 1   (odd → coprime with any power-of-2 capacity)
 *   key_n = key_0 + n * step
 *
 * At each candidate key, insert_and_find atomically claims the slot (if empty)
 * or returns the already-stored value (if occupied).  Only proceeds when both
 * high_ref and low_ref at that key contain the caller's own UInt128 halves,
 * guaranteeing no mix-up with a colliding UInt128.
 *
 * The probe bound is @p count_ref's own capacity() — no caller-supplied
 * capacity is needed. If every slot within that bound is occupied by a
 * different value, the insert is abandoned and @p overflow_flag is atomically
 * set to 1 so the host can detect the overflow after the kernel completes and
 * retry with a larger map, instead of silently losing the count.
 *
 * @param high_ref      High-half auxiliary map device ref.
 * @param low_ref       Low-half auxiliary map device ref.
 * @param count_ref     Count map device ref.
 * @param overflow_flag Managed-memory flag set to 1 if the map was full.
 * @param value         128-bit value to record.
 * @return True if the value was recorded; false if the map was full.
 */
__device__ bool atomic_insert_uint128_count(CucoAuxMapRef high_ref, CucoAuxMapRef low_ref,
                                            CucoMotifMapRef count_ref, uint32_t* overflow_flag,
                                            UInt128 value) noexcept;

/**
 * @brief Run a GPU enumeration kernel, growing the cuco map capacity on overflow.
 *
 * Allocates count/high/low cuco maps starting at
 * SgfConstants::GPU_UINT128_MAP_CAPACITY slots, invokes @p launch_kernel with
 * their device refs, the chosen capacity, and a managed overflow flag, then
 * synchronizes. If the kernel reported overflow (any thread's
 * atomic_insert_uint128_count call filled its map), the maps are freed and
 * capacity is multiplied by SgfConstants::GPU_MAP_GROWTH_FACTOR before
 * retrying, up to SgfConstants::GPU_MAP_GROWTH_MAX_ATTEMPTS times.
 *
 * @tparam KernelLauncher Callable with signature
 *         void(CucoMotifMapRef, CucoAuxMapRef, CucoAuxMapRef, uint32_t* overflow_flag)
 *         that launches the kernel and does not itself call cudaDeviceSynchronize().
 * @param launch_kernel Launches the enumeration kernel for one attempt.
 * @return Reconstructed EnumerationResult from the first successful attempt.
 * @throws EnumerationOverflowException if the map still overflows after the
 *         maximum number of growth attempts.
 */
template <typename KernelLauncher>
EnumerationResult run_gpu_enumeration_with_growth(KernelLauncher&& launch_kernel)
{
    uint64_t capacity = SgfConstants::GPU_UINT128_MAP_CAPACITY;
    for (uint32_t attempt = 0U; attempt < SgfConstants::GPU_MAP_GROWTH_MAX_ATTEMPTS; ++attempt)
    {
        CucoMotifMap count_map = make_cuco_motif_map(capacity);
        CucoAuxMap high_map = make_cuco_aux_map(capacity);
        CucoAuxMap low_map = make_cuco_aux_map(capacity);
        const CucoMotifMapRef count_ref = count_map.ref(cuco::op::insert_or_apply_tag{});
        const CucoAuxMapRef high_ref = high_map.ref(cuco::op::insert_and_find_tag{});
        const CucoAuxMapRef low_ref = low_map.ref(cuco::op::insert_and_find_tag{});

        uint32_t* overflow_flag = nullptr;
        cudaMallocManaged(&overflow_flag, sizeof(uint32_t));
        *overflow_flag = 0U;

        launch_kernel(count_ref, high_ref, low_ref, overflow_flag);
        cudaDeviceSynchronize();

        const bool overflowed = (*overflow_flag != 0U);
        cudaFree(overflow_flag);

        if (!overflowed)
        {
            return cuco_maps_to_enumeration_result(count_map, high_map, low_map);
        }
        capacity *= SgfConstants::GPU_MAP_GROWTH_FACTOR;
    }
    throw EnumerationOverflowException(
        "GPU enumeration map capacity exceeded after maximum growth attempts.");
}

}  // namespace sgf

#endif  // __CUDACC__
