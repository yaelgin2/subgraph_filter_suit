#pragma once

#include "BoostGraph.h"
#include "ColoredGraph.h"
#include "LoggerHandler.h"
#include "PatternScorer.h"
#include "PatternState.h"

#include <cstdint>
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
 * @brief Finds the rarest subgraph pattern in a search graph using beam search.
 *
 * ### Algorithm overview
 *
 * 1. **Colour remapping** — vertex colours in the search graph S and the stored
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
 *
 * 4. **Expansion** — every live state asks its SingleGraphHistogram for the top-K
 *    candidate vertices, then clones itself for each candidate.
 *
 * 5. **Pruning** — after each expansion, states are scored and the weakest are
 *    dropped.  After a warmup period, a dynamic gap-based cut is applied.
 *
 * 6. **Termination** — the loop ends when no further expansion is possible,
 *    the safety iteration limit is reached, or any state's score falls below
 *    the caller's threshold.
 *
 * 7. **Output** — the top NUMBER_OF_STATES_TO_RETURN patterns by score are
 *    returned after their colours are remapped back to original values.
 *
 * The background graph G is supplied at construction; its edge density is
 * precomputed once and reused across all find_pattern calls.
 */
class SingleGraphPatternFinder
{
public:
    /**
     * @brief Construct the finder with a pre-remapped background graph.
     *
     * @param background_graph     Graph G that defines the null model, already remapped
     *                             to compact colour IDs by the caller.  Must be non-empty.
     * @param color_count          Number of distinct compact colour IDs in the remapped graphs.
     * @param is_directed          When true, treats edges as directed in all
     *                             subsequent find_pattern calls.
     * @param max_active_patterns  Hard cap on simultaneous live beam states.
     * @param alpha_0              Initial weight for the outside-neighbour score term
     *                             in SingleGraphHistogram.
     * @param alpha_decay          Per-depth multiplicative decay applied to alpha_0.
     * @param logger               Optional diagnostic logger.
     *
     * @throws InvalidArgumentException if background_graph has no vertices.
     */
    explicit SingleGraphPatternFinder(
        const ColoredGraph& background_graph, uint32_t color_count, bool is_directed,
        uint32_t max_active_patterns = DEFAULT_MAX_ACTIVE_PATTERNS, double alpha_0 = 1.0,
        double alpha_decay = DEFAULT_ALPHA_DECAY,
        LoggerHandler logger = LoggerHandler(std::weak_ptr<ILogger>{}));

    /**
     * @brief Find the rarest subgraph patterns in the search graph.
     *
     * The search graph must already be remapped to the same compact colour ID
     * space as the background graph (done once by the caller before any loop).
     *
     * @param search_graph    Pre-remapped graph S to search in.
     * @param score_threshold Stop as soon as any beam state's score falls below
     *                        this value.
     *
     * @return Up to NUMBER_OF_STATES_TO_RETURN BoostGraph patterns, sorted best first.
     *         Empty if no valid seed colours exist in S.
     *
     * @throws InvalidArgumentException if search_graph has no vertices.
     */
    std::vector<BoostGraph> find_pattern(const ColoredGraph& search_graph, double score_threshold);

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

    /// Minimum states allocated to each seed.
    static constexpr uint32_t MIN_STATES_PER_SEED = 1;

    /// select_valid_seeds stops collecting when total matches exceed this multiple of beam size.
    static constexpr uint32_t SEED_MATCH_BUFFER_FACTOR = 2;

    // ── Persistent state (set at construction) ────────────────────────────────

    bool m_is_directed;
    double m_background_density;  ///< Edge density of G, precomputed at construction.
    uint32_t m_max_active_patterns;
    double m_alpha_0;
    double m_alpha_decay;
    LoggerHandler m_logger;
    uint32_t m_color_count;                   ///< Number of distinct compact colour IDs.
    std::vector<double> m_color_probability;  ///< P(colour c) from background G.

    // ── Transient search state (populated by find_pattern, cleared on return) ─

    std::vector<PatternState> m_beam;

    // ── Internal helper types ─────────────────────────────────────────────────

    /**
     * @brief Intermediate result shared between allocate_proportionally and fill_remaining_quota.
     */
    struct ProportionalAllocation
    {
        std::vector<uint32_t> m_state_counts;
        uint32_t m_total_allocated = 0;
        std::vector<size_t> m_seeds_needing_more;
    };

    // ── Scoring ───────────────────────────────────────────────────────────────

    /**
     * @brief Score a single beam state under the null model.
     *
     * Uses m_background_density and m_is_directed from the object.
     * For undirected patterns the BoostGraph stores each edge twice, so
     * edge count is halved before passing to PatternScorer.
     */
    double score_state(const PatternState& state) const;

    // ── Beam state manipulation ───────────────────────────────────────────────

    /**
     * @brief Extend @p state by absorbing one new S-graph vertex.
     *
     * Adds the vertex and its colour, wires up all edges, calls
     * hist->absorb_vertex, and invalidates state.m_score_valid.
     */
    void expand_one_state(PatternState& state, const CandidateVertex& candidate,
                          const ColoredGraph& search_graph) const;

    /**
     * @brief Deep-copy a PatternState for beam branching.
     *
     * Sets destination.m_score_valid = false since the clone will be expanded.
     */
    static PatternState clone_state(const PatternState& source_state);

    // ── Seed selection and allocation ─────────────────────────────────────────

    /**
     * @brief Choose evenly-spaced indices into a sorted colour array.
     *
     * Always includes index 0 (rarest colour).
     */
    static std::vector<uint32_t> select_seed_indices(uint32_t total_color_count,
                                                     uint32_t initial_beam_size);

    /**
     * @brief Allocate states proportionally then redistribute leftover quota.
     *
     * First pass (allocate_proportionally): distribute states proportional to
     * inverse_probability_weight, capped by match capacity.
     * Second pass (fill_remaining_quota): redistribute remaining quota to seeds
     * with spare capacity, rarest first.
     */
    static std::vector<uint32_t> allocate_seed_states_improved(const std::vector<SeedInfo>& seeds,
                                                               uint32_t target_state_count);

    /**
     * @brief First pass: distribute states proportionally.
     */
    static ProportionalAllocation allocate_proportionally(const std::vector<SeedInfo>& seeds,
                                                          double total_weight,
                                                          uint32_t target_state_count);

    /**
     * @brief Second pass: redistribute leftover quota to rarest seeds with spare capacity.
     */
    static void fill_remaining_quota(ProportionalAllocation& alloc,
                                     const std::vector<SeedInfo>& seeds,
                                     uint32_t target_state_count);

    /**
     * @brief Select seed colours (rarest first) until total matches reach
     *        SEED_MATCH_BUFFER_FACTOR * initial_beam_size.
     */
    static std::vector<SeedInfo> select_valid_seeds(
        const std::vector<std::tuple<double, uint32_t, uint32_t>>& sorted_colors_with_matches,
        const std::vector<std::vector<uint32_t>>& vertices_by_color, uint32_t initial_beam_size);

    // ── Beam construction ─────────────────────────────────────────────────────

    /**
     * @brief Create PatternStates from seed descriptors and per-seed allocation counts.
     */
    std::vector<PatternState> create_beam_from_seeds(const std::vector<SeedInfo>& seeds,
                                                     const std::vector<uint32_t>& seed_state_counts,
                                                     const ColoredGraph& search_graph) const;

    /**
     * @brief Create one PatternState seeded at a single S-graph vertex.
     *
     * Uses m_color_probability, m_is_directed, m_alpha_0, m_alpha_decay, m_logger.
     */
    PatternState create_initial_state(const ColoredGraph& search_graph, uint32_t color_id,
                                      uint32_t match_vertex) const;

    // ── Gap detection ─────────────────────────────────────────────────────────

    /**
     * @brief Find the cut index in a sorted score list using gap detection.
     *
     * Returns scored_states.size() when no significant gap is found (keep all).
     */
    static uint32_t find_gap_cut(const std::vector<std::pair<double, uint32_t>>& scored_states);

    // ── Beam search steps ─────────────────────────────────────────────────────

    /**
     * @brief Return indices into m_beam of the NUMBER_OF_STATES_TO_RETURN best states.
     *
     * Uses m_beam_score when valid, otherwise recomputes via score_state.
     */
    std::vector<uint32_t> select_best_state() const;

    /**
     * @brief Check whether any beam state's score is already below @p threshold.
     */
    bool any_state_below_threshold(double threshold) const;

    /**
     * @brief Collect all colours present in the search graph and sort them by probability.
     *
     * Returns a vector of (probability, color_id, match_count) tuples sorted
     * ascending by probability so the rarest colour comes first.  If any colour
     * has zero or absent probability the vector is collapsed to that single entry
     * and returned immediately, since an absent colour is automatically the rarest.
     *
     * @param vertices_by_color Vertices grouped by compact colour id.
     * @return Sorted colour descriptors ready for select_valid_seeds.
     */
    std::vector<std::tuple<double, uint32_t, uint32_t>> collect_sorted_colors_with_matches(
        const std::vector<std::vector<uint32_t>>& vertices_by_color) const;

    /**
     * @brief Build m_beam from diverse seed colours.
     *
     * Uses m_color_probability, m_color_map, m_background_density, m_is_directed.
     */
    void build_initial_beam(const ColoredGraph& search_graph);

    /**
     * @brief Expand each live state by cloning for the top-K candidates.
     *
     * @return true if at least one state was successfully expanded.
     */
    bool expand_beam(const ColoredGraph& search_graph);

    /**
     * @brief Drop weak beam states using a score-gap heuristic.
     *
     * Caches each computed score in state.m_beam_score and sets
     * state.m_score_valid = true.
     */
    void prune_beam(uint32_t iteration);

    /**
     * @brief Run the expand-prune loop until a termination condition fires.
     */
    void run_beam_expansion(const ColoredGraph& search_graph, double score_threshold);

    /**
     * @brief Select the best patterns, restore original colours, and move them
     *        into the return vector.
     */
    std::vector<BoostGraph> collect_best_patterns();
};

}  // namespace sgf
