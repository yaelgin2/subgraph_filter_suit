#pragma once

#include "PatternState.h"
#include "PatternScorer.h"
#include "ColoredGraph.h"
#include "BoostGraph.h"
#include "LoggerHandler.h"

#include <cstdint>
#include <unordered_set>
#include <utility>
#include <vector>

namespace sgf
{

/**
 * @brief Colour-index and match bookkeeping for one beam seed.
 *
 * A seed is a starting colour chosen for initial pattern vertices.
 * The initial beam contains one PatternState per selected seed vertex.
 */
struct SeedInfo
{
    uint32_t              color_id;                  ///< Compact (remapped) colour id.
    double                probability;               ///< P(a random vertex has this colour).
    std::vector<uint32_t> vertex_matches;            ///< All S-graph vertices with this colour.
    double                inverse_probability_weight; ///< 1/probability; rarer colours score higher.
};

/**
 * @brief Finds the rarest subgraph pattern in a single search graph using beam search.
 *
 * ### Algorithm overview
 *
 * 1. **Colour remapping** — vertex colours in both the search graph S and the
 *    background graph G are remapped to compact zero-based IDs so that colour
 *    probability vectors can be indexed directly.
 *
 * 2. **Null model** — the "rarity" of a pattern is measured as its
 *    log-likelihood under a random-graph null model (PatternScorer):
 *      score = Σ log P(color(v)) + edge_count * log(density)
 *                                + (potential_edges - edge_count) * log(1 - density)
 *    Lower (more negative) = rarer = better pattern.
 *
 * 3. **Initial beam** — seed PatternStates are created from the rarest colours
 *    in S.  Each state begins with one absorbed vertex and its own histogram.
 *    The initial beam is intentionally small (m_max_active_patterns / INITIAL_BEAM_DIVISOR)
 *    so the expansion loop can fill it toward m_max_active_patterns.
 *
 * 4. **Expansion** — every live state asks its SingleGraphHistogram for the top-K
 *    candidate vertices, then clones itself for each candidate.  K is set so
 *    that the beam grows toward m_max_active_patterns naturally.
 *
 * 5. **Pruning** — after each expansion, states are scored and the weakest are
 *    dropped.  After a warmup period, a dynamic gap-based cut is applied: the
 *    largest score gap in the sorted list becomes the cut point if it is
 *    significant enough.
 *
 * 6. **Termination** — the loop ends when the beam is full, the safety iteration
 *    limit is reached, or any state's score falls below the caller's threshold.
 *
 * 7. **Output** — the top NUMBER_OF_STATES_TO_RETURN patterns by score are
 *    returned after their colours are remapped back to original values.
 */
class SingleGraphPatternFinder
{
public:
    /**
     * @brief Construct the finder with beam-search parameters.
     *
     * @param max_active_patterns  Hard cap on simultaneous live beam states.
     * @param alpha_0              Initial weight for the outside-neighbour score term
     *                             in SingleGraphHistogram.  Passed through to every histogram.
     * @param alpha_decay          Per-depth multiplicative decay applied to alpha_0.
     *                             Reduces outside-neighbour influence as the pattern grows.
     * @param logger               Optional diagnostic logger.
     */
    explicit SingleGraphPatternFinder(
        uint32_t      max_active_patterns  = 500,
        double        alpha_0              = 1.0,
        double        alpha_decay          = 0.9,
        LoggerHandler logger               = LoggerHandler(std::weak_ptr<ILogger>{}));

    /**
     * @brief Find the rarest subgraph patterns in the search graph.
     *
     * Both graphs are modified in-place (colour remapping) and restored
     * to original colours in the returned patterns.
     *
     * @param search_graph      Graph S to search in.
     * @param background_graph  Graph G whose colour distribution and edge density
     *                          define the null model (scoring reference).
     * @param score_threshold   Stop as soon as any beam state's score falls below
     *                          this value (early exit when a rare-enough pattern is found).
     * @param is_directed       When true, treats both graphs as directed and adds
     *                          edges in both directions independently.
     *
     * @return Up to NUMBER_OF_STATES_TO_RETURN BoostGraph patterns, sorted best first.
     *         Empty if no valid seed colours exist in S.
     */
    std::vector<BoostGraph> find_pattern(
        ColoredGraph& search_graph,
        ColoredGraph& background_graph,
        double        score_threshold,
        bool          is_directed);

private:
    /// Safety cap on expansion iterations to prevent infinite loops on degenerate inputs.
    static constexpr uint32_t MAX_ITERATIONS = 50;

    uint32_t      m_max_active_patterns; ///< Hard cap on live beam states.
    double        m_alpha_0;             ///< Initial alpha weight forwarded to histograms.
    double        m_alpha_decay;         ///< Per-depth alpha decay forwarded to histograms.
    LoggerHandler m_logger;

    /**
     * @brief Score a single beam state under the null model.
     *
     * Delegates to PatternScorer::score.  For undirected patterns the BoostGraph
     * stores each edge twice (both directions), so edge count is halved.
     */
    double score_state(PatternState& state, double background_density, bool is_directed) const;

    /**
     * @brief Extend @p state by absorbing one new S-graph vertex.
     *
     * Adds the vertex and its colour to the pattern, wires up all edges
     * between the new vertex and already-matched vertices, then calls
     * hist->absorb_vertex so that the histogram caches stay consistent.
     */
    void expand_one_state(
        PatternState&          state,
        const CandidateVertex& candidate,
        const ColoredGraph&    search_graph,
        bool                   is_directed) const;

    /**
     * @brief Deep-copy a PatternState for beam branching.
     *
     * The histogram is deep-copied so each branch maintains independent
     * candidate caches.  The pattern BoostGraph is copied by value.
     */
    PatternState clone_state(const PatternState& source_state) const;

    /**
     * @brief Choose evenly-spaced indices into a sorted colour array.
     *
     * Always includes index 0 (rarest colour).  Remaining indices are
     * distributed uniformly so the beam starts with diverse seed colours.
     *
     * @param total_color_count  Number of distinct colours available.
     * @param initial_beam_size  Target number of initial states.
     * @return Indices into the sorted colour array (ascending).
     */
    std::vector<uint32_t> select_seed_indices(
        uint32_t total_color_count,
        uint32_t initial_beam_size) const;

    /**
     * @brief Allocate beam states across seeds proportional to rarity.
     *
     * Each seed gets max(1, round(target * weight / total_weight)) states,
     * capped by the number of available vertex matches for that colour.
     * Does not guarantee the total reaches target_state_count exactly.
     *
     * @param seeds              Seed descriptors with weights and match lists.
     * @param target_state_count Desired total number of initial states.
     * @return Per-seed state counts (same length as seeds).
     */
    std::vector<uint32_t> allocate_seed_states(
        const std::vector<SeedInfo>& seeds,
        uint32_t                     target_state_count) const;

    /**
     * @brief Allocate states proportionally then redistribute leftover quota.
     *
     * First pass: same proportional allocation as allocate_seed_states.
     * Second pass: any remaining quota is given to seeds with spare capacity,
     * prioritising rarer colours (higher inverse_probability_weight).
     * Guarantees the total reaches target_state_count when enough matches exist.
     *
     * @param seeds              Seed descriptors with weights and match lists.
     * @param target_state_count Desired total number of initial states.
     * @return Per-seed state counts (same length as seeds).
     */
    std::vector<uint32_t> allocate_seed_states_improved(
        const std::vector<SeedInfo>& seeds,
        uint32_t                     target_state_count) const;

    /**
     * @brief Select which colours to use as seeds for the initial beam.
     *
     * Iterates through colours in rarest-first order, collecting seeds until
     * accumulated match count reaches 2 * initial_beam_size.  The 2x factor
     * ensures sufficient diversity even after allocation capping.
     *
     * @param sorted_colors_with_matches  (probability, color_id, match_count) tuples, ascending.
     * @param vertices_by_color           All S-vertices grouped by colour.
     * @param initial_beam_size           Target number of initial states.
     * @return SeedInfo list ready for allocation.
     */
    std::vector<SeedInfo> select_valid_seeds(
        const std::vector<std::tuple<double, uint32_t, uint32_t>>& sorted_colors_with_matches,
        const std::vector<std::vector<uint32_t>>&                   vertices_by_color,
        uint32_t                                                     initial_beam_size) const;

    /**
     * @brief Immutable context bundle passed to beam-initialisation helpers.
     *
     * Avoids passing eight separate parameters through create_beam_from_seeds
     * and create_initial_state.  All reference fields must outlive the BeamContext.
     */
    struct BeamContext
    {
        const ColoredGraph&         search_graph;        ///< S-graph (remapped colours).
        const std::vector<double>&  color_probability;   ///< P(colour c) from background G.
        const std::vector<int32_t>& color_map;           ///< Compact id → original colour value.
        double                      background_log_density; ///< log(edge density of G).
        bool                        is_directed;         ///< True when edges are directed.
    };

    /**
     * @brief Mutable state accumulated during the full beam search.
     *
     * Returned by initialise_beam_search and consumed by run_beam_expansion
     * and collect_best_patterns.
     */
    struct BeamSearchState
    {
        std::vector<int32_t>      color_map;            ///< Compact id → original colour (for recolor).
        double                    background_density = 0.0; ///< Raw edge density of G.
        std::vector<PatternState> beam;                 ///< Live beam states.
    };

    /**
     * @brief Instantiate PatternStates from seed descriptors and allocation counts.
     *
     * For each seed, creates seed_state_counts[i] initial states, one per
     * distinct S-vertex match, each with its own SingleGraphHistogram.
     */
    std::vector<PatternState> create_beam_from_seeds(
        const std::vector<SeedInfo>& seeds,
        const std::vector<uint32_t>& seed_state_counts,
        const BeamContext&           context) const;

    /**
     * @brief Create one PatternState seeded at a single S-graph vertex.
     *
     * Builds a fresh SingleGraphHistogram, absorbs the seed vertex,
     * creates a one-vertex BoostGraph pattern, and initialises
     * pattern_vertex_color_log_prob from the colour probability.
     */
    PatternState create_initial_state(
        const BeamContext& context,
        uint32_t           color_id,
        uint32_t           match_vertex) const;

    /**
     * @brief Find the index at which to cut a sorted score list (gap detection).
     *
     * Looks for the largest score gap strictly after the minimum-keep floor.
     * If that gap is at least MIN_GAP_SCORE_RATIO of the total score range,
     * returns the cut position (discard everything after it).
     * Returns scored_states.size() when no significant gap is found.
     *
     * @param scored_states  (score, beam_index) pairs sorted ascending by score.
     * @return Cut index: keep [0, cut), discard [cut, end).
     */
    uint32_t find_gap_cut(
        const std::vector<std::pair<double, uint32_t>>& scored_states) const;

    /**
     * @brief Return pointers to the NUMBER_OF_STATES_TO_RETURN best beam states.
     *
     * Scores every state, sorts ascending, and returns pointers into @p beam.
     * The caller must not invalidate the beam before using the pointers.
     */
    std::vector<PatternState*> select_best_state(
        std::vector<PatternState>& beam,
        double                     background_density,
        bool                       is_directed) const;

    /**
     * @brief Check whether any beam state already beats the score threshold.
     *
     * Used to detect early termination: if any state's score is already
     * below the threshold, further expansion cannot improve the best result.
     */
    bool any_state_below_threshold(
        std::vector<PatternState>& beam,
        double                     background_density,
        double                     threshold,
        bool                       is_directed) const;

    /**
     * @brief Build the initial beam from diverse seed colours.
     *
     * Selects seed colours spread across the rarity range, always including
     * the rarest.  Allocates m_max_active_patterns / INITIAL_BEAM_DIVISOR
     * total states so the expansion loop has room to grow the beam.
     */
    std::vector<PatternState> build_initial_beam(
        const ColoredGraph&        search_graph,
        const std::vector<double>& color_probability,
        const std::vector<int32_t>& color_map,
        double                      background_density,
        bool                        is_directed) const;

    /**
     * @brief Expand each live state by cloning for the top-K candidates.
     *
     * K = max(1, m_max_active_patterns / current_beam_size) so the beam
     * naturally fills toward m_max_active_patterns without exceeding it.
     * States with no candidates are moved unchanged into the new beam.
     *
     * @return true if at least one state was successfully expanded.
     */
    bool expand_beam(
        std::vector<PatternState>& beam,
        const ColoredGraph&        search_graph,
        bool                       is_directed) const;

    /**
     * @brief Drop weak beam states using a score-gap heuristic.
     *
     * For the first PRUNE_WARMUP_ITERATIONS iterations only the hard size cap
     * is enforced (gap pruning would be premature on small, noisy beams).
     * After warmup, states are sorted by score and find_gap_cut determines
     * how many to keep.  The hard cap m_max_active_patterns is always applied.
     */
    void prune_beam(
        std::vector<PatternState>& beam,
        double                     background_density,
        uint32_t                   iteration,
        bool                       is_directed) const;

    /**
     * @brief Remap colours in S and G and build the initial beam.
     *
     * Remaps vertex colours in both graphs to compact shared IDs via
     * PatternUtils::map_colors (modifies both graphs in-place), computes the
     * background colour distribution and edge density, then calls build_initial_beam.
     * Original colour values are preserved in BeamSearchState::color_map for
     * later restoration by collect_best_patterns.
     */
    BeamSearchState initialise_beam_search(
        ColoredGraph& search_graph,
        ColoredGraph& background_graph,
        bool          is_directed) const;

    /**
     * @brief Run the expand-and-check loop until a termination condition fires.
     *
     * Terminates when the beam reaches m_max_active_patterns, any state score
     * falls below score_threshold, no further expansion is possible, or
     * MAX_ITERATIONS is reached.
     *
     * @param beam_state       Mutable beam (beam, color_map, background_density).
     * @param search_graph     Graph S (with remapped colours) to expand candidates in.
     * @param score_threshold  Stop early when any state's score beats this value.
     * @param is_directed      Whether to treat edges as directed.
     */
    void run_beam_expansion(
        BeamSearchState&    beam_state,
        const ColoredGraph& search_graph,
        double              score_threshold,
        bool                is_directed) const;

    /**
     * @brief Select the best patterns, restore original colours, and return them.
     *
     * Scores all beam states, picks the top NUMBER_OF_STATES_TO_RETURN,
     * remaps vertex colours back to original values via beam_state.color_map,
     * and moves the resulting BoostGraph patterns into the return vector.
     */
    std::vector<BoostGraph> collect_best_patterns(
        BeamSearchState& beam_state,
        bool             is_directed) const;
};

} // namespace sgf
