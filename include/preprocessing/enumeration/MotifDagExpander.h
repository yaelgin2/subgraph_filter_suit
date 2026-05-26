#pragma once

#include "IGraphPreprocessor.h"
#include "MotifDag.h"

namespace sgf
{

/**
 * @class MotifDagExpander
 * @brief Converts induced motif counts to non-induced by propagating along the
 *        motif inclusion DAG.
 *
 * For each motif key in the input EnumerationResult, looks up the corresponding
 * motif structure node in the DAG and, for every out-edge, applies the stored
 * color permutations to produce the sub-motif key and accumulates the count.
 * Keys that already exist in the result are incremented in-place; new keys are
 * collected and merged at the end to avoid iterator invalidation.
 */
class MotifDagExpander
{
public:
    /**
     * @brief Selects which inclusion DAG to use.
     */
    enum class GraphType
    {
        DIRECTED,
        UNDIRECTED
    };

    /**
     * @brief Construct the expander for the given graph type.
     * @param graph_type DIRECTED or UNDIRECTED selects the inclusion DAG.
     */
    explicit MotifDagExpander(GraphType graph_type);

    ~MotifDagExpander() = default;
    MotifDagExpander(const MotifDagExpander&) = delete;
    MotifDagExpander& operator=(const MotifDagExpander&) = delete;
    MotifDagExpander(MotifDagExpander&&) = delete;
    MotifDagExpander& operator=(MotifDagExpander&&) = delete;

    /**
     * @brief Expand induced motif counts to non-induced counts.
     *
     * Takes the input by value so it can be modified and returned directly.
     *
     * @param motifs Induced motif frequency map for one graph.
     * @return Non-induced motif frequency map.
     */
    [[nodiscard]] EnumerationResult expand(EnumerationResult motifs) const;

private:
    static constexpr uint32_t BITS_PER_BYTE = 8U;
    static constexpr uint32_t COLOR_SHIFT = BITS_PER_BYTE * SgfConstants::MOTIF_SIZE;
    static constexpr uint32_t BYTE_MASK = 0xFFU;

    const DagAdjacency& m_dag;

    /**
     * @brief Extract the per-vertex 8-bit color values packed in the low COLOR_SHIFT bits.
     * @param colors_bits Low 32 bits of a motif key.
     * @return Array of MOTIF_SIZE color values.
     */
    static std::array<uint32_t, SgfConstants::MOTIF_SIZE> extract_colors(uint32_t colors_bits);

    /**
     * @brief Apply a permutation to a color array, returning the repacked color bits.
     * @param color_array Per-vertex colors extracted by extract_colors.
     * @param permutation Permutation indices from a DAG edge.
     * @return Repacked color bits after applying the permutation.
     */
    static uint32_t apply_permutation(
        const std::array<uint32_t, SgfConstants::MOTIF_SIZE>& color_array,
        const DagPermutation& permutation);

    /**
     * @brief Process one motif key: follow all DAG out-edges and accumulate sub-motif counts.
     * @param motif_key The full 128-bit motif key.
     * @param count     Occurrence count of this motif.
     * @param motifs    Result map updated in-place for existing keys.
     * @param keys_to_add Accumulator for newly discovered keys.
     */
    void process_motif(UInt128 motif_key, uint32_t count, EnumerationResult& motifs,
                       EnumerationResult& keys_to_add) const;
};

}  // namespace sgf
