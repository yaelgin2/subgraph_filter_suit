#include "PatternScorer.h"

#include <cmath>
#include <cstdint>
#include <iostream>

namespace sgf
{

/**
 * @brief Compute the log-likelihood score of a pattern under a random-graph
 *        null model.  More negative = more surprising = better pattern.
 *
 * --- Null model ---
 *   1. Each vertex colour is drawn i.i.d. from the global colour distribution.
 *   2. Each possible undirected edge exists independently with probability
 *      equal to the background graph G's edge density.
 *
 * --- Score formula ---
 *   score = colour_term + edge_term
 *
 *   colour_term = sum_i log P(c_i)       (= pattern_vertex_color_log_prob,
 *                                           accumulated incrementally as vertices are added)
 *
 *   edge_term   = pattern_edges * log(background_edge_density)
 *               + (potential_pattern_edges - pattern_edges)
 *                 * log(1 - background_edge_density)
 *
 *   This is the log-probability of a Binomial(potential_pattern_edges,
 *   background_edge_density) observation with the pattern's actual edge count.
 */
double PatternScorer::score(
    double   pattern_vertex_color_log_prob,
    uint32_t pattern_edge_count,
    double   background_edge_density,
    uint32_t vertex_count)
{
    if (vertex_count == 0)
        return 0.0;

    const uint64_t potential_pattern_edges =
        static_cast<uint64_t>(vertex_count)
        * static_cast<uint64_t>(vertex_count - 1) / 2ULL;

    // Single-vertex pattern: no edges possible, score is colour-only.
    if (potential_pattern_edges == 0)
        return pattern_vertex_color_log_prob;

    // Clamp background density into (epsilon, 1-epsilon) so that
    // log(density) and log(1-density) never produce -inf or NaN.
    const double background_edge_probability =
        clamp_probability(background_edge_density);

    // Log-probability of observing exactly pattern_edge_count edges out of
    // potential_pattern_edges possible under independent Bernoulli trials.
    const double edge_log_probability =
        static_cast<double>(pattern_edge_count)
            * std::log(background_edge_probability)
        + static_cast<double>(potential_pattern_edges - pattern_edge_count)
            * std::log(1.0 - background_edge_probability);
    return pattern_vertex_color_log_prob + edge_log_probability;
}

double PatternScorer::clamp_probability(double probability) noexcept
{
    constexpr double epsilon = 1e-12;
    if (probability < epsilon)       return epsilon;
    if (probability > 1.0 - epsilon) return 1.0 - epsilon;
    return probability;
}

} // namespace sgf
