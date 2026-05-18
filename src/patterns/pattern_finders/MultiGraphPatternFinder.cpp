#include "MultiGraphPatternFinder.h"

#include "BoostGraph.h"
#include "GeneralColorHist.h"
#include "LogLevel.h"
#include "PatternUtils.h"
#include "Tree.h"

#include <algorithm>
#include <boost/graph/adjacency_list.hpp>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace sgf
{

MultiGraphPatternFinder::MultiGraphPatternFinder(
    std::vector<ColoredGraph>& s_list, const bool is_directed, LoggerHandler logger)
    : m_s_list(s_list), m_is_directed(is_directed), m_logger(logger)
{
}

/* ---------- Helper Functions ---------- */

std::tuple<int32_t, int32_t, bool> MultiGraphPatternFinder::get_candidates_from_histogram(
    GeneralColorHist& color_hist,
    boost::optional<GeneralColorHist>& reverse_color_hist,
    const uint32_t alive_threshold_number,
    const bool is_random)
{
    std::mt19937_64 rng;
    const uint64_t time_seed =
        static_cast<uint64_t>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count());
    std::seed_seq ss{static_cast<uint32_t>(time_seed & 0xffffffffULL),
                     static_cast<uint32_t>(time_seed >> 32U)};
    rng.seed(ss);

    const std::tuple<int32_t, int32_t, uint32_t> candidates =
        color_hist.get_color_to_add(std::max(1U, alive_threshold_number), is_random);

    if (m_is_directed)
    {
        const std::tuple<int32_t, int32_t, uint32_t> reverse_candidates =
            reverse_color_hist->get_color_to_add(std::max(1U, alive_threshold_number));

        const uint32_t total_weight = std::get<2>(candidates) + std::get<2>(reverse_candidates);

        if (is_random)
        {
            if (total_weight > 0U)
            {
                std::uniform_real_distribution<double> dist(
                    0.0, static_cast<double>(total_weight));
                if (dist(rng) <= static_cast<double>(std::get<2>(candidates)))
                {
                    return {std::get<0>(candidates), std::get<1>(candidates), false};
                }
                return {std::get<0>(reverse_candidates), std::get<1>(reverse_candidates), true};
            }
        }
        else
        {
            if (std::get<2>(candidates) > std::get<2>(reverse_candidates))
            {
                return {std::get<0>(candidates), std::get<1>(candidates), false};
            }
            return {std::get<0>(reverse_candidates), std::get<1>(reverse_candidates), true};
        }
    }

    return {std::get<0>(candidates), std::get<1>(candidates), false};
}

/* ---------- Statistics ---------- */

uint32_t MultiGraphPatternFinder::find_first_color(const uint32_t color_number)
{
    std::vector<std::pair<uint32_t, uint32_t>> color_count(color_number, {0U, 0U});
    for (uint32_t color_idx = 0U; color_idx < static_cast<uint32_t>(color_count.size());
         ++color_idx)
    {
        color_count[color_idx].second = color_idx;
    }

    const uint32_t graph_count = static_cast<uint32_t>(m_s_list.size());
    for (uint32_t graph_idx = 0U; graph_idx < graph_count; ++graph_idx)
    {
        const uint32_t vertex_count = m_s_list[graph_idx].vertex_count();
        for (uint32_t vertex_idx = 0U; vertex_idx < vertex_count; ++vertex_idx)
        {
            color_count[m_s_list[graph_idx].get_vertex_color(vertex_idx)].first++;
        }
    }

    std::sort(color_count.begin(), color_count.end());
    return color_count[color_count.size() - FIRST_COLOR_ORDER].second;
}

/* ---------- Pattern extension ---------- */

std::pair<int32_t, int32_t>
MultiGraphPatternFinder::extend_pattern_at_node_find_matches_in_s(
    const uint32_t new_color,
    const uint32_t node_to_connect_id,
    const bool is_reversed,
    std::vector<std::vector<NodePtr>>& last_nodes)
{
    const uint32_t graph_count = static_cast<uint32_t>(m_s_list.size());
    for (uint32_t graph_idx = 0U; graph_idx < graph_count; ++graph_idx)
    {
        if (m_trees[graph_idx]->is_empty())
        {
            continue;
        }
        std::vector<std::pair<uint32_t, NodePtr>> candidates;

        for (const NodePtr& lowest : last_nodes[graph_idx])
        {
            const NodePtr node_in_tree =
                Tree::get_node_by_depth(lowest, node_to_connect_id + 1U);

            const std::unordered_map<uint32_t, uint32_t> in_match =
                Tree::get_tree_path_map(lowest);

            const std::pair<std::vector<uint32_t>::const_iterator,
                            std::vector<uint32_t>::const_iterator>
                neighbour_range =
                    m_s_list[graph_idx].get_neighbours(node_in_tree->m_index, is_reversed);

            for (std::vector<uint32_t>::const_iterator neighbour_it = neighbour_range.first;
                 neighbour_it != neighbour_range.second;
                 ++neighbour_it)
            {
                if (m_s_list[graph_idx].get_vertex_color(*neighbour_it) == new_color &&
                    in_match.find(*neighbour_it) == in_match.end())
                {
                    candidates.push_back({*neighbour_it, lowest});
                }
            }
        }

        std::vector<NodePtr> new_last_nodes = m_trees[graph_idx]->add_tree_level(candidates);

        for (const NodePtr& node : last_nodes[graph_idx])
        {
            if (node->m_son == nullptr)
            {
                m_trees[graph_idx]->remove_node(node);
            }
        }

        last_nodes[graph_idx] = std::move(new_last_nodes);

        if (m_trees[graph_idx]->is_empty())
        {
            m_alive_indexes.erase(graph_idx);
        }
    }

    uint32_t sum_last_nodes_sizes = 0U;
    for (const auto& nodes : last_nodes)
    {
        sum_last_nodes_sizes += static_cast<uint32_t>(nodes.size());
    }

    return {static_cast<int32_t>(m_alive_indexes.size()),
            static_cast<int32_t>(sum_last_nodes_sizes)};
}

/* ---------- Edge scoring and pruning ---------- */

uint32_t MultiGraphPatternFinder::score_edge_support(
    const uint32_t uP,
    const uint32_t vP,
    const std::vector<std::vector<NodePtr>>& last_nodes)
{
    uint32_t score = 0U;

    const uint32_t graph_count = static_cast<uint32_t>(m_s_list.size());
    for (uint32_t graph_idx = 0U; graph_idx < graph_count; ++graph_idx)
    {
        if (!m_trees[graph_idx]->is_empty())
        {
            continue;
        }

        bool supported = false;
        for (const NodePtr& last_node : last_nodes[graph_idx])
        {
            const NodePtr uS = Tree::get_node_by_depth(last_node, uP + 1U);
            const NodePtr vS = Tree::get_node_by_depth(last_node, vP + 1U);
            if (!uS || !vS)
            {
                continue;
            }

            const BoostGraph::vertex_descriptor u_desc =
                static_cast<BoostGraph::vertex_descriptor>(uS->m_index);
            const BoostGraph::vertex_descriptor v_desc =
                static_cast<BoostGraph::vertex_descriptor>(vS->m_index);

            if (m_s_list[graph_idx].is_edge(u_desc, v_desc))
            {
                supported = true;
                break;
            }
        }
        if (supported)
        {
            ++score;
        }
    }

    return score;
}

void MultiGraphPatternFinder::apply_edge_and_prune(
    const uint32_t uP,
    const uint32_t vP,
    std::vector<std::vector<NodePtr>>& last_nodes)
{
    PatternUtils::add_edge(m_is_directed, m_pattern, uP, vP);

    const uint32_t tree_count = static_cast<uint32_t>(m_trees.size());
    for (uint32_t tree_idx = 0U; tree_idx < tree_count; ++tree_idx)
    {
        if (!m_trees[tree_idx]->is_empty())
        {
            continue;
        }

        std::vector<NodePtr> updated_matches;
        for (const NodePtr& match : last_nodes[tree_idx])
        {
            const NodePtr uS = Tree::get_node_by_depth(match, uP + 1U);
            const NodePtr vS = Tree::get_node_by_depth(match, vP + 1U);
            if (!uS || !vS)
            {
                continue;
            }

            const BoostGraph::vertex_descriptor u_desc =
                static_cast<BoostGraph::vertex_descriptor>(uS->m_index);
            const BoostGraph::vertex_descriptor v_desc =
                static_cast<BoostGraph::vertex_descriptor>(vS->m_index);

            if (m_s_list[tree_idx].is_edge(u_desc, v_desc))
            {
                updated_matches.push_back(match);
            }
            else
            {
                m_trees[tree_idx]->remove_node(match);
            }
        }

        last_nodes[tree_idx] = std::move(updated_matches);

        if (m_trees[tree_idx]->is_empty())
        {
            m_alive_indexes.erase(tree_idx);
        }
    }
}

bool MultiGraphPatternFinder::add_edge(
    std::vector<std::vector<NodePtr>>& last_nodes,
    const double threshold,
    const double alive_threshold)
{
    if (m_alive_indexes.empty())
    {
        return false;
    }

    uint32_t best_score = 0U;
    uint32_t best_u = 0U;
    uint32_t best_v = 0U;
    bool found = false;

    const uint32_t vertex_count = static_cast<uint32_t>(boost::num_vertices(m_pattern));
    for (uint32_t uP = 0U; uP < vertex_count; ++uP)
    {
        for (uint32_t vP = 0U; vP < vertex_count; ++vP)
        {
            if (!m_is_directed && uP >= vP)
            {
                continue;
            }
            if (boost::edge(uP, vP, m_pattern).second)
            {
                continue;
            }

            const uint32_t score = score_edge_support(uP, vP, last_nodes);
            if (score > best_score)
            {
                best_score = score;
                best_u = uP;
                best_v = vP;
                found = true;
            }
        }
    }

    if (!found)
    {
        return false;
    }

    if ((best_score >= alive_threshold * static_cast<double>(m_s_list.size())) &&
        (best_score >= threshold * static_cast<double>(m_alive_indexes.size())))
    {
        apply_edge_and_prune(best_u, best_v, last_nodes);
        m_logger.log(LogLevel::INFO,
                     "Adding edge (" + std::to_string(best_u) + ", " +
                         std::to_string(best_v) + ") score=" + std::to_string(best_score));
        return true;
    }

    return false;
}

/* ---------- Main algorithm ---------- */

std::pair<BoostGraph, std::unordered_set<uint32_t>>
MultiGraphPatternFinder::find_pattern(const double alive_threshold, const bool is_random)
{
    if (m_s_list.empty())
    {
        throw std::runtime_error("No input graphs provided to MultiGraphPatternFinder");
    }

    m_trees.clear();
    m_alive_indexes.clear();
    m_pattern.clear();

    const std::chrono::time_point<std::chrono::high_resolution_clock> start_time =
        std::chrono::high_resolution_clock::now();

    std::vector<int32_t> color_map = PatternUtils::map_colors(m_s_list);

    const std::vector<double> color_prob = PatternUtils::compute_color_distribution(
        static_cast<uint32_t>(color_map.size()),
        static_cast<int32_t>(m_s_list.size()),
        m_s_list);

    if (color_prob.empty())
    {
        throw std::runtime_error("No input graphs provided to MultiGraphPatternFinder");
    }

    GeneralColorHist color_hist(static_cast<uint32_t>(color_map.size()));
    boost::optional<GeneralColorHist> reverse_color_hist;
    if (m_is_directed)
    {
        reverse_color_hist = GeneralColorHist(static_cast<uint32_t>(color_map.size()));
    }

    std::optional<std::reference_wrapper<GeneralColorHist>> reverse_hist_opt = std::nullopt;
    if (m_is_directed && reverse_color_hist)
    {
        reverse_hist_opt = std::ref(*reverse_color_hist);
    }

    const uint32_t graph_count = static_cast<uint32_t>(m_s_list.size());
    m_trees.resize(graph_count);
    for (uint32_t graph_idx = 0U; graph_idx < graph_count; ++graph_idx)
    {
        m_trees[graph_idx] =
            std::make_shared<Tree>(0U, m_s_list[graph_idx], m_logger, color_hist, reverse_hist_opt);
    }
    std::vector<std::vector<NodePtr>> last_nodes(graph_count);

    std::vector<std::pair<double, uint32_t>> colors;
    for (uint32_t color_idx = 0U; color_idx < static_cast<uint32_t>(color_prob.size());
         ++color_idx)
    {
        if (color_prob[color_idx] > 0.0)
        {
            colors.emplace_back(color_prob[color_idx], color_idx);
        }
    }

    std::sort(colors.begin(), colors.end(),
              [](const std::pair<double, uint32_t>& left,
                 const std::pair<double, uint32_t>& right)
              { return left.first > right.first; });

    const uint32_t first_color = colors.empty() ? 0U : colors.front().second;
    m_logger.log(LogLevel::INFO,
                 "Choosing first color: " + std::to_string(color_map[first_color]));

    boost::add_vertex(VertexProperties{first_color}, m_pattern);
    uint32_t alive_s = graph_count;

    for (uint32_t graph_idx = 0U; graph_idx < graph_count; ++graph_idx)
    {
        std::vector<uint32_t> matches =
            PatternUtils::find_initial_matches(m_s_list[graph_idx], first_color);

        std::vector<std::pair<uint32_t, NodePtr>> initial_indexes;
        for (const uint32_t match : matches)
        {
            initial_indexes.push_back({match, m_trees[graph_idx]->get_root()});
        }
        last_nodes[graph_idx] = m_trees[graph_idx]->add_tree_level(initial_indexes);

        if (matches.empty())
        {
            m_trees[graph_idx].reset();
            --alive_s;
        }
    }

    for (uint32_t graph_idx = 0U; graph_idx < graph_count; ++graph_idx)
    {
        if (m_trees[graph_idx])
        {
            m_alive_indexes.insert(graph_idx);
        }
    }

    bool failed_add_edge = false;
    bool done_adding_vertices = false;

    std::mt19937_64 rng;
    const uint64_t time_seed =
        static_cast<uint64_t>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count());
    std::seed_seq ss{static_cast<uint32_t>(time_seed & 0xffffffffULL),
                     static_cast<uint32_t>(time_seed >> 32U)};
    rng.seed(ss);
    std::uniform_real_distribution<double> unif(0.0, 1.0);

    while (alive_s >= alive_threshold * static_cast<double>(m_s_list.size()))
    {
        m_logger.log(LogLevel::DEBUG,
                     "Alive graphs: " + std::to_string(m_alive_indexes.size()));

        const double prob_add_vertex =
            1.0 / std::cbrt(static_cast<double>(boost::num_vertices(m_pattern)));
        double random_value = unif(rng);
        if (!is_random)
        {
            random_value = 0.5;
        }

        if (((random_value < prob_add_vertex) && !done_adding_vertices) || failed_add_edge)
        {
            const std::tuple<int32_t, int32_t, bool> candidates =
                get_candidates_from_histogram(
                    color_hist,
                    reverse_color_hist,
                    static_cast<uint32_t>(static_cast<double>(m_s_list.size()) * alive_threshold),
                    is_random);

            if (std::get<0>(candidates) == -1)
            {
                done_adding_vertices = true;
            }
            else
            {
                const int32_t color_new = std::get<0>(candidates);
                const int32_t node_to_connect = std::get<1>(candidates);
                const bool is_edge_reversed = std::get<2>(candidates);
                const uint32_t new_node_id =
                    static_cast<uint32_t>(boost::add_vertex(VertexProperties{static_cast<uint32_t>(color_new)}, m_pattern));

                m_logger.log(LogLevel::INFO,
                             "Adding vertex color=" +
                                 std::to_string(color_map[static_cast<uint32_t>(color_new)]) +
                                 " connected_to=" + std::to_string(node_to_connect) +
                                 " reversed=" + std::to_string(static_cast<int32_t>(is_edge_reversed)));

                uint32_t src = static_cast<uint32_t>(node_to_connect);
                uint32_t tgt = new_node_id;
                if (is_edge_reversed)
                {
                    std::swap(src, tgt);
                }

                PatternUtils::add_edge(m_is_directed, m_pattern, src, tgt);
                extend_pattern_at_node_find_matches_in_s(
                    static_cast<uint32_t>(color_new),
                    static_cast<uint32_t>(node_to_connect),
                    is_edge_reversed,
                    last_nodes);

                alive_s = static_cast<uint32_t>(m_alive_indexes.size());
            }

            failed_add_edge = false;
        }
        else if (!failed_add_edge)
        {
            failed_add_edge = !add_edge(last_nodes, 0.0, alive_threshold);
            alive_s = static_cast<uint32_t>(m_alive_indexes.size());
        }

        if (failed_add_edge && done_adding_vertices)
        {
            break;
        }
    }

    PatternUtils::recolor_pattern(m_pattern, color_map);

    const std::chrono::time_point<std::chrono::high_resolution_clock> end_time =
        std::chrono::high_resolution_clock::now();
    m_logger.log(LogLevel::DEBUG,
                 "find_pattern completed in " +
                     std::to_string(
                         std::chrono::duration<double>(end_time - start_time).count()) +
                     "s");

    BoostGraph result_pattern = std::move(m_pattern);
    std::unordered_set<uint32_t> result_alive = std::move(m_alive_indexes);

    m_trees.clear();
    m_alive_indexes.clear();
    m_pattern.clear();

    return {std::move(result_pattern), std::move(result_alive)};
}

}  // namespace sgf
