#include "SingleGraphPatternFinder.h"

#include "BoostGraph.h"
#include "ColoredGraph.h"
#include "InvalidArgumentException.h"
#include "LogLevel.h"
#include "LoggerHandler.h"
#include "PatternScorer.h"
#include "PatternState.h"
#include "PatternUtils.h"
#include "SingleGraphHistogram.h"
#include "DebugLog.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace sgf
{

/* ---------- Construction ---------- */

// NOLINTNEXTLINE(readability-function-size)
SingleGraphPatternFinder::SingleGraphPatternFinder(ColoredGraph background_graph, bool is_directed,
                                                   uint32_t max_active_patterns, double alpha_0,
                                                   double alpha_decay, LoggerHandler logger)
    : m_background_graph(std::move(background_graph))
    , m_is_directed(is_directed)
    , m_background_density(PatternUtils::compute_density(m_background_graph.vertex_count(),
                                                         m_background_graph.edge_count()))
    , m_max_active_patterns(max_active_patterns)
    , m_alpha_0(alpha_0)
    , m_alpha_decay(alpha_decay)
    , m_logger(std::move(logger))
{
    if (m_background_graph.vertex_count() == 0)
    {
        throw InvalidArgumentException("G has no nodes.");
    }
}

/* ---------- score_state ---------- */

double SingleGraphPatternFinder::score_state(const PatternState& state) const
{
    const uint32_t vertex_count = boost::num_vertices(state.m_pattern);
    // BoostGraph stores both directions for undirected edges, so halve the count
    // to get the true undirected edge count passed to PatternScorer.
    const uint32_t edge_count = m_is_directed ? boost::num_edges(state.m_pattern)
                                              : boost::num_edges(state.m_pattern) / 2;

    return PatternScorer::score(state.m_pattern_vertex_color_log_prob, edge_count,
                                m_background_density, vertex_count);
}

/* ---------- expand_one_state ---------- */

void SingleGraphPatternFinder::expand_one_state(PatternState& state,
                                                const CandidateVertex& candidate,
                                                const ColoredGraph& search_graph) const
{
    const uint32_t new_search_vertex = static_cast<uint32_t>(candidate.m_search_vertex);
    const uint32_t vertex_color =
        static_cast<uint32_t>(search_graph.get_vertex_color(new_search_vertex));

    SGF_DEBUG_LOG(m_logger, "[EXPAND] selected_vertex=" + std::to_string(new_search_vertex) +
            " color=" + std::to_string(vertex_color) + " current_pattern_size=" +
            std::to_string(boost::num_vertices(state.m_pattern)) + " current_edges=" +
            std::to_string(boost::num_edges(state.m_pattern) / (m_is_directed ? 1U : 2U)) +
            " current_color_logp=" + std::to_string(state.m_pattern_vertex_color_log_prob));

    const uint32_t new_pattern_node =
        boost::add_vertex(VertexProperties{vertex_color}, state.m_pattern);

    state.m_pattern_vertex_color_log_prob += state.m_hist->log_prob_of_color(vertex_color);

    // Wire up edges between the new pattern node and all already-matched nodes.
    for (uint32_t match_index = 0; match_index < state.m_match_path.size(); ++match_index)
    {
        if (search_graph.is_edge(new_search_vertex, state.m_match_path[match_index]))
        {
            PatternUtils::add_edge(m_is_directed, state.m_pattern, new_pattern_node, match_index);
        }
        if (m_is_directed &&
            search_graph.is_edge(state.m_match_path[match_index], new_search_vertex))
        {
            PatternUtils::add_edge(m_is_directed, state.m_pattern, match_index, new_pattern_node);
        }
    }

    state.m_hist->absorb_vertex(new_search_vertex, m_is_directed);
    state.m_match_path.push_back(new_search_vertex);
    state.m_score_valid = false;
}

/* ---------- clone_state ---------- */

// Deep copy for beam branching: histogram must be deep-copied because its
// incremental caches are specific to the match path and diverge between branches.
PatternState SingleGraphPatternFinder::clone_state(const PatternState& source_state)
{
    PatternState destination_state;
    destination_state.m_pattern = source_state.m_pattern;
    destination_state.m_hist = std::make_unique<SingleGraphHistogram>(*source_state.m_hist);
    destination_state.m_match_path = source_state.m_match_path;
    destination_state.m_beam_score = source_state.m_beam_score;
    destination_state.m_score_valid = false;
    destination_state.m_pattern_vertex_color_log_prob =
        source_state.m_pattern_vertex_color_log_prob;
    return destination_state;
}

/* ---------- select_seed_indices ---------- */

std::vector<uint32_t> SingleGraphPatternFinder::select_seed_indices(uint32_t total_color_count,
                                                                    uint32_t initial_beam_size)
{
    const uint32_t seed_count =
        std::min(total_color_count, std::max(MIN_SEED_COLORS, initial_beam_size / STATES_PER_SEED));

    std::vector<uint32_t> seed_indices;
    seed_indices.reserve(seed_count);
    seed_indices.push_back(0);

    if (seed_count > 1 && total_color_count > 1)
    {
        const uint32_t denominator = seed_count - 1U;  // safe: seed_count > 1
        const double index_step =
            static_cast<double>(total_color_count - 1U) / static_cast<double>(denominator);
        for (uint32_t seed_index = 1; seed_index < seed_count; ++seed_index)
        {
            seed_indices.push_back(static_cast<uint32_t>(std::round(seed_index * index_step)));
        }
    }

    return seed_indices;
}

/* ---------- allocate_proportionally ---------- */

SingleGraphPatternFinder::ProportionalAllocation
SingleGraphPatternFinder::allocate_proportionally(const std::vector<SeedInfo>& seeds,
                                                  double total_weight,
                                                  uint32_t target_state_count)
{
    ProportionalAllocation result;
    result.m_state_counts.resize(seeds.size(), 0);
    for (size_t seed_index = 0; seed_index < seeds.size(); ++seed_index)
    {
        const double raw_allocation = static_cast<double>(target_state_count) *
                                      seeds[seed_index].m_inverse_probability_weight / total_weight;
        const uint32_t match_capacity =
            static_cast<uint32_t>(seeds[seed_index].m_vertex_matches.size());
        const uint32_t allocation =
            std::max(MIN_STATES_PER_SEED,
                     std::min(static_cast<uint32_t>(std::round(raw_allocation)), match_capacity));
        result.m_state_counts[seed_index] = allocation;
        result.m_total_allocated += allocation;
        if (allocation < match_capacity)
        {
            result.m_seeds_needing_more.push_back(seed_index);
        }
    }
    return result;
}

/* ---------- fill_remaining_quota ---------- */

void SingleGraphPatternFinder::fill_remaining_quota(ProportionalAllocation& alloc,
                                                    const std::vector<SeedInfo>& seeds,
                                                    uint32_t target_state_count)
{
    if (alloc.m_total_allocated >= target_state_count || alloc.m_seeds_needing_more.empty())
    {
        return;
    }

    uint32_t states_needed = target_state_count - alloc.m_total_allocated;
    std::sort(alloc.m_seeds_needing_more.begin(), alloc.m_seeds_needing_more.end(),
              [&](const size_t left_index, const size_t right_index)
              {
                  return seeds[left_index].m_inverse_probability_weight >
                         seeds[right_index].m_inverse_probability_weight;
              });

    for (const size_t seed_index : alloc.m_seeds_needing_more)
    {
        if (states_needed == 0)
        {
            break;
        }
        const uint32_t match_capacity =
            static_cast<uint32_t>(seeds[seed_index].m_vertex_matches.size());
        const uint32_t additional_capacity = match_capacity - alloc.m_state_counts[seed_index];
        const uint32_t states_to_add = std::min(additional_capacity, states_needed);
        alloc.m_state_counts[seed_index] += states_to_add;
        states_needed -= states_to_add;
    }
}

/* ---------- allocate_seed_states_improved ---------- */

std::vector<uint32_t>
SingleGraphPatternFinder::allocate_seed_states_improved(const std::vector<SeedInfo>& seeds,
                                                        uint32_t target_state_count)
{
    if (seeds.empty())
    {
        return {};
    }

    double total_weight = 0.0;
    for (const SeedInfo& seed : seeds)
    {
        total_weight += seed.m_inverse_probability_weight;
    }

    ProportionalAllocation alloc = allocate_proportionally(seeds, total_weight, target_state_count);
    fill_remaining_quota(alloc, seeds, target_state_count);
    return alloc.m_state_counts;
}

/* ---------- select_valid_seeds ---------- */

std::vector<SeedInfo> SingleGraphPatternFinder::select_valid_seeds(
    const std::vector<std::tuple<double, uint32_t, uint32_t>>& sorted_colors_with_matches,
    const std::vector<std::vector<uint32_t>>& vertices_by_color, uint32_t initial_beam_size)
{
    std::vector<SeedInfo> seeds;
    uint32_t total_match_count = 0;

    for (const std::tuple<double, uint32_t, uint32_t>& color_data : sorted_colors_with_matches)
    {
        const double color_probability = std::get<0>(color_data);
        const uint32_t color_identifier = std::get<1>(color_data);
        const uint32_t vertex_match_count = std::get<2>(color_data);

        seeds.push_back({color_identifier, color_probability, vertices_by_color[color_identifier],
                         1.0 / color_probability});
        total_match_count += vertex_match_count;

        if (static_cast<uint64_t>(total_match_count) >=
            static_cast<uint64_t>(initial_beam_size) * SEED_MATCH_BUFFER_FACTOR)
        {
            break;
        }
    }

    return seeds;
}

/* ---------- create_beam_from_seeds ---------- */

std::vector<PatternState>
SingleGraphPatternFinder::create_beam_from_seeds(const std::vector<SeedInfo>& seeds,
                                                 const std::vector<uint32_t>& seed_state_counts,
                                                 const ColoredGraph& search_graph) const
{
    std::vector<PatternState> beam;
    for (size_t seed_index = 0; seed_index < seeds.size(); ++seed_index)
    {
        SGF_DEBUG_LOG(m_logger, "Seed colour " +
                         std::to_string(m_color_map[seeds[seed_index].m_color_id]) +
                         " (p=" + std::to_string(seeds[seed_index].m_probability) +
                         ")  matches=" + std::to_string(seeds[seed_index].m_vertex_matches.size()) +
                         "  keeping=" + std::to_string(seed_state_counts[seed_index]));

        for (uint32_t match_index = 0; match_index < seed_state_counts[seed_index]; ++match_index)
        {
            beam.push_back(create_initial_state(search_graph, seeds[seed_index].m_color_id,
                                                seeds[seed_index].m_vertex_matches[match_index]));
        }
    }
    return beam;
}

/* ---------- create_initial_state ---------- */

PatternState SingleGraphPatternFinder::create_initial_state(const ColoredGraph& search_graph,
                                                            uint32_t color_id,
                                                            uint32_t match_vertex) const
{
    const double background_log_density =
        (m_background_density > 0.0) ? std::log(m_background_density) : 0.0;

    std::unique_ptr<SingleGraphHistogram> histogram = std::make_unique<SingleGraphHistogram>(
        search_graph, m_color_probability, background_log_density, m_alpha_0, m_alpha_decay,
        m_logger);
    histogram->absorb_vertex(match_vertex, m_is_directed);

    BoostGraph pattern;
    boost::add_vertex(VertexProperties{color_id}, pattern);

    PatternState state;
    state.m_pattern = pattern;
    state.m_hist = std::move(histogram);
    state.m_match_path = {match_vertex};
    state.m_beam_score = 0.0;
    state.m_score_valid = false;
    state.m_pattern_vertex_color_log_prob = state.m_hist->log_prob_of_color(color_id);
    return state;
}

/* ---------- find_gap_cut ---------- */

uint32_t SingleGraphPatternFinder::find_gap_cut(
    const std::vector<std::pair<double, uint32_t>>& scored_states)
{
    const uint32_t state_count = static_cast<uint32_t>(scored_states.size());
    const uint32_t minimum_keep_count =
        std::max(MIN_STATES_FOR_GAP_PRUNE,
                 static_cast<uint32_t>(std::ceil(state_count * MIN_KEEP_FRACTION)));
    const double score_range = scored_states.back().first - scored_states.front().first;

    if ((state_count < MIN_STATES_FOR_GAP_PRUNE) || 
        (minimum_keep_count >= state_count) ||
        (score_range <= 0.0))
    {
        return state_count;
    }

    double largest_gap = 0.0;
    uint32_t gap_cut_position = state_count;
    for (uint32_t state_index = minimum_keep_count; state_index < state_count; ++state_index)
    {
        const double current_gap =
            scored_states[state_index].first - scored_states[state_index - 1].first;
        if (current_gap > largest_gap)
        {
            largest_gap = current_gap;
            gap_cut_position = state_index;
        }
    }

    if (largest_gap >= MIN_GAP_SCORE_RATIO * score_range)
    {
        return gap_cut_position;
    }

    return state_count;
}

/* ---------- select_best_state ---------- */

std::vector<uint32_t> SingleGraphPatternFinder::select_best_state() const
{
    std::vector<std::pair<double, uint32_t>> scored_states;
    scored_states.reserve(m_beam.size());
    for (uint32_t state_index = 0; state_index < static_cast<uint32_t>(m_beam.size());
         ++state_index)
    {
        const double score = m_beam[state_index].m_score_valid ? m_beam[state_index].m_beam_score
                                                               : score_state(m_beam[state_index]);
        scored_states.emplace_back(score, state_index);
    }
    std::sort(scored_states.begin(), scored_states.end());

    std::vector<uint32_t> best_indices;
    best_indices.reserve(NUMBER_OF_STATES_TO_RETURN);
    for (uint32_t result_index = 0; result_index < NUMBER_OF_STATES_TO_RETURN &&
                                    result_index < static_cast<uint32_t>(scored_states.size());
         ++result_index)
    {
        best_indices.push_back(scored_states[result_index].second);
    }
    return best_indices;
}

/* ---------- any_state_below_threshold ---------- */

bool SingleGraphPatternFinder::any_state_below_threshold(const double threshold) const
{
    return std::any_of(m_beam.begin(), m_beam.end(), [this, threshold](const PatternState& state) {
        const double state_score =
            state.m_score_valid ? state.m_beam_score : score_state(state);
        return state_score < threshold;
    });
}

/* ---------- build_initial_beam ---------- */

void SingleGraphPatternFinder::build_initial_beam(const ColoredGraph& search_graph)
{
    const uint32_t initial_beam_size = m_max_active_patterns / INITIAL_BEAM_DIVISOR;

    const std::vector<std::vector<uint32_t>> vertices_by_color =
        PatternUtils::get_all_color_matches(search_graph, static_cast<uint32_t>(m_color_map.size()));

    std::vector<std::tuple<double, uint32_t, uint32_t>> sorted_colors_with_matches;
    for (uint32_t color_id = 0; color_id < static_cast<uint32_t>(m_color_map.size()); ++color_id)
    {
        if (!vertices_by_color[color_id].empty())
        {
            const bool is_absent_or_zero =
                (color_id >= static_cast<uint32_t>(m_color_probability.size())) ||
                (m_color_probability[color_id] == 0.0);
            if (is_absent_or_zero)
            {
                const double absent_prob =
                    (color_id < static_cast<uint32_t>(m_color_probability.size()))
                        ? m_color_probability[color_id]
                        : 0.0;
                sorted_colors_with_matches = {
                    {absent_prob, color_id,
                     static_cast<uint32_t>(vertices_by_color[color_id].size())}};
                break;
            }
            sorted_colors_with_matches.emplace_back(
                m_color_probability[color_id], color_id,
                static_cast<uint32_t>(vertices_by_color[color_id].size()));
        }
    }

    std::sort(sorted_colors_with_matches.begin(), sorted_colors_with_matches.end(),
              [](const std::tuple<double, uint32_t, uint32_t>& first_color,
                 const std::tuple<double, uint32_t, uint32_t>& second_color)
              {
                  return std::get<0>(first_color) < std::get<0>(second_color);
              });

    const std::vector<SeedInfo> seeds =
        select_valid_seeds(sorted_colors_with_matches, vertices_by_color, initial_beam_size);
    if (seeds.empty())
    {
        m_beam.clear();
        return;
    }

    const std::vector<uint32_t> seed_state_counts =
        allocate_seed_states_improved(seeds, initial_beam_size);

    m_beam = create_beam_from_seeds(seeds, seed_state_counts, search_graph);
}

/* ---------- expand_beam ---------- */

bool SingleGraphPatternFinder::expand_beam(const ColoredGraph& search_graph)
{
    bool any_expanded = false;
    const uint32_t current_beam_size = static_cast<uint32_t>(m_beam.size());
    const uint32_t branching_factor =
        std::max(1U, m_max_active_patterns / std::max(1U, current_beam_size));

    std::vector<PatternState> expanded_beam;
    expanded_beam.reserve(static_cast<size_t>(current_beam_size) * branching_factor);

    for (PatternState& state : m_beam)
    {
        std::vector<CandidateVertex> candidates =
            state.m_hist->get_top_k_vertices(branching_factor);
        if (candidates.empty())
        {
            expanded_beam.push_back(std::move(state));
            continue;
        }

        any_expanded = true;
        for (size_t candidate_index = 0; candidate_index + 1 < candidates.size(); ++candidate_index)
        {
            PatternState cloned_state = clone_state(state);
            expand_one_state(cloned_state, candidates[candidate_index], search_graph);
            expanded_beam.push_back(std::move(cloned_state));
        }
        expand_one_state(state, candidates.back(), search_graph);
        expanded_beam.push_back(std::move(state));
    }

    m_beam = std::move(expanded_beam);
    return any_expanded;
}

/* ---------- prune_beam ---------- */

void SingleGraphPatternFinder::prune_beam(uint32_t iteration)
{
    if (m_beam.size() <= 1)
    {
        return;
    }

    std::vector<std::pair<double, uint32_t>> scored_states;
    scored_states.reserve(m_beam.size());
    for (uint32_t state_index = 0; state_index < static_cast<uint32_t>(m_beam.size());
         ++state_index)
    {
        const double score = score_state(m_beam[state_index]);
        m_beam[state_index].m_beam_score = score;
        m_beam[state_index].m_score_valid = true;
        scored_states.emplace_back(score, state_index);
    }
    std::sort(scored_states.begin(), scored_states.end());

    uint32_t keep_count = static_cast<uint32_t>(scored_states.size());
    if (iteration >= PRUNE_WARMUP_ITERATIONS)
    {
        keep_count = std::min(keep_count, find_gap_cut(scored_states));
    }
    keep_count = std::min(keep_count, m_max_active_patterns);

    if (keep_count >= static_cast<uint32_t>(m_beam.size()))
    {
        return;
    }

    std::vector<PatternState> kept_states;
    kept_states.reserve(keep_count);
    for (uint32_t result_index = 0; result_index < keep_count; ++result_index)
    {
        kept_states.push_back(std::move(m_beam[scored_states[result_index].second]));
    }
    m_beam = std::move(kept_states);
}

/* ---------- initialise_beam_search ---------- */

void SingleGraphPatternFinder::initialise_beam_search(ColoredGraph& search_graph)
{
    // Make a working copy of the background graph for joint colour remapping.
    // The stored m_background_graph is never modified so subsequent find_pattern
    // calls always start from the original colours.
    ColoredGraph background_working = m_background_graph;
    m_color_map = PatternUtils::map_colors(search_graph, background_working);
    m_color_probability = PatternUtils::compute_color_distribution(
        static_cast<uint32_t>(m_color_map.size()), background_working);
    build_initial_beam(search_graph);
}

/* ---------- run_beam_expansion ---------- */

void SingleGraphPatternFinder::run_beam_expansion(const ColoredGraph& search_graph,
                                                  double score_threshold)
{
    uint32_t iteration = 0;
    bool threshold_reached = any_state_below_threshold(score_threshold);

    while (iteration < MAX_ITERATIONS && !threshold_reached)
    {
        SGF_DEBUG_LOG(m_logger, "Attempt expansion.");
        if (!expand_beam(search_graph))
        {
            SGF_DEBUG_LOG(m_logger, "No more expansions possible at iteration " + std::to_string(iteration));
            break;
        }

        prune_beam(iteration);

        if (any_state_below_threshold(score_threshold))
        {
            threshold_reached = true;
        }

        ++iteration;
    }
}

/* ---------- collect_best_patterns ---------- */

std::vector<BoostGraph> SingleGraphPatternFinder::collect_best_patterns()
{
    const std::vector<uint32_t> best_indices = select_best_state();

    for (const uint32_t state_index : best_indices)
    {
        PatternState& state = m_beam[state_index];
        SGF_DEBUG_LOG(m_logger, "Selected pattern with score: " +
                         std::to_string(score_state(state)));
        PatternUtils::recolor_pattern(state.m_pattern, m_color_map);
    }

    std::vector<BoostGraph> result;
    result.reserve(best_indices.size());
    for (const uint32_t state_index : best_indices)
    {
        result.push_back(std::move(m_beam[state_index].m_pattern));
    }
    return result;
}

/* ---------- find_pattern ---------- */

std::vector<BoostGraph> SingleGraphPatternFinder::find_pattern(ColoredGraph& search_graph,
                                                               double score_threshold)
{
    if (search_graph.vertex_count() == 0)
    {
        throw InvalidArgumentException("S has no nodes.");
    }

    const std::chrono::high_resolution_clock::time_point start_time =
        std::chrono::high_resolution_clock::now();

    SGF_DEBUG_LOG(m_logger, "Initiating beam.");
    initialise_beam_search(search_graph);
    if (m_beam.empty())
    {
        m_logger.log(LogLevel::WARNING, "SingleGraphPatternFinder: no valid seed.");
        m_color_map.clear();
        m_color_probability.clear();
        return {};
    }

    run_beam_expansion(search_graph, score_threshold);

    const std::chrono::high_resolution_clock::time_point end_time =
        std::chrono::high_resolution_clock::now();
   SGF_DEBUG_LOG(m_logger, "Total pattern finding time: " +
                     std::to_string(std::chrono::duration<double>(end_time - start_time).count()) +
                     " seconds");

    std::vector<BoostGraph> result = collect_best_patterns();

    m_beam.clear();
    m_color_map.clear();
    m_color_probability.clear();

    return result;
}

}  // namespace sgf
