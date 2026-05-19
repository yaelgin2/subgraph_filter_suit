#pragma once

#include "ColoredGraph.h"
#include "LoggerHandler.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <tuple>
#include <utility>
#include <vector>

namespace sgf
{

/**
 * @brief Candidate vertex returned by get_top_k_vertices.
 */
struct CandidateVertex
{
    int32_t search_vertex;          ///< S-graph vertex index of the candidate.
    int32_t connecting_match_vertex; ///< In-match vertex to connect to (-1 if none).
    double  score;                  ///< Histogram score (lower = rarer = better).
};

/**
 * @brief Candidate-scoring histogram for a single match path.
 *
 * Maintains a set of candidate vertices (S-graph vertices adjacent to
 * the current match path but not in it) and incrementally cached scores
 * used by get_top_k_vertices() to select the best extensions.
 *
 * Each PatternState owns one histogram.  Because there is exactly one
 * match path, all caches (outside-log-prob, match-neighbour count, connect-parent)
 * are always consistent.
 *
 * Scoring formula per candidate vertex v:
 *   score(v) = match_neighbor_count(v) * background_log_density
 *            + log(p[color(v)])
 *            + current_alpha * outside_log_prob(v)
 *
 * where:
 *   current_alpha    = initial_alpha_weight * alpha_decay_rate ^ match_depth
 *   outside_log_prob = Σ log(p[color(u)]) for u ∈ neighbours(v) not in match
 */
class SingleGraphHistogram
{
public:
    /**
     * @brief Construct a histogram for one S-graph match path.
     *
     * @param search_graph                S-graph in which matching is performed.
     * @param vertex_color_probabilities  Per-colour probability distribution.
     *                                    Element c = P(a random vertex has colour c).
     *                                    Must be indexed by compact remapped colour ids.
     * @param background_log_density      log(edge_count / possible_edges) for the
     *                                    background graph G.  Used as the expected
     *                                    log-probability that any given edge exists.
     * @param initial_alpha_weight        Starting weight for the outside-neighbour
     *                                    score term.  Higher values make the scorer
     *                                    prefer candidates that have many rare unmatched
     *                                    neighbours (broader exploration).
     * @param alpha_decay_rate            Multiplicative factor applied to alpha at each
     *                                    match-depth step.  Reduces the outside-neighbour
     *                                    influence as the match grows deeper.
     * @param logger                      Optional diagnostic logger.
     */
    SingleGraphHistogram(
        const ColoredGraph&        search_graph,
        const std::vector<double>& vertex_color_probabilities,
        double                     background_log_density,
        double                     initial_alpha_weight = 1.0,
        double                     alpha_decay_rate     = 0.9,
        LoggerHandler              logger               = LoggerHandler(std::weak_ptr<ILogger>{}));

    /**
     * @brief Absorb @p vertex_to_absorb into the match path.
     *
     * Updates the candidate set and all caches:
     *  - removes vertex_to_absorb from candidates
     *  - for each neighbour of vertex_to_absorb not already in the match,
     *    registers it as a candidate (or increments its match-neighbour count)
     *  - updates outside-log-prob caches for affected candidates
     *
     * @param vertex_to_absorb  S-graph vertex to add to the match.
     * @param is_directed       When true, also processes reverse-direction neighbours.
     */
    void absorb_vertex(uint32_t vertex_to_absorb, bool is_directed);

    /**
     * @brief Return the top candidates ranked by score (lowest = rarest = best).
     *
     * @param max_result_count  Maximum number of candidates to return.
     * @return Vector of up to max_result_count CandidateVertex structs, sorted
     *         ascending by score.  Empty if no valid candidates remain.
     */
    std::vector<CandidateVertex> get_top_k_vertices(uint32_t max_result_count);

    /**
     * @brief Current match depth (number of absorbed vertices).
     */
    uint32_t current_depth() const noexcept
    {
        return m_current_depth;
    }

    /**
     * @brief log(p(color)) for a remapped colour id.
     *
     * Used by pattern-level scorers (e.g. PatternScorer) to accumulate
     * the colour log-probability term when a vertex is added to the pattern.
     *
     * @param remapped_color_id  Compact zero-based colour id (after map_colors).
     */
    double log_prob_of_color(uint32_t remapped_color_id) const;

private:
    const ColoredGraph& m_graph;
    LoggerHandler       m_logger;

    /// log(edge_count / possible_edges) of the background graph G.
    double m_background_log_density;

    /// Starting weight for the outside-neighbour score term (decays with depth).
    double m_initial_alpha_weight;

    /// Per-depth multiplicative decay applied to m_initial_alpha_weight.
    double m_alpha_decay_rate;

    uint32_t m_current_depth = 0;

    /// Cached log(p[colour]) per remapped colour id to avoid repeated std::log calls.
    std::vector<double> m_color_log_probabilities;

    /// Set of S-vertices currently in the match path.
    std::unordered_set<uint32_t> m_match_vertices;

    /// Candidate set: S-vertices adjacent to the match path but not yet in it.
    std::unordered_set<uint32_t> m_candidates;

    /// Number of match-path neighbours each candidate currently has.
    std::unordered_map<uint32_t, uint32_t> m_candidate_match_neighbor_count;

    /// Per-candidate sum of log(p[colour(u)]) for neighbours u outside the match.
    /// Maintained incrementally: decremented as each neighbour is absorbed.
    std::unordered_map<uint32_t, double> m_candidate_outside_log_prob;

    /// For each candidate, one match-path neighbour to use as the connection point.
    std::unordered_map<uint32_t, uint32_t> m_candidate_match_neighbor;

    inline double log_prob_of_vertex(uint32_t search_vertex) const;
    void add_vertex_neighbour_to_candidate(uint32_t candidate_vertex, uint32_t absorbed_vertex);
    void add_all_vertex_neighbours_to_candidate(uint32_t absorbed_vertex, bool is_reversed);
};

} // namespace sgf
