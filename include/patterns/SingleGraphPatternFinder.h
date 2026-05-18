#pragma once

#include "PatternState.h"
#include "PatternScorer.h"
#include "ColoredGraph.h"
#include "BoostGraph.h"

#include <cstdint>
#include <unordered_set>
#include <utility>
#include <vector>

namespace sgf
{

/**
 * @brief Seed information for initial beam creation.
 */
struct SeedInfo {
    uint32_t              color_id;
    double                probability;
    std::vector<uint32_t> matches;
    double                weight;
};

/**
 * @brief Pattern finder for the single-S-graph case.
 *
 * Activated when the tool receives --single-s.  Exposes an identical
 * public signature to MultiGraphPatternFinder so main.cpp branches on
 * the flag and otherwise handles both paths the same way.
 *
 * Differences from MultiGraphPatternFinder:
 *  - Vertex selection uses SingleGraphHistogram (scoring formula) not
 *    GeneralColorHist (weighted random sample).
 *  - All edges are added during growth (no separate Phase 2).
 *  - Maintains a small beam of parallel candidate patterns pruned by
 *    PatternScorer.
 *  - Each beam entry tracks a single match (one S-graph path), so
 *    histogram caches are always consistent.  No Tree needed.
 *
 * Shared utilities live in PatternUtils (map_colors, recolor_pattern,
 * find_initial_matches, compute_color_distribution).
 *
 * Beam search over single-match PatternStates.  Each state tracks one
 * match path in S and its own histogram.  States split when the
 * histogram yields multiple promising candidates, and weak states are
 * pruned via a dynamic gap-based threshold.
 */
class SingleGraphPatternFinder
{
public:
    /**
     * @param max_active_patterns  Maximum simultaneous live patterns in the beam.
     * @param alpha_0              Initial alpha weight for the histogram scoring formula.
     * @param alpha_decay          Per-vertex multiplicative alpha decay.
     */
    explicit SingleGraphPatternFinder(
        uint32_t max_active_patterns  = 500,
        double   alpha_0              = 1.0,
        double   alpha_decay          = 0.9);

    /**
     * @brief Find the rarest subgraph pattern in graph S.
     *
     * @param search_graph      The search graph S.
     * @param background_graph  Background graph whose colour distribution and
     *                          edge density define the null model for scoring.
     * @param score_threshold   Stop when pattern score falls below this value.
     *
     * @return {pattern BoostGraph, alive_indexes} — alive_indexes is {0} if
     *         S still has matches, {} if the beam was exhausted.
     */
    std::vector<BoostGraph> find_pattern(
        ColoredGraph&  search_graph,
        ColoredGraph&  background_graph,
        double  score_threshold,
        bool    is_directed);

private:
    static constexpr uint32_t MAX_ITERATIONS = 50;  // Safety limit for pattern expansion
    
    uint32_t m_max_active_patterns;
    double   m_alpha_0;
    double   m_alpha_decay;

    // Private helper functions
    double score_state(PatternState& state, double background_density,bool is_directed) const;
    
    void expand_one_state(
        PatternState&          state,
        const CandidateVertex& cand,
        const ColoredGraph&           search_graph,
        bool                   is_directed) const;
    
    PatternState clone_state(const PatternState& src) const;
    
    std::vector<uint32_t> select_seed_indices(
        uint32_t total_colors,
        uint32_t initial_count) const;
    
    std::vector<uint32_t> allocate_seed_states(
        const std::vector<SeedInfo>& seeds,
        uint32_t                     target_count) const;
    
    std::vector<uint32_t> allocate_seed_states_improved(
        const std::vector<SeedInfo>& seeds,
        uint32_t                     target_count) const;
    
    std::vector<SeedInfo> select_valid_seeds(
        const std::vector<std::tuple<double, uint32_t, uint32_t>>& valid_colors,
        const std::vector<std::vector<uint32_t>>& all_matches,
        uint32_t initial_count) const;
    
    std::vector<PatternState> create_beam_from_seeds(
        const std::vector<SeedInfo>& seeds,
        const std::vector<uint32_t>& alloc,
        const ColoredGraph& search_graph,
        const std::vector<double>& color_probability,
        const std::vector<int32_t>& color_map,
        double log_bg_density,
        double alpha_0,
        double alpha_decay,
        bool is_directed) const;
    
    PatternState create_initial_state(
        const ColoredGraph&               search_graph,
        const std::vector<double>& color_probability,
        double                     log_bg_density,
        double                     alpha_0,
        double                     alpha_decay,
        uint32_t                   color_id,
        uint32_t                   match_vertex,
        bool                       is_directed) const;
    
    uint32_t find_gap_cut(
        const std::vector<std::pair<double, uint32_t>>& scored) const;
    
    std::vector<PatternState*> select_best_state(
        std::vector<PatternState>& beam,
        double                     background_density,
        bool                       is_directed) const;
    
    bool any_state_below_threshold(
        std::vector<PatternState>& beam,
        double bg_density, double threshold, bool is_directed) const;

    /**
     * @brief Build the initial beam from diverse seed colours.
     *
     * Picks seed colours distributed across the probability range (always
     * includes the rarest).  Rarer colours receive more initial matches.
     * Total states ≈ m_max_active_patterns / INITIAL_BEAM_DIVISOR.
     */
    std::vector<PatternState> build_initial_beam(
        const ColoredGraph&                search_graph,
        const std::vector<double>&  color_probability,
        const std::vector<int32_t>& color_map,
        double                      background_density,
        bool                        is_directed) const;

    /**
     * @brief Expand each live state by cloning it for the top-K candidates.
     *
     * K = max(1, m_max_active_patterns / current_beam_size) so that the
     * beam naturally fills toward m_max_active_patterns.
     *
     * @return false when every state is dead or cannot be extended.
     */
    bool expand_beam(
        std::vector<PatternState>& beam,
        const ColoredGraph&               search_graph,
        bool                       is_directed) const;

    /**
     * @brief Prune weak states using a dynamic gap-based threshold.
     *
     * - iteration < 5: no score-based pruning (only hard cap).
     * - iteration >= 5: sort scores, find largest gap; if the gap is
     *   significantly larger than the median gap, prune everything
     *   worse than the gap.  Always cap at m_max_active_patterns.
     */
    void prune_beam(
        std::vector<PatternState>& beam,
        double                     background_density,
        uint32_t                   iteration,
        bool                       is_directed) const;
};

} // namespace sgf
