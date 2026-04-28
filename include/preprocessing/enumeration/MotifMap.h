#pragma once

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace sgf
{

/**
 * @brief Canonical motif entry: the minimal motif number and all color
 *        permutations that map the raw motif number to it.
 */
struct MotifCanonical
{
    uint32_t minimal_motif_num;
    std::vector<std::array<uint32_t, 4>> color_permutations;
};

/**
 * @brief Motif canonicalization map for 4-node undirected colored motifs.
 *
 * Maps each raw motif number to its canonical (minimal) representative and
 * the color permutations that achieve it.
 */
extern const std::unordered_map<uint32_t, MotifCanonical> UNDIRECTED_MOTIF_CANONICAL_MAP;

/**
 * @brief Motif canonicalization map for 4-node directed colored motifs.
 *
 * Maps each raw motif number to its canonical (minimal) representative and
 * the color permutations that achieve it.
 */
extern const std::unordered_map<uint32_t, MotifCanonical> DIRECTED_MOTIF_CANONICAL_MAP;

}  // namespace sgf
