#pragma once

#include "Constants.h"

#include <array>
#include <cstdint>
#include <initializer_list>
#include <unordered_map>

namespace sgf
{

/**
 * @brief Canonical motif entry: the minimal motif number and all color
 *        permutations that map the raw motif number to it.
 *
 * Uses a fixed-size array so the struct is trivially copyable and safe for
 * both host and device use (e.g. cudaMallocManaged + memcpy). The active
 * permutation count is stored in m_permutation_count.
 *
 * The initializer_list constructor keeps MotifMap.cpp initialization syntax
 * unchanged while populating the fixed-size array on the host side.
 */
struct MotifCanonical
{
    /// Maximum number of color permutations for any 4-node motif.
    static constexpr uint32_t MAX_PERMUTATIONS = 24U;

    uint32_t m_minimal_motif_num{0U};  ///< Canonical motif number.
    uint32_t m_permutation_count{0U};  ///< Number of active entries in m_color_permutations.

    /// Color reorder indices; only [0, m_permutation_count) are valid.
    std::array<std::array<uint32_t, SgfConstants::MOTIF_SIZE>, MAX_PERMUTATIONS>
        m_color_permutations{};

    /** @brief Default construct (all zeros). Required for flat-array device allocation. */
    MotifCanonical() = default;

    /**
     * @brief Construct from an initializer list of permutations.
     *
     * Accepts existing MotifMap.cpp syntax:
     *   {minimal_num, {{p0_0, p0_1, p0_2, p0_3}, {p1_0, ...}, ...}}
     *
     * @param min_motif Canonical motif number.
     * @param perms     Color permutation entries; must not exceed MAX_PERMUTATIONS.
     */
    MotifCanonical(uint32_t min_motif,
                   std::initializer_list<std::array<uint32_t, SgfConstants::MOTIF_SIZE>> perms)
        : m_minimal_motif_num(min_motif)
        , m_permutation_count(static_cast<uint32_t>(perms.size()))
    {
        uint32_t idx = 0U;
        for (const auto& perm : perms)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
            m_color_permutations[idx++] = perm;
        }
    }
};

/**
 * @brief Motif canonicalization map for 4-node undirected colored motifs.
 */
extern const std::unordered_map<uint32_t, MotifCanonical> UNDIRECTED_MOTIF_CANONICAL_MAP;

/**
 * @brief Motif canonicalization map for 4-node directed colored motifs.
 */
extern const std::unordered_map<uint32_t, MotifCanonical> DIRECTED_MOTIF_CANONICAL_MAP;

}  // namespace sgf
