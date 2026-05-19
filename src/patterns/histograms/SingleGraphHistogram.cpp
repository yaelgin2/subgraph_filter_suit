#include "SingleGraphHistogram.h"
#include "HistogramOverflowException.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace sgf
{

/* ---------- Construction ---------- */

SingleGraphHistogram::SingleGraphHistogram( // NOLINT(readability-function-size)
    const ColoredGraph&        search_graph,
    const std::vector<double>& vertex_color_probabilities,
    double                     background_log_density,
    double                     initial_alpha_weight,
    double                     alpha_decay_rate,
    LoggerHandler              logger)
    : m_graph(search_graph)
    , m_logger(std::move(logger))
    , m_background_log_density(background_log_density)
    , m_initial_alpha_weight(initial_alpha_weight)
    , m_alpha_decay_rate(alpha_decay_rate)
    , m_color_log_probabilities(vertex_color_probabilities.size(), 0.0)
{
    const uint32_t search_graph_vertex_count = search_graph.vertex_count();
    // Reserve ~1% of vertices (capped at 512) to avoid frequent reallocs.
    const uint32_t candidate_reserve_hint =
        std::min<uint32_t>(search_graph_vertex_count / 100u, 512u);
    m_candidates.reserve(candidate_reserve_hint);
    m_candidate_match_neighbor_count.reserve(candidate_reserve_hint);
    m_candidate_outside_log_prob.reserve(candidate_reserve_hint);

    for (uint32_t color_index = 0; color_index < vertex_color_probabilities.size(); ++color_index)
    {
        m_color_log_probabilities[color_index] =
            (vertex_color_probabilities[color_index] > 0.0)
                ? std::log(vertex_color_probabilities[color_index])
                // Use half of lowest() rather than lowest() itself so that
                // adding two such values does not produce -inf or overflow.
                : std::numeric_limits<double>::lowest() * 0.5;
    }
}

/* ---------- Private helpers ---------- */

inline double SingleGraphHistogram::log_prob_of_vertex(uint32_t search_vertex) const
{
    const uint32_t vertex_color =
        static_cast<uint32_t>(m_graph.get_vertex_color(search_vertex));
    if (vertex_color >= m_color_log_probabilities.size())
        throw std::out_of_range(
            "SingleGraphHistogram::log_prob_of_vertex: color "
            + std::to_string(vertex_color) + " out of range (size "
            + std::to_string(m_color_log_probabilities.size()) + ")");
    return m_color_log_probabilities[vertex_color];
}

double SingleGraphHistogram::log_prob_of_color(uint32_t remapped_color_id) const
{
    if (remapped_color_id >= m_color_log_probabilities.size())
        throw std::out_of_range(
            "SingleGraphHistogram::log_prob_of_color: color "
            + std::to_string(remapped_color_id) + " out of range (size "
            + std::to_string(m_color_log_probabilities.size()) + ")");
    return m_color_log_probabilities[remapped_color_id];
}

/* ---------- absorb_vertex ---------- */

/**
 * @brief Absorb a vertex into the match set and update all candidate caches.
 *
 * Steps:
 * 1. Insert vertex into the match set.
 * 2. Remove it from every candidate data structure.
 * 3. Walk its neighbours: register new candidates or update existing ones.
 */
void SingleGraphHistogram::absorb_vertex(uint32_t vertex_to_absorb, bool is_directed)
{
    m_match_vertices.insert(vertex_to_absorb);

    m_candidates.erase(vertex_to_absorb);
    m_candidate_match_neighbor_count.erase(vertex_to_absorb);
    m_candidate_outside_log_prob.erase(vertex_to_absorb);

    add_all_vertex_neighbours_to_candidate(vertex_to_absorb, false);
    if (is_directed)
    {
        add_all_vertex_neighbours_to_candidate(vertex_to_absorb, true);
    }

    ++m_current_depth;
}

// Walk every neighbour of absorbed_vertex (in the forward or reverse direction) and
// register each non-matched neighbour as a candidate, or update its caches if it is
// already a candidate.  Called twice for directed graphs (forward + reverse) so that
// vertices reachable via either direction are considered for the next expansion.
void SingleGraphHistogram::add_all_vertex_neighbours_to_candidate(
    uint32_t absorbed_vertex, bool is_reversed)
{
    auto [neighbor_begin, neighbor_end] = m_graph.get_neighbours(absorbed_vertex, is_reversed);
    for (auto neighbor_iterator = neighbor_begin; neighbor_iterator != neighbor_end; ++neighbor_iterator)
    {
        const uint32_t neighbor_vertex = *neighbor_iterator;
        if (m_match_vertices.find(neighbor_vertex) != m_match_vertices.end())
            continue;
        add_vertex_neighbour_to_candidate(neighbor_vertex, absorbed_vertex);
    }
}

// Register candidate_vertex as a new candidate or update its caches now that
// absorbed_vertex (a newly matched vertex) is its neighbour.
//
// Two cases:
//   New candidate     — insert into all four candidate maps; compute outside-log-prob
//                       by scanning all current neighbours of candidate_vertex.
//   Existing candidate — increment match-neighbour count; subtract absorbed_vertex's
//                        log-prob from the outside cache (it moved from outside to inside).
void SingleGraphHistogram::add_vertex_neighbour_to_candidate(
    uint32_t candidate_vertex, uint32_t absorbed_vertex)
{
    auto degree_iterator = m_candidate_match_neighbor_count.find(candidate_vertex);
    if (degree_iterator == m_candidate_match_neighbor_count.end())
    {
        // Brand-new candidate: register and compute its full outside-log-prob.
        m_candidates.insert(candidate_vertex);
        m_candidate_match_neighbor_count[candidate_vertex] = 1;

        double outside_log_prob_sum = 0.0;
        auto [neighbor_begin, neighbor_end] = m_graph.get_neighbours(candidate_vertex, false);
        for (auto neighbor_iterator = neighbor_begin; neighbor_iterator != neighbor_end; ++neighbor_iterator)
        {
            if (m_match_vertices.find(*neighbor_iterator) == m_match_vertices.end())
                outside_log_prob_sum += log_prob_of_vertex(*neighbor_iterator);
        }
        if (std::isinf(outside_log_prob_sum))
            throw HistogramOverflowException(
                "outside_logp sum reached infinity for candidate vertex "
                + std::to_string(candidate_vertex));
        m_candidate_outside_log_prob[candidate_vertex] = outside_log_prob_sum;
    }
    else
    {
        // Existing candidate: increment match-neighbour count and remove
        // absorbed_vertex's contribution from the outside-log-prob cache.
        ++(degree_iterator->second);
        auto outside_logp_iterator = m_candidate_outside_log_prob.find(candidate_vertex);
        if (outside_logp_iterator != m_candidate_outside_log_prob.end())
            outside_logp_iterator->second -= log_prob_of_vertex(absorbed_vertex);
    }
}

/* ---------- compute_candidate_score ---------- */

// Returns nullopt for floating candidates (no match neighbour) at depth > 0 —
// they are unreachable from the current match and should not be expanded next.
// Throws HistogramOverflowException if the computed score diverges to infinity.
std::optional<double> SingleGraphHistogram::compute_candidate_score(
    uint32_t candidate_vertex, double current_alpha_weight) const
{
    const auto degree_iterator = m_candidate_match_neighbor_count.find(candidate_vertex);
    const uint32_t match_neighbor_count =
        (degree_iterator != m_candidate_match_neighbor_count.end())
            ? degree_iterator->second : 0u;
    // Skip floating candidates (no connection to the current match) except at depth 0
    // where the first vertex has no match neighbours by definition.
    if (match_neighbor_count == 0 && m_current_depth > 0)
        return std::nullopt;

    const auto outside_logp_iterator = m_candidate_outside_log_prob.find(candidate_vertex);
    if (outside_logp_iterator == m_candidate_outside_log_prob.end())
        throw std::runtime_error(
            "SingleGraphHistogram::compute_candidate_score: outside_logp cache "
            "missing for candidate " + std::to_string(candidate_vertex));

    // Score formula:
    //   match_neighbor_count * log(density)  — reward edges to the existing match
    //   + log(p[color(v)])                    — penalise common colours
    //   + alpha * outside_log_prob            — exploration: prefer rare unmatched neighbours
    const double candidate_score =
        static_cast<double>(match_neighbor_count) * m_background_log_density
        + log_prob_of_vertex(candidate_vertex)
        + current_alpha_weight * outside_logp_iterator->second;

    if (std::isinf(candidate_score))
        throw HistogramOverflowException(
            "candidate score reached infinity for vertex "
            + std::to_string(candidate_vertex));

    return candidate_score;
}

} // namespace sgf

namespace
{

// Partially sort @p candidates so the best result_count entries are in ascending
// score order at the front, then resize to result_count.
// partial_sort is cheaper than full sort when only the top-k entries are needed.
void select_top_k_candidates(std::vector<sgf::CandidateVertex>& candidates,
                              uint32_t                           result_count)
{
    std::partial_sort(
        candidates.begin(),
        candidates.begin() + static_cast<std::ptrdiff_t>(result_count),
        candidates.end(),
        [](const sgf::CandidateVertex& first, const sgf::CandidateVertex& second)
        {
            return first.score < second.score;
        });
    candidates.resize(result_count);
}

} // anonymous namespace

namespace sgf
{

/* ---------- get_top_k_vertices ---------- */

std::vector<CandidateVertex> SingleGraphHistogram::get_top_k_vertices(uint32_t max_result_count)
{
    if (m_candidates.empty() || max_result_count == 0)
        return {};

    const double current_alpha_weight =
        m_initial_alpha_weight
        * std::pow(m_alpha_decay_rate, static_cast<double>(m_current_depth));

    std::vector<CandidateVertex> scored_candidates;
    scored_candidates.reserve(m_candidates.size());

    for (uint32_t candidate_vertex : m_candidates)
    {
        const std::optional<double> candidate_score =
            compute_candidate_score(candidate_vertex, current_alpha_weight);
        if (!candidate_score.has_value())
            continue;
        scored_candidates.push_back({static_cast<int32_t>(candidate_vertex), *candidate_score});
    }

    if (scored_candidates.empty())
        return {};

    const uint32_t result_count =
        std::min(max_result_count, static_cast<uint32_t>(scored_candidates.size()));
    select_top_k_candidates(scored_candidates, result_count);
    return scored_candidates;
}

} // namespace sgf
