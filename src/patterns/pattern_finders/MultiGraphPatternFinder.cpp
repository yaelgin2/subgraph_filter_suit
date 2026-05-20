#include "MultiGraphPatternFinder.h"

#include "BoostGraph.h"
#include "ColoredGraph.h"
#include "DebugLog.h"
#include "GeneralColorHist.h"
#include "LogLevel.h"
#include "LoggerHandler.h"
#include "Node.h"
#include "PatternUtils.h"
#include "Tree.h"

#include <algorithm>
#include <boost/graph/adjacency_list.hpp>
#include <boost/none.hpp>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace sgf
{

MultiGraphPatternFinder::MultiGraphPatternFinder(std::vector<ColoredGraph>& graph_list,
                                                 const bool is_directed, LoggerHandler logger)
    : m_graph_list(graph_list)
    , m_is_directed(is_directed)
    , m_logger(std::move(logger))
{
}

/* ---------- Initialisation helpers ---------- */

void MultiGraphPatternFinder::build_color_map()
{
    m_color_map = PatternUtils::map_colors(m_graph_list);
}

std::vector<double> MultiGraphPatternFinder::build_color_distribution() const
{
    return PatternUtils::compute_color_distribution(static_cast<uint32_t>(m_color_map.size()),
                                                    static_cast<int32_t>(m_graph_list.size()),
                                                    m_graph_list);
}

void MultiGraphPatternFinder::create_histograms(const uint32_t color_count)
{
    m_forward_color_hist = GeneralColorHist(color_count, m_logger);
    if (m_is_directed)
    {
        m_reverse_color_hist = GeneralColorHist(color_count, m_logger);
    }
    else
    {
        m_reverse_color_hist = boost::none;
    }
}

std::vector<std::vector<NodePtr>> MultiGraphPatternFinder::initialize_match_trees()
{
    const uint32_t graph_count = static_cast<uint32_t>(m_graph_list.size());
    m_match_trees.resize(graph_count);

    std::optional<std::reference_wrapper<GeneralColorHist>> reverse_hist_ref = std::nullopt;
    if (m_is_directed && m_reverse_color_hist)
    {
        reverse_hist_ref = std::ref(*m_reverse_color_hist);
    }

    for (uint32_t graph_idx = 0U; graph_idx < graph_count; ++graph_idx)
    {
        m_match_trees[graph_idx] = std::make_shared<Tree>(0U, m_graph_list[graph_idx], m_logger,
                                                          *m_forward_color_hist, reverse_hist_ref);
    }

    return std::vector<std::vector<NodePtr>>(graph_count);
}

uint32_t MultiGraphPatternFinder::select_first_color(const std::vector<double>& color_distribution)
{
    std::vector<std::pair<double, uint32_t>> sorted_colors;
    for (uint32_t color_idx = 0U; color_idx < static_cast<uint32_t>(color_distribution.size());
         ++color_idx)
    {
        if (color_distribution[color_idx] > 0.0)
        {
            sorted_colors.emplace_back(color_distribution[color_idx], color_idx);
        }
    }

    std::sort(sorted_colors.begin(), sorted_colors.end(),
              [](const std::pair<double, uint32_t>& left, const std::pair<double, uint32_t>& right)
              {
                  return left.first > right.first;
              });

    return sorted_colors.empty() ? 0U : sorted_colors.front().second;
}

void MultiGraphPatternFinder::seed_initial_matches(const uint32_t first_color,
                                                   std::vector<std::vector<NodePtr>>& leaf_matches)
{
    boost::add_vertex(VertexProperties{first_color}, m_pattern);
    const uint32_t graph_count = static_cast<uint32_t>(m_graph_list.size());

    for (uint32_t graph_idx = 0U; graph_idx < graph_count; ++graph_idx)
    {
        const std::vector<uint32_t> initial_matches =
            PatternUtils::find_initial_matches(m_graph_list[graph_idx], first_color);

        std::vector<std::pair<uint32_t, NodePtr>> initial_match_pairs;
        initial_match_pairs.reserve(initial_matches.size());
        for (const uint32_t matched_vertex : initial_matches)
        {
            initial_match_pairs.emplace_back(matched_vertex, m_match_trees[graph_idx]->get_root());
        }
        leaf_matches[graph_idx] = m_match_trees[graph_idx]->add_tree_level(initial_match_pairs);

        if (initial_matches.empty())
        {
            m_match_trees[graph_idx].reset();
        }
    }

    for (uint32_t graph_idx = 0U; graph_idx < graph_count; ++graph_idx)
    {
        if (m_match_trees[graph_idx])
        {
            m_alive_graph_indexes.insert(graph_idx);
        }
    }
}

void MultiGraphPatternFinder::setup_random_engine()
{
    const uint64_t time_seed =
        static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    std::seed_seq seed_sequence{static_cast<uint32_t>(time_seed & LOWER_32_BITS_MASK),
                                static_cast<uint32_t>(time_seed >> UPPER_32_BITS_SHIFT)};
    m_random_engine.seed(seed_sequence);
}

/* ---------- Growth loop helpers ---------- */

bool MultiGraphPatternFinder::attempt_add_vertex(const double alive_threshold, const bool is_random,
                                                 std::vector<std::vector<NodePtr>>& leaf_matches)
{
    m_logger.log(LogLevel::INFO, "Attempting to add vertex.");
    const uint32_t min_alive_count =
        static_cast<uint32_t>(static_cast<double>(m_graph_list.size()) * alive_threshold);
    const std::tuple<int32_t, int32_t, bool> histogram_candidates =
        get_candidates_from_histogram(min_alive_count, is_random);

    if (std::get<0>(histogram_candidates) == -1)
    {
        return false;
    }

    const uint32_t new_vertex_color = static_cast<uint32_t>(std::get<0>(histogram_candidates));
    const uint32_t connection_vertex_id = static_cast<uint32_t>(std::get<1>(histogram_candidates));
    const bool is_edge_reversed = std::get<2>(histogram_candidates);
    const uint32_t new_vertex_id =
        static_cast<uint32_t>(boost::add_vertex(VertexProperties{new_vertex_color}, m_pattern));

    m_logger.log(LogLevel::INFO,
                 "Adding vertex color=" + std::to_string(m_color_map[new_vertex_color]) +
                     " connected_to=" + std::to_string(connection_vertex_id) +
                     " reversed=" + std::to_string(static_cast<int32_t>(is_edge_reversed)));

    uint32_t edge_src = connection_vertex_id;
    uint32_t edge_tgt = new_vertex_id;
    if (is_edge_reversed)
    {
        std::swap(edge_src, edge_tgt);
    }

    PatternUtils::add_edge(m_is_directed, m_pattern, edge_src, edge_tgt);
    extend_pattern_at_node_find_matches_in_s(new_vertex_color, connection_vertex_id,
                                             is_edge_reversed, leaf_matches);

    return true;
}

void MultiGraphPatternFinder::log_alive_graph_indexes() const
{
    std::string alive_indices_log;
    for (const uint32_t alive_idx : m_alive_graph_indexes)
    {
        alive_indices_log += std::to_string(alive_idx) + " ";
    }
    SGF_DEBUG_LOG(m_logger, "Alive indices: " + alive_indices_log);
}

void MultiGraphPatternFinder::run_one_growth_step(const double alive_threshold,
                                                  const bool is_random,
                                                  std::vector<std::vector<NodePtr>>& leaf_matches,
                                                  bool& done_adding_vertices, bool& failed_add_edge)
{
    const double vertex_add_probability =
        1.0 / std::cbrt(static_cast<double>(boost::num_vertices(m_pattern)));
    double random_value = m_uniform_dist(m_random_engine);
    if (!is_random)
    {
        random_value = NON_RANDOM_PROBABILITY;
    }

    if (((random_value < vertex_add_probability) && !done_adding_vertices) || failed_add_edge)
    {
        const bool vertex_added = attempt_add_vertex(alive_threshold, is_random, leaf_matches);
        if (!vertex_added)
        {
            done_adding_vertices = true;
        }
        failed_add_edge = false;
    }
    else
    {
        m_logger.log(LogLevel::INFO, "Attempting to add edge.");
        failed_add_edge = !add_edge(leaf_matches, 0.0, alive_threshold);
    }
}

std::pair<BoostGraph, std::unordered_set<uint32_t>> MultiGraphPatternFinder::finalize_and_return(
    const std::chrono::time_point<std::chrono::high_resolution_clock>& start_time)
{
    PatternUtils::recolor_pattern(m_pattern, m_color_map);

    const std::chrono::time_point<std::chrono::high_resolution_clock> end_time =
        std::chrono::high_resolution_clock::now();
    SGF_DEBUG_LOG(m_logger,
                  "find_pattern completed in " +
                      std::to_string(std::chrono::duration<double>(end_time - start_time).count()) +
                      "s");

    m_match_trees.clear();
    m_color_map.clear();

    return {std::move(m_pattern), std::move(m_alive_graph_indexes)};
}

/* ---------- Histogram selection ---------- */

std::tuple<int32_t, int32_t, bool> MultiGraphPatternFinder::select_directed_candidate(
    const std::tuple<int32_t, int32_t, uint32_t>& forward_candidate, const uint32_t min_alive_count,
    const bool is_random)
{
    const std::tuple<int32_t, int32_t, uint32_t> reverse_candidate =
        m_reverse_color_hist->get_color_to_add(std::max(1U, min_alive_count), is_random);

    const uint32_t combined_weight =
        std::get<2>(forward_candidate) + std::get<2>(reverse_candidate);

    if (is_random)
    {
        if (combined_weight > 0U)
        {
            std::uniform_real_distribution<double> weight_dist(
                0.0, static_cast<double>(combined_weight));
            if (weight_dist(m_random_engine) <= static_cast<double>(std::get<2>(forward_candidate)))
            {
                return {std::get<0>(forward_candidate), std::get<1>(forward_candidate), false};
            }
            return {std::get<0>(reverse_candidate), std::get<1>(reverse_candidate), true};
        }
        return {std::get<0>(forward_candidate), std::get<1>(forward_candidate), false};
    }

    if (std::get<2>(forward_candidate) >= std::get<2>(reverse_candidate))
    {
        return {std::get<0>(forward_candidate), std::get<1>(forward_candidate), false};
    }
    return {std::get<0>(reverse_candidate), std::get<1>(reverse_candidate), true};
}

std::tuple<int32_t, int32_t, bool>
MultiGraphPatternFinder::get_candidates_from_histogram(const uint32_t min_alive_count,
                                                       const bool is_random)
{
    const std::tuple<int32_t, int32_t, uint32_t> forward_candidate =
        m_forward_color_hist->get_color_to_add(std::max(1U, min_alive_count), is_random);

    if (m_is_directed)
    {
        return select_directed_candidate(forward_candidate, min_alive_count, is_random);
    }

    return {std::get<0>(forward_candidate), std::get<1>(forward_candidate), false};
}

/* ---------- Pattern extension ---------- */

std::vector<std::pair<uint32_t, NodePtr>> MultiGraphPatternFinder::collect_extension_candidates(
    const uint32_t graph_idx, const std::vector<std::vector<NodePtr>>& leaf_matches,
    const uint32_t new_vertex_color, const uint32_t connection_vertex_id,
    const bool is_edge_reversed)
{
    std::vector<std::pair<uint32_t, NodePtr>> extension_candidates;

    for (const NodePtr& current_leaf : leaf_matches[graph_idx])
    {
        const NodePtr connection_tree_node =
            Tree::get_node_by_depth(current_leaf, connection_vertex_id + 1U);

        const std::unordered_map<uint32_t, uint32_t> path_vertex_map =
            Tree::get_tree_path_map(current_leaf);

        const std::pair<std::vector<uint32_t>::const_iterator,
                        std::vector<uint32_t>::const_iterator>
            neighbor_range = m_graph_list[graph_idx].get_neighbours(connection_tree_node->m_index,
                                                                    is_edge_reversed);

        for (std::vector<uint32_t>::const_iterator neighbor_it = neighbor_range.first;
             neighbor_it != neighbor_range.second; ++neighbor_it)
        {
            if (m_graph_list[graph_idx].get_vertex_color(*neighbor_it) == new_vertex_color &&
                path_vertex_map.find(*neighbor_it) == path_vertex_map.end())
            {
                extension_candidates.emplace_back(*neighbor_it, current_leaf);
            }
        }
    }

    return extension_candidates;
}

void MultiGraphPatternFinder::update_tree_after_extension(
    const uint32_t graph_idx, const std::vector<std::pair<uint32_t, NodePtr>>& extension_candidates,
    std::vector<std::vector<NodePtr>>& leaf_matches)
{
    std::vector<NodePtr> new_leaf_nodes =
        m_match_trees[graph_idx]->add_tree_level(extension_candidates);

    for (const NodePtr& leaf_node : leaf_matches[graph_idx])
    {
        if (leaf_node->m_son == nullptr)
        {
            m_match_trees[graph_idx]->remove_node(leaf_node);
        }
    }

    leaf_matches[graph_idx] = std::move(new_leaf_nodes);

    if (m_match_trees[graph_idx]->is_empty())
    {
        m_alive_graph_indexes.erase(graph_idx);
    }
}

void MultiGraphPatternFinder::extend_pattern_at_node_find_matches_in_s(
    const uint32_t new_vertex_color, const uint32_t connection_vertex_id,
    const bool is_edge_reversed, std::vector<std::vector<NodePtr>>& leaf_matches)
{
    const uint32_t graph_count = static_cast<uint32_t>(m_graph_list.size());
    for (uint32_t graph_idx = 0U; graph_idx < graph_count; ++graph_idx)
    {
        if (m_match_trees[graph_idx]->is_empty())
        {
            continue;
        }

        const std::vector<std::pair<uint32_t, NodePtr>> extension_candidates =
            collect_extension_candidates(graph_idx, leaf_matches, new_vertex_color,
                                         connection_vertex_id, is_edge_reversed);

        update_tree_after_extension(graph_idx, extension_candidates, leaf_matches);
    }
}

/* ---------- Edge scoring and pruning ---------- */

bool MultiGraphPatternFinder::is_edge_supported_by_graph(
    const uint32_t graph_idx, const uint32_t pattern_src, const uint32_t pattern_tgt,
    const std::vector<std::vector<NodePtr>>& leaf_matches) const
{
    return std::any_of(leaf_matches[graph_idx].cbegin(), leaf_matches[graph_idx].cend(),
                       [this, graph_idx, pattern_src, pattern_tgt](const NodePtr& leaf_node)
                       {
                           const NodePtr source_node =
                               Tree::get_node_by_depth(leaf_node, pattern_src + 1U);
                           const NodePtr target_node =
                               Tree::get_node_by_depth(leaf_node, pattern_tgt + 1U);
                           if (!source_node || !target_node)
                           {
                               return false;
                           }
                           return m_graph_list[graph_idx].is_edge(
                               static_cast<BoostGraph::vertex_descriptor>(source_node->m_index),
                               static_cast<BoostGraph::vertex_descriptor>(target_node->m_index));
                       });
}

uint32_t MultiGraphPatternFinder::score_edge_support(
    const uint32_t pattern_src, const uint32_t pattern_tgt,
    const std::vector<std::vector<NodePtr>>& leaf_matches) const
{
    uint32_t support_count = 0U;
    const uint32_t graph_count = static_cast<uint32_t>(m_graph_list.size());
    for (uint32_t graph_idx = 0U; graph_idx < graph_count; ++graph_idx)
    {
        if (m_match_trees[graph_idx]->is_empty())
        {
            continue;
        }
        if (is_edge_supported_by_graph(graph_idx, pattern_src, pattern_tgt, leaf_matches))
        {
            ++support_count;
        }
    }
    return support_count;
}

void MultiGraphPatternFinder::filter_tree_matches_by_edge(
    const uint32_t tree_idx, const uint32_t pattern_src, const uint32_t pattern_tgt,
    std::vector<std::vector<NodePtr>>& leaf_matches)
{
    std::vector<NodePtr> surviving_matches;
    for (const NodePtr& leaf_node : leaf_matches[tree_idx])
    {
        const NodePtr source_tree_node = Tree::get_node_by_depth(leaf_node, pattern_src + 1U);
        const NodePtr target_tree_node = Tree::get_node_by_depth(leaf_node, pattern_tgt + 1U);
        if (!source_tree_node || !target_tree_node)
        {
            continue;
        }

        const BoostGraph::vertex_descriptor source_vertex_idx =
            static_cast<BoostGraph::vertex_descriptor>(source_tree_node->m_index);
        const BoostGraph::vertex_descriptor target_vertex_idx =
            static_cast<BoostGraph::vertex_descriptor>(target_tree_node->m_index);

        if (m_graph_list[tree_idx].is_edge(static_cast<uint32_t>(source_vertex_idx),
                                           static_cast<uint32_t>(target_vertex_idx)))
        {
            surviving_matches.push_back(leaf_node);
        }
        else
        {
            m_match_trees[tree_idx]->remove_node(leaf_node);
        }
    }

    leaf_matches[tree_idx] = std::move(surviving_matches);

    if (m_match_trees[tree_idx]->is_empty())
    {
        m_alive_graph_indexes.erase(tree_idx);
    }
}

void MultiGraphPatternFinder::apply_edge_and_prune(const uint32_t pattern_src,
                                                   const uint32_t pattern_tgt,
                                                   std::vector<std::vector<NodePtr>>& leaf_matches)
{
    PatternUtils::add_edge(m_is_directed, m_pattern, pattern_src, pattern_tgt);

    const uint32_t tree_count = static_cast<uint32_t>(m_match_trees.size());
    for (uint32_t tree_idx = 0U; tree_idx < tree_count; ++tree_idx)
    {
        if (m_match_trees[tree_idx]->is_empty())
        {
            continue;
        }
        filter_tree_matches_by_edge(tree_idx, pattern_src, pattern_tgt, leaf_matches);
    }
}

bool MultiGraphPatternFinder::is_candidate_edge(const uint32_t source_vertex,
                                                const uint32_t target_vertex) const
{
    if (source_vertex == target_vertex)
    {
        return false;
    }
    if (!m_is_directed && source_vertex >= target_vertex)
    {
        return false;
    }
    return !boost::edge(source_vertex, target_vertex, m_pattern).second;
}

bool MultiGraphPatternFinder::find_best_candidate_edge(
    const std::vector<std::vector<NodePtr>>& leaf_matches, uint32_t& best_src_out,
    uint32_t& best_tgt_out, uint32_t& best_score_out) const
{
    bool edge_found = false;
    const uint32_t vertex_count = static_cast<uint32_t>(boost::num_vertices(m_pattern));

    for (uint32_t source_vertex = 0U; source_vertex < vertex_count; ++source_vertex)
    {
        for (uint32_t target_vertex = 0U; target_vertex < vertex_count; ++target_vertex)
        {
            if (!is_candidate_edge(source_vertex, target_vertex))
            {
                continue;
            }

            const uint32_t edge_score =
                score_edge_support(source_vertex, target_vertex, leaf_matches);

            if (edge_score > best_score_out)
            {
                best_score_out = edge_score;
                best_src_out = source_vertex;
                best_tgt_out = target_vertex;
                edge_found = true;
            }
        }
    }

    return edge_found;
}

bool MultiGraphPatternFinder::add_edge(std::vector<std::vector<NodePtr>>& leaf_matches,
                                       const double support_threshold, const double alive_threshold)
{
    if (m_alive_graph_indexes.empty())
    {
        return false;
    }

    uint32_t best_score = 0U;
    uint32_t best_src = 0U;
    uint32_t best_tgt = 0U;

    if (!find_best_candidate_edge(leaf_matches, best_src, best_tgt, best_score))
    {
        return false;
    }

    if ((best_score >= alive_threshold * static_cast<double>(m_graph_list.size())) &&
        (best_score >= support_threshold * static_cast<double>(m_alive_graph_indexes.size())))
    {
        apply_edge_and_prune(best_src, best_tgt, leaf_matches);
        m_logger.log(LogLevel::INFO, "Adding edge (" + std::to_string(best_src) + ", " +
                                         std::to_string(best_tgt) +
                                         ") score=" + std::to_string(best_score));
        return true;
    }

    return false;
}

/* ---------- Main algorithm ---------- */

std::pair<BoostGraph, std::unordered_set<uint32_t>>
MultiGraphPatternFinder::find_pattern(const double alive_threshold, const bool is_random)
{
    if (m_graph_list.empty())
    {
        throw std::runtime_error("No input graphs provided to MultiGraphPatternFinder");
    }

    m_match_trees.clear();
    m_alive_graph_indexes.clear();
    m_pattern.clear();

    const std::chrono::time_point<std::chrono::high_resolution_clock> start_time =
        std::chrono::high_resolution_clock::now();

    build_color_map();
    const std::vector<double> color_distribution = build_color_distribution();

    if (color_distribution.empty())
    {
        throw std::runtime_error("No input graphs provided to MultiGraphPatternFinder");
    }

    create_histograms(static_cast<uint32_t>(m_color_map.size()));
    std::vector<std::vector<NodePtr>> leaf_matches = initialize_match_trees();

    const uint32_t first_color = select_first_color(color_distribution);
    m_logger.log(LogLevel::INFO,
                 "Choosing first color: " + std::to_string(m_color_map[first_color]));

    seed_initial_matches(first_color, leaf_matches);
    setup_random_engine();

    bool failed_add_edge = false;
    bool done_adding_vertices = false;

    while (static_cast<double>(m_alive_graph_indexes.size()) >=
           alive_threshold * static_cast<double>(m_graph_list.size()))
    {
        run_one_growth_step(alive_threshold, is_random, leaf_matches, done_adding_vertices,
                            failed_add_edge);

        if (failed_add_edge && done_adding_vertices)
        {
            m_logger.log(LogLevel::INFO, "Done growing pattern.");
            break;
        }

        log_alive_graph_indexes();
    }

    return finalize_and_return(start_time);
}

}  // namespace sgf
