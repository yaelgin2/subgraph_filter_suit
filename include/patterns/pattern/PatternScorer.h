#pragma once

#include <cstdint>


namespace sgf
{

/**
 * @brief Stateless scorer that assigns a log-likelihood score to a candidate pattern.
 *
 * The score measures how surprising (rare) the pattern is under a random-graph
 * null model where each vertex colour is i.i.d. from the background distribution
 * and each edge exists independently with the background graph's edge density.
 *
 * Lower (more negative) score = more surprising = better pattern.
 *
 * All inputs are scalars maintained incrementally by PatternState, so this
 * class never needs to iterate over pattern vertices or edges directly.
 */
class PatternScorer
{
public:
    PatternScorer() = delete;

    /**
     * @brief Score a candidate pattern for beam pruning.
     *
     * score = pattern_vertex_color_log_prob
     *       + pattern_edges * log(background_edge_density)
     *       + (potential_edges - pattern_edges) * log(1 - background_edge_density)
     *
     * where  potential_edges = vertex_count*(vertex_count-1)/2
     *
     * Lower (more negative) = rarer = better.
     *
     * @param pattern_vertex_color_log_prob  Σ log(p[color(v)]) over in-pattern vertices.
     * @param pattern_edge_count             Number of edges in the pattern.
     * @param background_edge_density        Background graph G edge density.
     * @param vertex_count                   Number of vertices in the pattern.
     */
    static double score(
        double   pattern_vertex_color_log_prob,
        uint32_t pattern_edge_count,
        double   background_edge_density,
        uint32_t vertex_count);

private:
    /**
     * @brief Clamp a probability to (epsilon, 1-epsilon) to keep log() finite.
     *
     * Without clamping, log(0) = -inf and log(1) = 0 break the edge-term
     * calculation.  The epsilon of 1e-12 is negligible in practice but keeps
     * all arithmetic well-defined.
     */
    static double clamp_probability(double probability) noexcept;
};

} // namespace sgf
