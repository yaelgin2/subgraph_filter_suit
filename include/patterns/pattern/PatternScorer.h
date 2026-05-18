#pragma once

#include <cstdint>


namespace sgf
{

/**
 * @brief Pattern-level beam pruning scorer.
 *
 * Stateless — never instantiated.  All inputs are precomputed scalars
 * maintained incrementally by PatternState and Tree.
 */
class PatternScorer
{
public:
    PatternScorer() = delete;

    /**
     * @brief Score a candidate pattern for beam pruning.
     *
     * score = pattern_color_logp
     *       + pattern_edges * log(background_edge_density)
     *       + (potential_edges - pattern_edges) * log(1 - background_edge_density)
     *
     * where  potential_edges = vertex_count*(vertex_count-1)/2
     *
     * Lower (more negative) = rarer = better.
     *
     * @param pattern_color_logp       Σ log(p[color(v)]) over in-pattern vertices.
     * @param pattern_edges            Number of edges in the pattern.
     * @param background_edge_density  Background graph G edge density.
     * @param vertex_count             Number of vertices in the pattern.
     */
    static double score(
        double   pattern_color_logp,
        uint32_t pattern_edges,
        double   background_edge_density,
        uint32_t vertex_count);

private:
    static double clamp_probability(double probability) noexcept;
};

} // namespace sgf
