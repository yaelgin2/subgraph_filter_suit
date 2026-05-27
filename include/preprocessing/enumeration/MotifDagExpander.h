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
    static constexpr uint32_t COLOR_SHIFT = static_cast<uint32_t>(SgfConstants::BITS_PER_COLOR) *
                                            static_cast<uint32_t>(SgfConstants::MOTIF_SIZE);

    const DagAdjacency& m_dag;

    /**
     * @brief Extract the per-vertex 24-bit color values packed in the low COLOR_SHIFT bits.
     *
     * Colors are stored LSB-first: color[i] at bits [i*BITS_PER_COLOR : (i+1)*BITS_PER_COLOR-1].
     *
     * @param colors_bits Low COLOR_SHIFT bits of a motif key.
     * @return Array of MOTIF_SIZE color values.
     */
    static std::array<uint32_t, SgfConstants::MOTIF_SIZE>
    extract_colors(const UInt128& colors_bits);

    /**
     * @brief Apply a permutation to a color array, returning the repacked 96-bit color section.
     *
     * Applies output[permutation[i]] = color_array[i], packed LSB-first.
     *
     * @param color_array Per-vertex colors extracted by extract_colors.
     * @param permutation Permutation indices from a DAG edge.
     * @return Repacked 96-bit color section after applying the permutation.
     */
    static UInt128
    apply_permutation(const std::array<uint32_t, SgfConstants::MOTIF_SIZE>& color_array,
                      const DagPermutation& permutation);

    /**
     * @brief Process one motif key: follow all DAG out-edges and accumulate sub-motif counts.
     *
     * Safe to call while holding a snapshot of the original keys, since iteration
     * is over the snapshot, not over @p motifs directly.
     *
     * @param motif_key The full 128-bit motif key.
     * @param count     Occurrence count of this motif.
     * @param motifs    Result map updated in-place (new keys may be inserted).
     */
    void process_motif(UInt128 motif_key, uint32_t count, EnumerationResult& motifs) const;
};

}  // namespace sgf
