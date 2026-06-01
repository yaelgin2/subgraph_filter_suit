#include "Tree.h"

#include "AddNodeException.h"
#include "ColoredGraph.h"
#include "DebugLog.h"
#include "DeleteNodeException.h"
#include "GeneralColorHist.h"
#include "PatternPeriphery.h"
#include "LogLevel.h"
#include "LoggerHandler.h"
#include "Node.h"
#include "PatternException.h"

#include <cstddef>
#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{

/**
 * @brief Format vertex-parent pairs as "[(v0,p0) (v1,p1) ...]" for debug logging.
 * @param pairs Vector of (vertex index, parent node) pairs.
 * @return Formatted string.
 */
[[maybe_unused]] std::string format_vertex_parent_pairs(
    const std::vector<std::pair<uint32_t, sgf::NodePtr>>& pairs)
{
    std::string result = "[";
    for (size_t idx = 0; idx < pairs.size(); ++idx)
    {
        if (idx > 0U)
        {
            result += " ";
        }
        const uint32_t parent_index = pairs[idx].second ? pairs[idx].second->m_index : 0U;
        result += "(" + std::to_string(pairs[idx].first) + "," + std::to_string(parent_index) + ")";
    }
    result += "]";
    return result;
}

/**
 * @brief Hash for pairs of uint32_t, used in unordered containers.
 */
struct PairHash
{
    /**
     * @brief Compute hash for a pair of uint32_t values.
     * @param pair The pair to hash.
     * @return Combined hash value.
     */
    std::size_t operator()(const std::pair<uint32_t, uint32_t>& pair) const
    {
        return std::hash<uint32_t>{}(pair.first) ^ (std::hash<uint32_t>{}(pair.second) << 1U);
    }
};


}  // namespace

namespace sgf
{

Tree::Tree(const uint32_t root_vertex_index, const ColoredGraph& graph, const LoggerHandler& logger,
           GeneralColorHist& general_hist,
           const std::optional<std::reference_wrapper<GeneralColorHist>> reverse_general_hist)
    : m_root(std::make_shared<Node>(root_vertex_index, 0U))
    , m_depth(0U)
    , m_logger(logger)
    , m_graph(graph)
    , m_is_directed(graph.is_directed())
    , m_periphery(general_hist, root_vertex_index, logger)
    , m_reverse_periphery(reverse_general_hist ? std::optional<PatternPeriphery>(
                                                std::in_place, reverse_general_hist->get(),
                                                root_vertex_index, logger)
                                          : std::nullopt)
{
    if (m_is_directed && !m_reverse_periphery)
    {
        m_logger.log(LogLevel::ERROR, "Tree: reverse histogram required for directed tree");
        throw PatternException("Reverse histogram required for directed trees");
    }
    SGF_DEBUG_LOG(m_logger, "Tree: root=" + std::to_string(root_vertex_index));
}

NodePtr Tree::add_node(const NodePtr& parent, const uint32_t vertex_index)
{
    if (!parent)
    {
        m_logger.log(LogLevel::ERROR,
                     "add_node: parent is null for vertex=" + std::to_string(vertex_index));
        throw AddNodeException("Parent is null");
    }

    NodePtr new_node = std::make_shared<Node>(vertex_index, parent->m_depth + 1U);
    new_node->m_parent = parent;

    if (!parent->m_son)
    {
        parent->m_son = new_node;
    }
    else if (!parent->m_son->m_left)
    {
        parent->m_son->m_left = new_node;
        parent->m_son->m_right = new_node;
        new_node->m_left = parent->m_son;
        new_node->m_right = parent->m_son;
    }
    else
    {
        new_node->m_right = parent->m_son->m_right;
        new_node->m_right->m_left = new_node;
        parent->m_son->m_right = new_node;
        new_node->m_left = parent->m_son;
    }

    return new_node;
}

void Tree::splice_out_of_sibling_ring(const NodePtr& node)
{
    if (node->m_left)
    {
        if (node->m_right == node->m_left)
        {
            node->m_left->m_right.reset();
        }
        else
        {
            node->m_left->m_right = node->m_right;
        }
    }

    if (node->m_right)
    {
        if (node->m_right == node->m_left)
        {
            node->m_right->m_left.reset();
        }
        else
        {
            node->m_right->m_left = node->m_left;
        }
    }
}

void Tree::delete_node(const NodePtr& node)
{
    if (node->m_son)
    {
        m_logger.log(LogLevel::ERROR,
                     "delete_node: vertex=" + std::to_string(node->m_index) + " has children");
        throw DeleteNodeException("Cannot delete node with children");
    }

    splice_out_of_sibling_ring(node);

    const NodePtr parent_node = node->m_parent.lock();
    if (parent_node)
    {
        if (parent_node->m_son == node)
        {
            parent_node->m_son = node->m_left;
        }
        
        if (parent_node->m_previous_children.find(node->m_index) == parent_node->m_previous_children.end())
        {
            parent_node->m_previous_children[node->m_index].insert({node->m_depth});
        }
        else {
            parent_node->m_previous_children.insert({node->m_index, {{node->m_depth}}});
        }
        for (auto [index, depth_set] : node->m_previous_children)
        {
            if (parent_node->m_previous_children.find(node->m_index) != parent_node->m_previous_children.end())
            {
                parent_node->m_previous_children[index].insert(depth_set.begin(), depth_set.end());
            }
            else
            {
                parent_node->m_previous_children[index] = depth_set;
            }

        }
    }
}

void Tree::validate_parent_ordering(
    const std::vector<std::pair<uint32_t, NodePtr>>& vertex_parent_pairs) const
{
    std::unordered_set<NodePtr> previously_seen_parent_nodes;
    for (size_t pair_index = 1U; pair_index < vertex_parent_pairs.size(); ++pair_index)
    {
        if (vertex_parent_pairs[pair_index - 1U].second != vertex_parent_pairs[pair_index].second)
        {
            previously_seen_parent_nodes.insert(vertex_parent_pairs[pair_index - 1U].second);
            if (previously_seen_parent_nodes.find(vertex_parent_pairs[pair_index].second) !=
                previously_seen_parent_nodes.end())
            {
                m_logger.log(LogLevel::ERROR,
                             "validate_parent_ordering: parent reappears at pair_index=" +
                                 std::to_string(pair_index));
                throw AddNodeException("New nodes are not ordered by parent index.");
            }
        }
    }
}

std::vector<NodePtr>
Tree::attach_all_new_nodes(const std::vector<std::pair<uint32_t, NodePtr>>& vertex_parent_pairs)
{
    std::vector<NodePtr> added_nodes;
    added_nodes.reserve(vertex_parent_pairs.size());
    for (const std::pair<uint32_t, NodePtr>& vertex_parent_pair : vertex_parent_pairs)
    {
        added_nodes.push_back(add_node(vertex_parent_pair.second, vertex_parent_pair.first));
    }
    return added_nodes;
}

std::unordered_set<uint32_t> Tree::collect_sibling_vertex_indexes(
    const std::vector<std::pair<uint32_t, NodePtr>>& vertex_parent_pairs,
    size_t& current_pair_index, NodePtr& current_parent_node)
{
    std::unordered_set<uint32_t> sibling_vertex_indexes;
    sibling_vertex_indexes.insert(vertex_parent_pairs[current_pair_index].first);
    current_parent_node = vertex_parent_pairs[current_pair_index].second;

    while (current_pair_index + 1U < vertex_parent_pairs.size() &&
           vertex_parent_pairs[current_pair_index + 1U].second == current_parent_node)
    {
        ++current_pair_index;
        sibling_vertex_indexes.insert(vertex_parent_pairs[current_pair_index].first);
    }
    ++current_pair_index;
    return sibling_vertex_indexes;
}

void Tree::advance_tree_path_to_parent(const NodePtr& previous_parent_node,
                                       const NodePtr& current_parent_node,
                                       std::unordered_map<uint32_t, uint32_t>& tree_path_depths,
                                       std::vector<bool>& forward_processed_flags,
                                       std::vector<bool>& reverse_processed_flags)
{
    NodePtr previous_ancestor = previous_parent_node;
    NodePtr current_ancestor = current_parent_node;
    std::unordered_set<uint32_t> newly_added_path_vertices;

    while (previous_ancestor != current_ancestor)
    {
        tree_path_depths[current_ancestor->m_index] = current_ancestor->m_depth;
        forward_processed_flags[current_ancestor->m_index] = false;
        reverse_processed_flags[current_ancestor->m_index] = false;
        newly_added_path_vertices.insert(current_ancestor->m_index);

        if (newly_added_path_vertices.find(previous_ancestor->m_index) ==
            newly_added_path_vertices.end())
        {
            tree_path_depths.erase(previous_ancestor->m_index);
        }

        current_ancestor = current_ancestor->m_parent.lock();
        previous_ancestor = previous_ancestor->m_parent.lock();
    }
}

void Tree::update_neighbours_in_tree_path(
    const std::unordered_set<uint32_t>& candidate_indexes,
    const std::unordered_map<uint32_t, uint32_t>& tree_path_depths,
    std::vector<uint32_t>& found_depths, std::vector<bool>& vertex_processed_flags,
    const bool is_reversed) const
{

    for (const auto& [path_vertex_index, path_vertex_depth] : tree_path_depths)
    {
        if (vertex_processed_flags[path_vertex_index])
        {
            continue;
        }
        
        const std::pair<std::vector<uint32_t>::const_iterator,
                        std::vector<uint32_t>::const_iterator>
            neighbour_range = m_graph.get_neighbours(path_vertex_index, is_reversed);

        bool all_candidates_neighbours = true;
        for (auto candidate_index : candidate_indexes)
        {
            if(std::find(neighbour_range.first, neighbour_range.second, candidate_index) == neighbour_range.second)
            {
                all_candidates_neighbours = false;
                break;
            }
        }
        if (all_candidates_neighbours)
        {
            SGF_DEBUG_LOG(m_logger, "Decreasing neighbour for vertex=" + std::to_string(path_vertex_index) + " at depth=" + std::to_string(path_vertex_depth));
            found_depths.push_back(path_vertex_depth - 1U);
            vertex_processed_flags[path_vertex_index] = true;
        }
    }
}

std::vector<uint32_t> Tree::collect_vertex_external_colors(
    const uint32_t candidate_vertex, const std::unordered_map<uint32_t, uint32_t>& tree_path_depths,
    const std::unordered_map<uint32_t, std::unordered_set<uint32_t>>& excluded_previous_children,
    const bool is_reversed) const
{
    std::vector<uint32_t> collected_colors;
    std::unordered_map<uint32_t, std::unordered_set<uint32_t>> color_to_tree_level_count;
    std::unordered_map<uint32_t, uint32_t> neighbour_count_in_color;

    const std::pair<std::vector<uint32_t>::const_iterator, std::vector<uint32_t>::const_iterator>
        neighbour_range = m_graph.get_neighbours(candidate_vertex, is_reversed);

    for (std::vector<uint32_t>::const_iterator neighbour_it = neighbour_range.first;
         neighbour_it != neighbour_range.second; ++neighbour_it)
    {
        SGF_DEBUG_LOG(m_logger, "collect_vertex_external_colors checking neighbour: " + std::to_string(*neighbour_it));
        const uint32_t adjacent_vertex_index = *neighbour_it;
        if (tree_path_depths.find(adjacent_vertex_index) != tree_path_depths.end())
        {
            SGF_DEBUG_LOG(m_logger, "collect_vertex_external_colors in tree path");
            continue;
        }
        if (excluded_previous_children.find(adjacent_vertex_index) ==
            excluded_previous_children.end())
        {
            SGF_DEBUG_LOG(m_logger, "collect_vertex_external_colors not in previous children");
            collected_colors.push_back(m_graph.get_vertex_color(adjacent_vertex_index));
        }
        else
        {
            SGF_DEBUG_LOG(m_logger, "collect_vertex_external_colors in previous children");
            const std::unordered_set<uint32_t> ancestor_depth = excluded_previous_children.at(adjacent_vertex_index);
            const uint32_t color = m_graph.get_vertex_color(adjacent_vertex_index);
            for (auto depth : ancestor_depth)
            {
                SGF_DEBUG_LOG(m_logger, "collect_vertex_external_colors ancestor depth: " + std::to_string(depth));
                color_to_tree_level_count[color].insert(depth);
            }
            ++neighbour_count_in_color[color];
        }
    }

    for (const auto& [color, count] : neighbour_count_in_color)
    {
        const uint32_t unique_depths =
            static_cast<uint32_t>(color_to_tree_level_count[color].size());
        SGF_DEBUG_LOG(m_logger, "collect_vertex_external_colors color: " + std::to_string(color) + " count: " + std::to_string(count) + " unique_depths: " + std::to_string(unique_depths));
        for (uint32_t i = 0U; i < count - unique_depths; ++i)
        {
            collected_colors.push_back(color);
        }
    }

    return collected_colors;
}

std::vector<uint32_t> Tree::get_colors_of_neighbours_not_in_tree_path(
    const std::unordered_set<uint32_t>& candidate_indexes,
    const std::unordered_map<uint32_t, uint32_t>& tree_path_depths,
    const std::unordered_map<uint32_t, std::unordered_set<uint32_t>>& excluded_previous_children,
    const bool is_reversed) const
{
    std::vector<uint32_t> collected_colors;
    for (const uint32_t candidate_vertex : candidate_indexes)
    {
        const std::vector<uint32_t> vertex_colors = collect_vertex_external_colors(
            candidate_vertex, tree_path_depths, excluded_previous_children, is_reversed);
        collected_colors.insert(collected_colors.end(), vertex_colors.begin(), vertex_colors.end());
    }
    return collected_colors;
}

std::unordered_map<uint32_t, uint32_t> Tree::get_tree_path_map(const NodePtr& last_node_in_path)
{
    std::unordered_map<uint32_t, uint32_t> tree_path_depths;
    tree_path_depths.reserve(last_node_in_path->m_depth + 1U);

    NodePtr current_node = last_node_in_path;
    while (current_node && !current_node->m_parent.expired())
    {
        tree_path_depths[current_node->m_index] = current_node->m_depth;
        current_node = current_node->m_parent.lock();
    }

    return tree_path_depths;
}

NodePtr Tree::get_root() const
{
    return m_root;
}

bool Tree::is_empty() const
{
    return m_root == nullptr;
}

void Tree::process_new_level_histogram(
    const std::vector<std::pair<uint32_t, NodePtr>>& vertex_parent_pairs,
    std::vector<uint32_t>& forward_decrease_depths, std::vector<uint32_t>& reverse_decrease_depths)
{
    const uint32_t vertex_count = m_graph.vertex_count();
    std::vector<bool> forward_processed_flags(vertex_count, false);
    std::vector<bool> reverse_processed_flags(vertex_count, false);
    const std::unordered_map<uint32_t, std::unordered_set<uint32_t>> empty_excluded_previous_children;
    std::unordered_map<uint32_t, uint32_t> tree_path_depths =
        get_tree_path_map(vertex_parent_pairs[0].second);
    NodePtr previous_parent_node;
    size_t current_pair_index = 0U;

    while (current_pair_index < vertex_parent_pairs.size())
    {
        NodePtr current_parent_node;
        const std::unordered_set<uint32_t> sibling_vertex_indexes = collect_sibling_vertex_indexes(
            vertex_parent_pairs, current_pair_index, current_parent_node);

        if (previous_parent_node)
        {
            advance_tree_path_to_parent(previous_parent_node, current_parent_node, tree_path_depths,
                                        forward_processed_flags, reverse_processed_flags);
        }
        previous_parent_node = current_parent_node;

        update_neighbours_in_tree_path(sibling_vertex_indexes, tree_path_depths,
                                       forward_decrease_depths, forward_processed_flags, false);
        if (m_reverse_periphery.has_value())
        {
            update_neighbours_in_tree_path(sibling_vertex_indexes, tree_path_depths,
                                           reverse_decrease_depths, reverse_processed_flags, true);
        }

        m_periphery.added_vertex_to_pattern_add_neihgbours_to_periphery(
            m_depth - 1U,
            get_colors_of_neighbours_not_in_tree_path(sibling_vertex_indexes, tree_path_depths,
                                                      empty_excluded_previous_children, false));

        if (m_reverse_periphery.has_value())
        {
            m_reverse_periphery->added_vertex_to_pattern_add_neihgbours_to_periphery(
                m_depth - 1U,
                get_colors_of_neighbours_not_in_tree_path(sibling_vertex_indexes, tree_path_depths,
                                                          empty_excluded_previous_children, true));
        }
    }
}

std::vector<NodePtr>
Tree::add_tree_level(const std::vector<std::pair<uint32_t, NodePtr>>& vertex_parent_pairs)
{
    SGF_DEBUG_LOG(m_logger, "add_tree_level: root=" + std::to_string(m_root->m_index) +
                                " pairs=" + format_vertex_parent_pairs(vertex_parent_pairs));
    if (vertex_parent_pairs.empty())
    {
        return {};
    }

    validate_parent_ordering(vertex_parent_pairs);

    ++m_depth;

    std::vector<NodePtr> added_nodes = attach_all_new_nodes(vertex_parent_pairs);

    std::vector<uint32_t> forward_decrease_depths;
    std::vector<uint32_t> reverse_decrease_depths;
    process_new_level_histogram(vertex_parent_pairs, forward_decrease_depths,
                                reverse_decrease_depths);

    const uint32_t representative_vertex_color =
        m_graph.get_vertex_color(vertex_parent_pairs[0].first);
    m_periphery.added_vertex_to_pattern_remove_from_periphery(representative_vertex_color,
                                                forward_decrease_depths);

    if (m_reverse_periphery.has_value())
    {
        m_reverse_periphery->added_vertex_to_pattern_remove_from_periphery(representative_vertex_color,
                                                             reverse_decrease_depths);
    }

    return added_nodes;
}

void Tree::remove_node(const NodePtr& node)
{
    SGF_DEBUG_LOG(m_logger, "remove_node: root=" + std::to_string(m_root->m_index) +
                                " vertex=" + std::to_string(node->m_index) +
                                " depth=" + std::to_string(node->m_depth));
    NodePtr node_to_remove = node;
    std::unordered_map<uint32_t, uint32_t> tree_path_depths = get_tree_path_map(node_to_remove);

    while (node_to_remove)
    {
        if (node_to_remove->m_son != nullptr)
        {
            m_logger.log(LogLevel::ERROR,
                         "remove_node: vertex=" + std::to_string(node_to_remove->m_index) +
                             " has children");
            throw DeleteNodeException("Unable to delete a node with children.");
        }

        if (node_to_remove->m_depth == 0U)
        {
            m_root.reset();
            break;
        }

        m_periphery.remove_vertex_from_pattern_remove_from_periphery(
            node_to_remove->m_depth - 1U,
            get_colors_of_neighbours_not_in_tree_path({node_to_remove->m_index}, tree_path_depths,
                                                      node_to_remove->m_previous_children, false));

        if (m_reverse_periphery.has_value())
        {
            m_reverse_periphery->remove_vertex_from_pattern_remove_from_periphery(
                node_to_remove->m_depth - 1U, get_colors_of_neighbours_not_in_tree_path(
                                                  {node_to_remove->m_index}, tree_path_depths,
                                                  node_to_remove->m_previous_children, true));
        }

        const NodePtr parent_node = node_to_remove->m_parent.lock();
        delete_node(node_to_remove);

        if (parent_node && !parent_node->m_son)
        {
            tree_path_depths.erase(node_to_remove->m_index);
            node_to_remove = parent_node;
        }
        else
        {
            break;
        }
    }
}

NodePtr Tree::get_node_by_depth(const NodePtr& lowest_node_in_match, const uint32_t target_depth)
{
    NodePtr current_node = lowest_node_in_match;
    while (current_node && current_node->m_depth != target_depth)
    {
        current_node = current_node->m_parent.lock();
    }
    return current_node;
}

}  // namespace sgf
