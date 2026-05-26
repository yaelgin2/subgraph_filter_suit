#include "MotifDagExpander.h"

#include "Constants.h"
#include "IGraphPreprocessor.h"
#include "Int128.h"
#include "MotifDag.h"

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

namespace sgf
{

MotifDagExpander::MotifDagExpander(const GraphType graph_type)
    : m_dag(graph_type == GraphType::DIRECTED ? DIRECTED_MOTIF_DAG : UNDIRECTED_MOTIF_DAG)
{
}

std::array<uint32_t, SgfConstants::MOTIF_SIZE>
MotifDagExpander::extract_colors(const uint32_t colors_bits)
{
    std::array<uint32_t, SgfConstants::MOTIF_SIZE> color_array{};
    for (uint32_t index = 0U; index < SgfConstants::MOTIF_SIZE; ++index)
    {
        const uint32_t shift = BITS_PER_BYTE * (SgfConstants::MOTIF_SIZE - 1U - index);
        color_array.at(index) = (colors_bits >> shift) & BYTE_MASK;
    }
    return color_array;
}

uint32_t MotifDagExpander::apply_permutation(
    const std::array<uint32_t, SgfConstants::MOTIF_SIZE>& color_array,
    const DagPermutation& permutation)
{
    uint32_t color_perm = 0U;
    for (uint32_t index = 0U; index < SgfConstants::MOTIF_SIZE; ++index)
    {
        color_perm += color_array.at(index)
                      << ((SgfConstants::MOTIF_SIZE - 1U - permutation.at(index)) * BITS_PER_BYTE);
    }
    return color_perm;
}

void MotifDagExpander::process_motif(const UInt128 motif_key, const uint32_t count,
                                     EnumerationResult& motifs,
                                     EnumerationResult& keys_to_add) const
{
    const uint32_t motif_number = static_cast<uint32_t>(motif_key >> COLOR_SHIFT);
    const auto edge_it = m_dag.find(motif_number);
    if (edge_it == m_dag.end())
    {
        return;
    }
    const std::array<uint32_t, SgfConstants::MOTIF_SIZE> color_array =
        extract_colors(static_cast<uint32_t>(motif_key));
    for (const auto& [dest_node, permutations] : edge_it->second)
    {
        for (const auto& permutation : permutations)
        {
            const uint32_t color_perm = apply_permutation(color_array, permutation);
            const UInt128 perm_key =
                (UInt128(static_cast<uint64_t>(dest_node)) << COLOR_SHIFT) + UInt128(color_perm);
            if (motifs.count(perm_key) == 0U)
            {
                keys_to_add[perm_key] += count;
            }
            else
            {
                motifs[perm_key] += count;
            }
        }
    }
}

EnumerationResult MotifDagExpander::expand(EnumerationResult motifs) const
{
    const std::vector<std::pair<UInt128, uint32_t>> snapshot(motifs.begin(), motifs.end());
    EnumerationResult keys_to_add;
    for (const auto& [motif_key, count] : snapshot)
    {
        process_motif(motif_key, count, motifs, keys_to_add);
    }
    for (const auto& [key, val] : keys_to_add)
    {
        motifs[key] += val;
    }
    return motifs;
}

}  // namespace sgf
