#pragma once

#include <cstdint>
#include <limits>

namespace sgf
{

/**
 * @brief Project-wide compile-time constants.
 */
class SgfConstants
{
public:
    static constexpr uint8_t BITS_PER_COLOR = 24;
    /// Maximum vertex color that can be stored in ColoredGraph's uint32_t array.
    static constexpr uint32_t MAX_VERTEX_COLOR = static_cast<uint32_t>((1U << BITS_PER_COLOR) - 1);
    static constexpr uint8_t MOTIF_SIZE = 4;
    /// Default number of worker threads used by preprocessing and filtering stages.
    static constexpr uint32_t DEFAULT_THREAD_NUMBER = 10U;

    // ── GPU UInt128 count-map constants (shared by all CUDA enumeration backends) ──

    /// Block dimension used for x and y axes in 2D GPU kernel launches.
    static constexpr uint32_t GPU_KERNEL_BLOCK_DIM = 16U;
    /// Sentinel key for empty cuco map slots (never a valid MurmurHash3 output).
    static constexpr uint64_t GPU_EMPTY_HASH_KEY = std::numeric_limits<uint64_t>::max();
    /// Fixed capacity for all cuco UInt128 count/auxiliary maps.
    static constexpr uint64_t GPU_UINT128_MAP_CAPACITY = 1'000'000UL;
    /// Seed for the primary slot key: hash(value, GPU_HASH_PRIMARY_SEED).
    static constexpr uint64_t GPU_HASH_PRIMARY_SEED = 0ULL;
    /// Seed for the double-hash probe step: hash(value, GPU_HASH_STEP_SEED) | 1.
    static constexpr uint64_t GPU_HASH_STEP_SEED = 1ULL;
    /// Multiplier applied to the cuco map capacity each time a GPU enumeration kernel
    /// reports overflow and must be retried.
    static constexpr uint64_t GPU_MAP_GROWTH_FACTOR = 4ULL;
    /// Maximum number of capacity-growth retries before a GPU enumeration gives up.
    static constexpr uint32_t GPU_MAP_GROWTH_MAX_ATTEMPTS = 4U;
};

}  // namespace sgf
