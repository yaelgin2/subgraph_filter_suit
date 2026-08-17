#include "MultiGraphPatternFinder.h"

#include "BoostGraph.h"
#include "ColoredGraph.h"
#include "CountsMap.h"
#include "DebugLog.h"
#include "LogLevel.h"
#include "LoggerHandler.h"
#include "Node.h"
#include "PatternUtils.h"
#include "Tree.h"

#include <algorithm>
#include <boost/graph/adjacency_list.hpp>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
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

MultiGraphPatternFinder::MultiGraphPatternFinder(const std::vector<ColoredGraph>& graph_list,
                                                 const bool is_directed, const uint32_t color_count,
                                                 const uint32_t thread_index, LoggerHandler logger)
    : m_graph_list(graph_list)
    , m_is_directed(is_directed)
    , m_color_count(color_count)
    , m_thread_index(thread_index)
    , m_logger(std::move(logger))
{
}

/* ---------- Initialisation helpers ---------- */

std::vector<double> MultiGraphPatternFinder::build_color_distribution() const
{
    return PatternUtils::compute_color_distribution(
        m_color_count, static_cast<int32_t>(m_graph_list.size()), m_graph_list);
}

std::vector<std::vector<NodePtr>> MultiGraphPatternFinder::initialize_match_trees()
{
    const uint32_t graph_count = static_cast<uint32_t>(m_graph_list.size());
    m_match_trees.resize(graph_count);

    for (uint32_t graph_idx = 0U; graph_idx < graph_count; ++graph_idx)
    {
        m_match_trees[graph_idx] =
            std::make_shared<Tree>(graph_idx, m_graph_list[graph_idx], m_logger);
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

std::vector<MultiGraphPatternFinder::Entry> MultiGraphPatternFinder::build_candidates(
    const CountsMap& combined_counts, const CountsMap& tree_support, const uint32_t min_alive_count)
{
    std::vector<Entry> candidates;
    for (const auto& entry : combined_counts)
    {
        const CountsMap::const_iterator support_it = tree_support.find(entry.first);
        if (support_it != tree_support.cend() && support_it->second >= min_alive_count)
        {
            candidates.emplace_back(entry.first, entry.second);
        }
    }
    return candidates;
}

std::tuple<uint32_t, uint32_t, bool>
MultiGraphPatternFinder::sample_candidate_random(const std::vector<Entry>& candidates)
{
    std::vector<double> weights;
    weights.reserve(candidates.size());
    for (const Entry& candidate : candidates)
    {
        weights.push_back(static_cast<double>(candidate.second));
    }
    std::discrete_distribution<uint32_t> dist(weights.cbegin(), weights.cend());
    const uint32_t sampled_idx = dist(m_random_engine);
    return candidates[sampled_idx].first;
}

std::pair<CountsMap, CountsMap> MultiGraphPatternFinder::accumulate_vertex_counts(
    const std::vector<std::vector<NodePtr>>& leaf_matches) const
{
    CountsMap combined_counts;
    CountsMap tree_support;
    for (const uint32_t graph_idx : m_alive_graph_indexes)
    {
        if (!m_match_trees[graph_idx] || m_match_trees[graph_idx]->is_empty())
        {
            continue;
        }
        CountsMap per_tree_counts;
        m_match_trees[graph_idx]->get_color_by_depth_neighbour_counts(leaf_matches[graph_idx],
                                                                      per_tree_counts);
        for (const auto& entry : per_tree_counts)
        {
            combined_counts[entry.first] += entry.second;
            ++tree_support[entry.first];
        }
    }
    return {combined_counts, tree_support};
}

std::optional<std::tuple<uint32_t, uint32_t, bool>>
MultiGraphPatternFinder::choose_next_vertex(const CountsMap& combined_counts,
                                            const CountsMap& tree_support,
                                            const uint32_t min_alive_count, const bool is_random)
{
    const std::vector<Entry> candidates =
        build_candidates(combined_counts, tree_support, min_alive_count);

    if (candidates.empty())
    {
        SGF_DEBUG_LOG(m_logger, thread_log_prefix() + "choose_next_vertex: no candidates");
        return std::nullopt;
    }

    std::string candidates_log = "choose_next_vertex: " + std::to_string(candidates.size()) +
                                 " candidates is_random=" + (is_random ? "true" : "false");
    for (const Entry& candidate : candidates)
    {
        candidates_log += " (color=" + std::to_string(std::get<0>(candidate.first)) +
                          " depth=" + std::to_string(std::get<1>(candidate.first)) +
                          " reversed=" + (std::get<2>(candidate.first) ? "true" : "false") +
                          " count=" + std::to_string(candidate.second) + ")";
    }
    SGF_DEBUG_LOG(m_logger, thread_log_prefix() + candidates_log);

    if (!is_random)
    {
        const std::vector<Entry>::const_iterator best_it =
            std::max_element(candidates.cbegin(), candidates.cend(),
                             [](const Entry& left, const Entry& right)
                             {
                                 return left.second < right.second;
                             });
        return best_it->first;
    }

    return sample_candidate_random(candidates);
}

bool MultiGraphPatternFinder::attempt_add_vertex(const double alive_threshold, const bool is_random,
                                                 std::vector<std::vector<NodePtr>>& leaf_matches)
{
    log_with_thread(LogLevel::TRACE, "Attempting to add vertex.");
    const uint32_t min_alive_count = std::max(
        1U, static_cast<uint32_t>(static_cast<double>(m_graph_list.size()) * alive_threshold));

    const std::pair<CountsMap, CountsMap> counts = accumulate_vertex_counts(leaf_matches);
    const std::optional<std::tuple<uint32_t, uint32_t, bool>> chosen =
        choose_next_vertex(counts.first, counts.second, min_alive_count, is_random);
    if (!chosen)
    {
        return false;
    }

    const uint32_t new_vertex_color = std::get<0>(*chosen);
    const uint32_t connection_vertex_id = std::get<1>(*chosen);
    const bool is_edge_reversed = std::get<2>(*chosen);
    const uint32_t new_vertex_id =
        static_cast<uint32_t>(boost::add_vertex(VertexProperties{new_vertex_color}, m_pattern));

    log_with_thread(LogLevel::TRACE, "Adding vertex color=" + std::to_string(new_vertex_color) +
                                         " connected_to=" + std::to_string(connection_vertex_id) +
                                         " reversed=" + (is_edge_reversed ? "true" : "false"));

    PatternUtils::add_edge(m_is_directed, m_pattern,
                           is_edge_reversed ? new_vertex_id : connection_vertex_id,
                           is_edge_reversed ? connection_vertex_id : new_vertex_id);
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
    log_with_thread(LogLevel::TRACE, "Alive indices: " + alive_indices_log);
}

std::string MultiGraphPatternFinder::thread_log_prefix() const
{
    return "[thread " + std::to_string(m_thread_index) + "] ";
}

void MultiGraphPatternFinder::log_with_thread(const LogLevel level, const std::string& message) const
{
    m_logger.log(level, thread_log_prefix() + message);
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
        log_with_thread(LogLevel::TRACE, "Attempting to add edge.");
        failed_add_edge = !add_edge(leaf_matches, 0.0, alive_threshold);
    }
}

std::pair<BoostGraph, std::unordered_set<uint32_t>> MultiGraphPatternFinder::finalize_and_return(
    [[maybe_unused]] const std::chrono::time_point<std::chrono::high_resolution_clock>& start_time)
{
    [[maybe_unused]] const std::chrono::time_point<std::chrono::high_resolution_clock> end_time =
        std::chrono::high_resolution_clock::now();
    SGF_DEBUG_LOG(m_logger,
                  thread_log_prefix() + "find_pattern completed in " +
                      std::to_string(std::chrono::duration<double>(end_time - start_time).count()) +
                      "s");

    m_match_trees.clear();

    return {std::move(m_pattern), std::move(m_alive_graph_indexes)};
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
    std::vector<std::vector<NodePtr>>& leaf_matches, const uint32_t /*connected_pattern_vertex*/)
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
        if (!m_match_trees[graph_idx] || m_match_trees[graph_idx]->is_empty())
        {
            continue;
        }

        const std::vector<std::pair<uint32_t, NodePtr>> extension_candidates =
            collect_extension_candidates(graph_idx, leaf_matches, new_vertex_color,
                                         connection_vertex_id, is_edge_reversed);

        update_tree_after_extension(graph_idx, extension_candidates, leaf_matches,
                                    connection_vertex_id);
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
        if (!m_match_trees[graph_idx] || m_match_trees[graph_idx]->is_empty())
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
        if (!m_match_trees[tree_idx] || m_match_trees[tree_idx]->is_empty())
        {
            continue;
        }
        filter_tree_matches_by_edge(tree_idx, pattern_src, pattern_tgt, leaf_matches);
    }
}

uint64_t MultiGraphPatternFinder::encode_edge_key(const uint32_t source_vertex,
                                                  const uint32_t target_vertex)
{
    return (static_cast<uint64_t>(source_vertex) << UPPER_32_BITS_SHIFT) | target_vertex;
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
    if (m_dead_edge_pairs.find(encode_edge_key(source_vertex, target_vertex)) !=
        m_dead_edge_pairs.cend())
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

            if (edge_score == 0U)
            {
                m_dead_edge_pairs.insert(encode_edge_key(source_vertex, target_vertex));
            }

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
        log_with_thread(LogLevel::TRACE, "Adding edge (" + std::to_string(best_src) + ", " +
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
    log_with_thread(LogLevel::INFO, "Start looking for pattern.");
    if (m_graph_list.empty())
    {
        throw std::runtime_error("No input graphs provided to MultiGraphPatternFinder");
    }

    m_match_trees.clear();
    m_alive_graph_indexes.clear();
    m_dead_edge_pairs.clear();
    m_pattern.clear();

    const std::chrono::time_point<std::chrono::high_resolution_clock> start_time =
        std::chrono::high_resolution_clock::now();

    const std::vector<double> color_distribution = build_color_distribution();

    if (color_distribution.empty())
    {
        throw std::runtime_error("No input graphs provided to MultiGraphPatternFinder");
    }

    std::vector<std::vector<NodePtr>> leaf_matches = initialize_match_trees();

    const uint32_t first_color = select_first_color(color_distribution);
    log_with_thread(LogLevel::TRACE, "Choosing first color: " + std::to_string(first_color));

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
            log_with_thread(LogLevel::INFO, "Done growing pattern.");
            break;
        }

        log_alive_graph_indexes();
    }

    return finalize_and_return(start_time);
}

}  // namespace sgf
