#pragma once

#include "BoostGraph.h"
#include "ColoredGraph.h"
#include "LoggerHandler.h"
#include "PatternScorer.h"
#include "PatternState.h"

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
    uint32_t m_color_id;                     ///< Compact (remapped) colour id.
    double m_probability;                    ///< P(a random vertex has this colour).
    std::vector<uint32_t> m_vertex_matches;  ///< All S-graph vertices with this colour.
    double m_inverse_probability_weight;     ///< 1/probability; rarer colours score higher.
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
 * 6. **Termination** — the loop ends when no further expansion is possible,
 *    the safety iteration limit is reached, or any state's score falls below
 *    the caller's threshold.
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
        uint32_t max_active_patterns = DEFAULT_MAX_ACTIVE_PATTERNS, double alpha_0 = 1.0,
        double alpha_decay = DEFAULT_ALPHA_DECAY,
        LoggerHandler logger = LoggerHandler(std::weak_ptr<ILogger>{}));

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
    std::vector<BoostGraph> find_pattern(ColoredGraph& search_graph, ColoredGraph& background_graph,
                                         double score_threshold, bool is_directed);

private:
    /// Default maximum number of simultaneously active beam states.
    static constexpr uint32_t DEFAULT_MAX_ACTIVE_PATTERNS = 500;

    /// Default per-depth multiplicative decay applied to the alpha weight.
    static constexpr double DEFAULT_ALPHA_DECAY = 0.9;

    /// Safety cap on expansion iterations to prevent infinite loops on degenerate inputs.
    static constexpr uint32_t MAX_ITERATIONS = 50;

    /// The initial beam contains m_max_active_patterns / INITIAL_BEAM_DIVISOR states.
    static constexpr uint32_t INITIAL_BEAM_DIVISOR = 3;

    /// Minimum number of distinct seed colours chosen during beam initialisation.
    static constexpr uint32_t MIN_SEED_COLORS = 3;

    /// Rough number of initial beam states allocated per seed colour.
    static constexpr uint32_t STATES_PER_SEED = 10;

    /// Number of expansion iterations before gap-based pruning is enabled.
    static constexpr uint32_t PRUNE_WARMUP_ITERATIONS = 5;

    /// After warmup, always keep at least this fraction of scored states.
    static constexpr double MIN_KEEP_FRACTION = 0.3;

    /// A gap must be at least this fraction of the total score range to trigger pruning.
    static constexpr double MIN_GAP_SCORE_RATIO = 0.1;

    /// Minimum beam size for gap-based pruning to be applied at all.
    static constexpr uint32_t MIN_STATES_FOR_GAP_PRUNE = 3;

    /// Number of best-scored patterns returned by find_pattern.
    static constexpr uint32_t NUMBER_OF_STATES_TO_RETURN = 5;

    /// Minimum states allocated to each seed (ensures every seed gets at least one state).
    static constexpr uint32_t MIN_STATES_PER_SEED = 1;

    /// select_valid_seeds stops collecting when total matches exceed this multiple of beam size.
    static constexpr uint32_t SEED_MATCH_BUFFER_FACTOR = 2;

    uint32_t m_max_active_patterns;  ///< Hard cap on live beam states.
    double m_alpha_0;                ///< Initial alpha weight forwarded to histograms.
    double m_alpha_decay;            ///< Per-depth alpha decay forwarded to histograms.
    LoggerHandler m_logger;

    /**
     * @brief Intermediate result shared between allocate_proportionally and fill_remaining_quota.
     */
    struct ProportionalAllocation
    {
        std::vector<uint32_t> m_state_counts;
        uint32_t m_total_allocated = 0;
        std::vector<size_t> m_seeds_needing_more;
    };

    /**
     * @brief Score a single beam state under the null model.
     *
     * Delegates to PatternScorer::score.  For undirected patterns the BoostGraph
     * stores each edge twice (both directions), so edge count is halved.
     */
    static double score_state(const PatternState& state, double background_density,
                               bool is_directed);

    /**
     * @brief Extend @p state by absorbing one new S-graph vertex.
     *
     * Adds the vertex and its colour to the pattern, wires up all edges
     * between the new vertex and already-matched vertices, then calls
     * hist->absorb_vertex so that the histogram caches stay consistent.
     * Invalidates state.m_score_valid after modification.
     */
    void expand_one_state(PatternState& state, const CandidateVertex& candidate,
                          const ColoredGraph& search_graph, bool is_directed) const;

    /**
     * @brief Deep-copy a PatternState for beam branching.
     *
     * The histogram is deep-copied so each branch maintains independent
     * candidate caches.  The pattern BoostGraph is copied by value.
     * The destination state's m_score_valid is set to false.
     */
    static PatternState clone_state(const PatternState& source_state);

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
    static std::vector<uint32_t> select_seed_indices(uint32_t total_color_count,
                                                     uint32_t initial_beam_size);

    /**
     * @brief Allocate states proportionally then redistribute leftover quota.
     *
     * First pass (allocate_proportionally): distribute target_state_count states
     * proportional to inverse_probability_weight. Each seed gets at least
     * MIN_STATES_PER_SEED, capped by match capacity.
     * Second pass (fill_remaining_quota): redistribute remaining quota to seeds
     * with spare capacity, prioritising rarer colours first.
     *
     * @param seeds              Seed descriptors with weights and match lists.
     * @param target_state_count Desired total number of initial states.
     * @return Per-seed state counts (same length as seeds).
     */
    static std::vector<uint32_t> allocate_seed_states_improved(const std::vector<SeedInfo>& seeds,
                                                               uint32_t target_state_count);

    /**
     * @brief First pass of seed state allocation: distribute proportionally.
     *
     * @param seeds               Seed descriptors with weights and match lists.
     * @param total_weight        Sum of all seeds' inverse_probability_weight values.
     * @param target_state_count  Total states to distribute.
     * @return ProportionalAllocation with per-seed counts and spare-capacity bookkeeping.
     */
    static ProportionalAllocation allocate_proportionally(const std::vector<SeedInfo>& seeds,
                                                          double total_weight,
                                                          uint32_t target_state_count);

    /**
     * @brief Second pass of seed state allocation: redistribute leftover quota.
     *
     * Gives remaining quota to seeds with spare capacity, rarest first.
     *
     * @param alloc               Allocation from the first pass; updated in-place.
     * @param seeds               Same seeds passed to allocate_proportionally.
     * @param target_state_count  Original target; used to compute remaining quota.
     */
    static void fill_remaining_quota(ProportionalAllocation& alloc,
                                     const std::vector<SeedInfo>& seeds,
                                     uint32_t target_state_count);

    /**
     * @brief Select which colours to use as seeds for the initial beam.
     *
     * Iterates through colours in rarest-first order, collecting seeds until
     * accumulated match count reaches SEED_MATCH_BUFFER_FACTOR * initial_beam_size.
     *
     * @param sorted_colors_with_matches  (probability, color_id, match_count) tuples, ascending.
     * @param vertices_by_color           All S-vertices grouped by colour.
     * @param initial_beam_size           Target number of initial states.
     * @return SeedInfo list ready for allocation.
     */
    static std::vector<SeedInfo> select_valid_seeds(
        const std::vector<std::tuple<double, uint32_t, uint32_t>>& sorted_colors_with_matches,
        const std::vector<std::vector<uint32_t>>& vertices_by_color, uint32_t initial_beam_size);

    /**
     * @brief Immutable context bundle passed to beam-initialisation helpers.
     *
     * Avoids passing eight separate parameters through create_beam_from_seeds
     * and create_initial_state.  All reference fields must outlive the BeamContext.
     */
    struct BeamContext
    {
        const ColoredGraph& m_search_graph;              ///< S-graph (remapped colours).
        const std::vector<double>& m_color_probability;  ///< P(colour c) from background G.
        const std::vector<int32_t>& m_color_map;         ///< Compact id → original colour value.
        double m_background_log_density;                 ///< log(edge density of G).
        bool m_is_directed;                              ///< True when edges are directed.
    };

    /**
     * @brief Mutable state accumulated during the full beam search.
     *
     * Returned by initialise_beam_search and consumed by run_beam_expansion
     * and collect_best_patterns.
     */
    struct BeamSearchState
    {
        std::vector<int32_t> m_color_map;   ///< Compact id → original colour (for recolor).
        double m_background_density = 0.0;  ///< Raw edge density of G.
        std::vector<PatternState> m_beam;   ///< Live beam states.
    };

    /**
     * @brief Instantiate PatternStates from seed descriptors and allocation counts.
     *
     * For each seed, creates seed_state_counts[i] initial states, one per
     * distinct S-vertex match, each with its own SingleGraphHistogram.
     */
    std::vector<PatternState> create_beam_from_seeds(const std::vector<SeedInfo>& seeds,
                                                     const std::vector<uint32_t>& seed_state_counts,
                                                     const BeamContext& context) const;

    /**
     * @brief Create one PatternState seeded at a single S-graph vertex.
     *
     * Builds a fresh SingleGraphHistogram, absorbs the seed vertex,
     * creates a one-vertex BoostGraph pattern, and initialises
     * pattern_vertex_color_log_prob from the colour probability.
     */
    PatternState create_initial_state(const BeamContext& context, uint32_t color_id,
                                      uint32_t match_vertex) const;

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
    static uint32_t find_gap_cut(const std::vector<std::pair<double, uint32_t>>& scored_states);

    /**
     * @brief Return the indices of the NUMBER_OF_STATES_TO_RETURN best beam states.
     *
     * Uses cached m_beam_score when valid (m_score_valid == true), otherwise
     * recomputes.  Sorts ascending and returns the lowest-scoring state indices.
     *
     * @param beam               Live beam states.
     * @param background_density Edge density of the background graph G.
     * @param is_directed        Whether to treat patterns as directed.
     * @return Indices into beam of the best states (at most NUMBER_OF_STATES_TO_RETURN).
     */
    static std::vector<uint32_t> select_best_state(const std::vector<PatternState>& beam,
                                                   double background_density, bool is_directed);

    /**
     * @brief Check whether any beam state already beats the score threshold.
     *
     * Used to detect early termination: if any state's score is already
     * below the threshold, further expansion cannot improve the best result.
     */
    bool any_state_below_threshold(const std::vector<PatternState>& beam,
                                   double background_density, double threshold,
                                   bool is_directed) const;

    /**
     * @brief Build the initial beam from diverse seed colours.
     *
     * Selects seed colours spread across the rarity range, always including
     * the rarest.  Allocates m_max_active_patterns / INITIAL_BEAM_DIVISOR
     * total states so the expansion loop has room to grow the beam.
     */
    std::vector<PatternState> build_initial_beam(const ColoredGraph& search_graph,
                                                 const std::vector<double>& color_probability,
                                                 const std::vector<int32_t>& color_map,
                                                 double background_density, bool is_directed) const;

    /**
     * @brief Expand each live state by cloning for the top-K candidates.
     *
     * K = max(1, m_max_active_patterns / current_beam_size) so the beam
     * naturally fills toward m_max_active_patterns without exceeding it.
     * States with no candidates are moved unchanged into the new beam.
     *
     * @return true if at least one state was successfully expanded.
     */
    bool expand_beam(std::vector<PatternState>& beam, const ColoredGraph& search_graph,
                     bool is_directed) const;

    /**
     * @brief Drop weak beam states using a score-gap heuristic.
     *
     * Scores all states, caches each score in state.m_beam_score, and sets
     * state.m_score_valid = true.  For the first PRUNE_WARMUP_ITERATIONS
     * iterations only the hard size cap is enforced.  After warmup,
     * find_gap_cut determines how many to keep.  The hard cap
     * m_max_active_patterns is always applied.
     */
    void prune_beam(std::vector<PatternState>& beam, double background_density, uint32_t iteration,
                    bool is_directed) const;

    /**
     * @brief Remap colours in S and G and build the initial beam.
     *
     * Remaps vertex colours in both graphs to compact shared IDs via
     * PatternUtils::map_colors (modifies both graphs in-place), computes the
     * background colour distribution and edge density, then calls build_initial_beam.
     * Original colour values are preserved in BeamSearchState::color_map for
     * later restoration by collect_best_patterns.
     */
    BeamSearchState initialise_beam_search(ColoredGraph& search_graph,
                                           ColoredGraph& background_graph, bool is_directed) const;

    /**
     * @brief Run the expand-prune loop until a termination condition fires.
     *
     * Each iteration: expands the beam, prunes it, then checks the score
     * threshold.  Terminates when no further expansion is possible,
     * MAX_ITERATIONS is reached, or any state score falls below score_threshold.
     *
     * @param beam_state       Mutable beam (beam, color_map, background_density).
     * @param search_graph     Graph S (with remapped colours) to expand candidates in.
     * @param score_threshold  Stop early when any state's score beats this value.
     * @param is_directed      Whether to treat edges as directed.
     */
    void run_beam_expansion(BeamSearchState& beam_state, const ColoredGraph& search_graph,
                            double score_threshold, bool is_directed) const;

    /**
     * @brief Select the best patterns, restore original colours, and return them.
     *
     * Picks the top NUMBER_OF_STATES_TO_RETURN states (using cached scores where
     * available), remaps vertex colours back to original values via
     * beam_state.color_map, and moves the resulting BoostGraph patterns into the
     * return vector.
     */
    std::vector<BoostGraph> collect_best_patterns(BeamSearchState& beam_state,
                                                  bool is_directed) const;
};

}  // namespace sgf
