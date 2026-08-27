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
    /// Initial capacity for MotifPreprocessor's cuco UInt128 count/auxiliary maps.
    /// Motifs have a small, bounded number of distinct canonical structures.
    static constexpr uint64_t GPU_MOTIF_MAP_INITIAL_CAPACITY = 1'000'000UL;
    /// Initial capacity for PathProcessor's cuco UInt128 count/auxiliary maps.
    /// Paths canonicalize to orders of magnitude more distinct ids than motifs
    /// do (a single mid-sized graph can already exceed a million distinct
    /// paths), so they need a much larger starting point to avoid thrashing
    /// an already-overfull map before the first overflow-triggered retry.
    static constexpr uint64_t GPU_PATH_MAP_INITIAL_CAPACITY = 100'000'000UL;
    /// Seed for the primary slot key: hash(value, GPU_HASH_PRIMARY_SEED).
    static constexpr uint64_t GPU_HASH_PRIMARY_SEED = 0ULL;
    /// Seed for the double-hash probe step: hash(value, GPU_HASH_STEP_SEED) | 1.
    static constexpr uint64_t GPU_HASH_STEP_SEED = 1ULL;
    /// Multiplier applied to the cuco map capacity each time a GPU enumeration kernel
    /// reports overflow and must be retried.
    static constexpr uint64_t GPU_MAP_GROWTH_FACTOR = 10ULL;
    /// Maximum number of capacity-growth retries before a GPU enumeration gives up.
    static constexpr uint32_t GPU_MAP_GROWTH_MAX_ATTEMPTS = 4U;
    /// Bit index within a UInt128 key's high 64-bit half that is guaranteed always 0
    /// for every real value stored via atomic_insert_uint128_count() - used to smuggle
    /// the low half's own top bit so its sentinel-avoidance transform is reversible.
    /// Verified safe for every current caller's key layout:
    ///  - Motif keys (MotifPreprocessor::calculate_motif_number_from_arrays): the
    ///    canonical motif number occupies bits [96, 96+12) of the full 128-bit value
    ///    (< 4096 directed 4-vertex edge descriptors), leaving high-half bits [44, 64)
    ///    always 0.
    ///  - Path keys (PathProcessor::calculate_motif_number): the 4-bit path descriptor
    ///    occupies bits [120, 124), leaving high-half bits [60, 64) always 0.
    /// Bit 62 falls inside both always-0 ranges.
    static constexpr uint32_t GPU_UINT128_HIGH_SPARE_BIT = 62U;
};

}  // namespace sgf
