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

/** Candidate returned by get_top_k_vertices. */
struct CandidateVertex {
    int32_t  s_vertex;         ///< S-graph vertex index.
    int32_t  connect_s_vertex; ///< In-match vertex to connect to.
    double   score;            ///< Histogram score (lower = rarer).
};

/**
 * @brief Candidate-scoring histogram for a single match path.
 *
 * Maintains a set of candidate vertices (S-graph vertices adjacent to
 * the current match path but not in it) and incrementally cached scores
 * used by get_top_k_vertices() to select the best extensions.
 *
 * Each PatternState owns one histogram.  Because there is exactly one
 * match path, all caches (outside-logp, in-degree, connect-parent) are
 * always consistent.
 */
class SingleGraphHistogram
{
public:
    SingleGraphHistogram(
        const ColoredGraph&        graph,
        const std::vector<double>& color_prob,
        double                     log_density,
        double                     alpha_0 = 1.0,
        double                     decay   = 0.9,
        LoggerHandler              logger  = LoggerHandler(std::weak_ptr<ILogger>{}));

    /**
     * @brief Absorb @p s_vertex into the match path.
     *
     * Updates the candidate set and all caches:
     *  - removes s_vertex from candidates
     *  - for each neighbour of s_vertex not already in the match,
     *    registers it as a candidate (or increments its in-degree)
     *  - updates outside-logp caches for affected candidates
     */
    void absorb_vertex(uint32_t s_vertex, bool directed);

    /**
     * @brief Return the top-k best candidate vertices with connect points.
     * @param k  Maximum number of candidates to return.
     * @return Vector of up to k CandidateVertex, sorted best (lowest score) first.
     *         Empty if no candidates remain.
     */
    std::vector<CandidateVertex> get_top_k_vertices(uint32_t k);

    /** Current match depth (number of absorbed vertices). */
    uint32_t current_depth() const noexcept { return m_current_depth; }

    /** log(p(color)) for a remapped colour id. Used by pattern-level scorers. */
    double log_prob_of_color(uint32_t remapped_color) const;

private:
    const ColoredGraph& m_graph;
    LoggerHandler m_logger;
    double       m_log_density;
    double       m_alpha_0;
    double       m_decay;
    uint32_t     m_current_depth = 0;

    // Cache log p(color) per remapped colour id to avoid repeated std::log.
    std::vector<double> m_color_log_prob;

    // Set of S-vertices currently in the match path.
    std::unordered_set<uint32_t> m_match_vertices;

    // Candidate set: outside-neighbour vertices with >=1 match-path neighbour.
    std::unordered_set<uint32_t>           m_candidates;
    std::unordered_map<uint32_t, uint32_t> m_candidate_in_degree;

    // Per-candidate cache: sum of log(p[color(u)]) for u in neighbours(v)
    // that are currently outside the match path. Updated incrementally.
    std::unordered_map<uint32_t, double>   m_candidate_outside_logp;

    // For each candidate, any match-path neighbour to connect to.
    std::unordered_map<uint32_t, uint32_t> m_candidate_any_parent;

    inline double log_prob_of_vertex(uint32_t s_vertex) const;
    void add_vertex_neighbour_to_candidate(uint32_t vertex, uint32_t absorbed_vertex);
    void add_all_vertex_neighbours_to_candidate(uint32_t absorbed_vertex, bool is_reversed);
};

}
