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
MotifDagExpander::extract_colors(const UInt128& colors_bits)
{
    std::array<uint32_t, SgfConstants::MOTIF_SIZE> color_array{};
    for (uint32_t index = 0U; index < SgfConstants::MOTIF_SIZE; ++index)
    {
        const uint32_t shift = index * static_cast<uint32_t>(SgfConstants::BITS_PER_COLOR);
        color_array.at(index) =
            static_cast<uint32_t>(colors_bits >> shift) & SgfConstants::MAX_VERTEX_COLOR;
    }
    return color_array;
}

UInt128 MotifDagExpander::apply_permutation(
    const std::array<uint32_t, SgfConstants::MOTIF_SIZE>& color_array,
    const DagPermutation& permutation)
{
    UInt128 color_perm{};
    for (uint32_t index = 0U; index < SgfConstants::MOTIF_SIZE; ++index)
    {
        const uint32_t dest_shift =
            permutation.at(index) * static_cast<uint32_t>(SgfConstants::BITS_PER_COLOR);
        color_perm = color_perm + (UInt128{color_array.at(index)} << dest_shift);
    }
    return color_perm;
}

void MotifDagExpander::process_motif(const UInt128 motif_key, const uint32_t count,
                                     EnumerationResult& motifs) const
{
    const uint32_t motif_number = static_cast<uint32_t>(motif_key >> COLOR_SHIFT);
    const auto edge_it = m_dag.find(motif_number);
    if (edge_it == m_dag.end())
    {
        return;
    }
    const UInt128 color_section =
        motif_key - (UInt128{static_cast<uint64_t>(motif_number)} << COLOR_SHIFT);
    const std::array<uint32_t, SgfConstants::MOTIF_SIZE> color_array =
        extract_colors(color_section);
    for (const auto& [dest_node, permutations] : edge_it->second)
    {
        for (const auto& permutation : permutations)
        {
            const UInt128 color_perm = apply_permutation(color_array, permutation);
            const UInt128 perm_key =
                (UInt128{static_cast<uint64_t>(dest_node)} << COLOR_SHIFT) + color_perm;
            motifs[perm_key] += count;
        }
    }
}

EnumerationResult MotifDagExpander::expand(EnumerationResult motifs) const
{
    const std::vector<std::pair<UInt128, uint32_t>> snapshot(motifs.begin(), motifs.end());
    for (const auto& [motif_key, count] : snapshot)
    {
        process_motif(motif_key, count, motifs);
    }
    return motifs;
}

}  // namespace sgf
