#include "Tree.h"

#include "AddNodeException.h"
#include "ColoredGraph.h"
#include "CountsMap.h"
#include "DebugLog.h"
#include "DeleteNodeException.h"
#include "LogLevel.h"
#include "LoggerHandler.h"
#include "Node.h"

#include <cstddef>
#include <cstdint>
#include <tuple>
#include <memory>
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
[[maybe_unused]] std::string
format_vertex_parent_pairs(const std::vector<std::pair<uint32_t, sgf::NodePtr>>& pairs)
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

}  // namespace

namespace sgf
{

Tree::Tree(const uint32_t root_vertex_index, const ColoredGraph& graph, LoggerHandler logger)
    : m_root(std::make_shared<Node>(root_vertex_index, 0U))
    , m_logger(std::move(logger))
    , m_graph(graph)
    , m_is_directed(graph.is_directed())
{
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
    if (parent_node && parent_node->m_son == node)
    {
        parent_node->m_son = node->m_left;
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
    return attach_all_new_nodes(vertex_parent_pairs);
}

void Tree::remove_node(const NodePtr& node)
{
    SGF_DEBUG_LOG(m_logger, "remove_node: root=" + std::to_string(m_root->m_index) +
                                " vertex=" + std::to_string(node->m_index) +
                                " depth=" + std::to_string(node->m_depth));
    NodePtr node_to_remove = node;

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

        const NodePtr parent_node = node_to_remove->m_parent.lock();
        delete_node(node_to_remove);

        if (parent_node && !parent_node->m_son)
        {
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

void Tree::accumulate_direction_neighbour_counts(const std::vector<uint32_t>& path,
                                                 const std::unordered_set<uint32_t>& path_set,
                                                 const bool is_reversed, CountsMap& counts) const
{
    for (uint32_t pattern_index = 0U; pattern_index < static_cast<uint32_t>(path.size());
         ++pattern_index)
    {
        const std::pair<std::vector<uint32_t>::const_iterator,
                        std::vector<uint32_t>::const_iterator>
            neighbour_range = m_graph.get_neighbours(path[pattern_index], is_reversed);

        for (std::vector<uint32_t>::const_iterator neighbour_it = neighbour_range.first;
             neighbour_it != neighbour_range.second; ++neighbour_it)
        {
            if (path_set.find(*neighbour_it) == path_set.end())
            {
                ++counts[std::make_tuple(m_graph.get_vertex_color(*neighbour_it), pattern_index,
                                         is_reversed)];
            }
        }
    }
}

void Tree::accumulate_path_neighbour_counts(const std::vector<uint32_t>& path,
                                            const std::unordered_set<uint32_t>& path_set,
                                            CountsMap& counts) const
{
    accumulate_direction_neighbour_counts(path, path_set, false, counts);
    if (m_is_directed)
    {
        accumulate_direction_neighbour_counts(path, path_set, true, counts);
    }
}

void Tree::seed_path_from_leaf(const NodePtr& leaf, std::vector<uint32_t>& path_vec,
                              std::unordered_set<uint32_t>& path_set)
{
    NodePtr ancestor = leaf;
    while (ancestor && !ancestor->m_parent.expired())
    {
        path_vec[ancestor->m_depth - 1U] = ancestor->m_index;
        path_set.insert(ancestor->m_index);
        ancestor = ancestor->m_parent.lock();
    }
}

void Tree::update_path_to_next_leaf(const NodePtr& prev_leaf, const NodePtr& curr_leaf,
                                    std::vector<uint32_t>& path_vec,
                                    std::unordered_set<uint32_t>& path_set)
{
    NodePtr prev_ancestor = prev_leaf;
    NodePtr curr_ancestor = curr_leaf;
    std::vector<NodePtr> to_remove;
    std::vector<NodePtr> to_add;
    while (prev_ancestor != curr_ancestor)
    {
        to_remove.push_back(prev_ancestor);
        to_add.push_back(curr_ancestor);
        prev_ancestor = prev_ancestor->m_parent.lock();
        curr_ancestor = curr_ancestor->m_parent.lock();
    }
    for (const NodePtr& node : to_remove)
    {
        path_set.erase(node->m_index);
    }
    for (const NodePtr& node : to_add)
    {
        path_vec[node->m_depth - 1U] = node->m_index;
        path_set.insert(node->m_index);
    }
}

void Tree::get_color_by_depth_neighbour_counts(const std::vector<NodePtr>& leaves,
                                               CountsMap& counts) const
{
    if (leaves.empty())
    {
        return;
    }

    const uint32_t frontier_depth = leaves.at(0)->m_depth;
    std::vector<uint32_t> path_vec(frontier_depth, 0U);
    std::unordered_set<uint32_t> path_set;

    seed_path_from_leaf(leaves.at(0), path_vec, path_set);
    accumulate_path_neighbour_counts(path_vec, path_set, counts);

    for (uint32_t leaf_idx = 1U; leaf_idx < static_cast<uint32_t>(leaves.size()); ++leaf_idx)
    {
        update_path_to_next_leaf(leaves[leaf_idx - 1U], leaves[leaf_idx], path_vec, path_set);
        accumulate_path_neighbour_counts(path_vec, path_set, counts);
    }
}

}  // namespace sgf
