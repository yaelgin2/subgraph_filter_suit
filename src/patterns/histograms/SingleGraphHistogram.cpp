#include "SingleGraphHistogram.h"
#include <algorithm>
#include <cmath>
#include <limits>

/* ---------- Construction ---------- */

SingleGraphHistogram::SingleGraphHistogram(
    const sgf::ColoredGraph&   s_graph,
    const std::vector<double>& color_prob,
    double                     log_density,
    double                     alpha_0,
    double                     decay)
    : m_graph(s_graph)
    , m_log_density(log_density)
    , m_alpha_0(alpha_0)
    , m_decay(decay)
    , m_color_log_prob(color_prob.size(), 0.0)
{
    const uint32_t s_graph_vertex_number = s_graph.vertex_count();
    // estimates 1% tops (maximum 512)
    const uint32_t reserve_hint =
        std::min<uint32_t>(s_graph_vertex_number / 100u, 512u);
    m_candidates.reserve(reserve_hint);
    m_candidate_in_degree.reserve(reserve_hint);
    m_candidate_any_parent.reserve(reserve_hint);
    m_candidate_outside_logp.reserve(reserve_hint);

    for (uint32_t color_index = 0; color_index < color_prob.size(); ++color_index) {
        m_color_log_prob[color_index] =
            (color_prob[color_index] > 0.0)
                ? std::log(color_prob[color_index])
                : std::numeric_limits<double>::lowest() * 0.5;
    }
}

/* ---------- Private helpers ---------- */

inline double SingleGraphHistogram::log_prob_of_vertex(uint32_t s_vertex) const
{
    const uint32_t color =
        static_cast<uint32_t>(m_graph.get_vertex_color(s_vertex));
    if (color >= m_color_log_prob.size())
        throw std::out_of_range(
            "SingleGraphHistogram::log_prob_of_vertex: color "
            + std::to_string(color) + " out of range (size "
            + std::to_string(m_color_log_prob.size()) + ")");
    return m_color_log_prob[color];
}

double SingleGraphHistogram::log_prob_of_color(uint32_t remapped_color) const
{
    if (remapped_color >= m_color_log_prob.size())
        throw std::out_of_range(
            "SingleGraphHistogram::log_prob_of_color: color "
            + std::to_string(remapped_color) + " out of range (size "
            + std::to_string(m_color_log_prob.size()) + ")");
    return m_color_log_prob[remapped_color];
}

/* ---------- absorb_vertex ---------- */
/**
 * @brief Absorbs a vertex into the current match set and updates candidate structures.
 * 
 * This function performs the following operations:
 * 1. Adds the specified vertex to the match set.
 * 2. Removes the vertex from candidate structures.
 * 3. Processes the neighbors of the absorbed vertex to update candidate information.
 * 
 * @param s_vertex The vertex to absorb into the match set.
 */
void SingleGraphHistogram::absorb_vertex(uint32_t s_vertex, bool directed)
{
    // 1. Add to match set.
    m_match_vertices.insert(s_vertex);

    // 2. Remove from candidate structures.
    m_candidates.erase(s_vertex);
    m_candidate_in_degree.erase(s_vertex);
    m_candidate_any_parent.erase(s_vertex);
    m_candidate_outside_logp.erase(s_vertex);

    // 3. Process neighbours of the newly absorbed vertex.
    add_all_vertex_neighbours_to_candidate(s_vertex, false);
    if (directed)
    {
        add_all_vertex_neighbours_to_candidate(s_vertex, true);
    }

    ++m_current_depth;
}

void SingleGraphHistogram::add_all_vertex_neighbours_to_candidate(uint32_t absorbed_vertex, bool is_reversed)
{
    auto [nb_begin, nb_end] = m_graph.get_neighbours(absorbed_vertex, is_reversed);
    for (auto it = nb_begin; it != nb_end; ++it) {
        const uint32_t u = *it;
        if (m_match_vertices.find(u) != m_match_vertices.end()) continue;
        add_vertex_neighbour_to_candidate(u, absorbed_vertex);
    }
}

void SingleGraphHistogram::add_vertex_neighbour_to_candidate(uint32_t vertex, uint32_t absorbed_vertex)
{
    auto deg_it = m_candidate_in_degree.find(vertex);
    if (deg_it == m_candidate_in_degree.end()) {
        // Brand-new candidate.
        m_candidates.insert(vertex);
        m_candidate_in_degree[vertex] = 1;
        m_candidate_any_parent[vertex] = absorbed_vertex;

        // Compute full outside-logp by scanning u's neighbours.
        double outside_sum = 0.0;
        auto [nb_b, nb_e] = m_graph.get_neighbours(vertex, false);
        for (auto it2 = nb_b; it2 != nb_e; ++it2) {
            if (m_match_vertices.find(*it2) == m_match_vertices.end())
                outside_sum += log_prob_of_vertex(*it2);
        }
        m_candidate_outside_logp[vertex] = outside_sum;
    } else {
        // Existing candidate — increment in-degree.
        ++(deg_it->second);
        // s_vertex moved from outside to inside for this candidate.
        auto cache_it = m_candidate_outside_logp.find(vertex);
        if (cache_it != m_candidate_outside_logp.end())
            cache_it->second -= log_prob_of_vertex(absorbed_vertex);
    }
}

/* ---------- get_top_k_vertices ---------- */

std::vector<CandidateVertex> SingleGraphHistogram::get_top_k_vertices(uint32_t k)
{
    if (m_candidates.empty() || k == 0) return {};

    const double alpha =
        m_alpha_0 * std::pow(m_decay, static_cast<double>(m_current_depth));

    // Score all valid candidates.
    std::vector<CandidateVertex> scored;
    scored.reserve(m_candidates.size());

    for (uint32_t v : m_candidates) {
        const auto deg_it   = m_candidate_in_degree.find(v);
        const uint32_t k_in = (deg_it != m_candidate_in_degree.end())
                              ? deg_it->second : 0u;
        if (k_in == 0 && m_current_depth > 0) continue;

        const auto cache_it = m_candidate_outside_logp.find(v);
        if (cache_it == m_candidate_outside_logp.end())
            throw std::runtime_error(
                "SingleGraphHistogram::get_top_k_vertices: outside_logp cache "
                "missing for candidate " + std::to_string(v));
        const double outside_logp = cache_it->second;

        const double score =
            static_cast<double>(k_in) * m_log_density
            + log_prob_of_vertex(v)
            + alpha * outside_logp;

        auto parent_it = m_candidate_any_parent.find(v);
        const int32_t connect = (parent_it != m_candidate_any_parent.end())
            ? static_cast<int32_t>(parent_it->second)
            : -1;

        scored.push_back({static_cast<int32_t>(v), connect, score});
    }

    if (scored.empty()) return {};

    // Partial sort: bring the best k to the front.
    const uint32_t actual_k = std::min(k, static_cast<uint32_t>(scored.size()));
    std::partial_sort(
        scored.begin(),
        scored.begin() + static_cast<std::ptrdiff_t>(actual_k),
        scored.end(),
        [](const CandidateVertex& a, const CandidateVertex& b) {
            return a.score < b.score;
        });

    scored.resize(actual_k);
    return scored;
}


