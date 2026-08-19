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
#include <memory>
#include <string>
#include <tuple>
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

/**
 * @brief Keep only elements of @p target that are also present in @p other.
 * @param target Set intersected in-place.
 * @param other  Set to intersect against.
 */
void intersect_path_set_in_place(std::unordered_set<uint32_t>& target,
                                 const std::unordered_set<uint32_t>& other)
{
    for (std::unordered_set<uint32_t>::iterator element_it = target.begin();
         element_it != target.end();)
    {
        if (other.find(*element_it) == other.end())
        {
            element_it = target.erase(element_it);
        }
        else
        {
            ++element_it;
        }
    }
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

Tree::~Tree()
{
    if (m_root)
    {
        detach_subtree(m_root);
    }
}

void Tree::detach_subtree(const NodePtr& node)
{
    std::vector<NodePtr> pending_nodes;
    pending_nodes.push_back(node);

    while (!pending_nodes.empty())
    {
        const NodePtr current_node = pending_nodes.back();
        pending_nodes.pop_back();

        const NodePtr first_child = current_node->m_son;
        NodePtr current_child = first_child;
        while (current_child)
        {
            const NodePtr next_child = current_child->m_right;
            pending_nodes.push_back(current_child);
            current_child->m_left.reset();
            current_child->m_right.reset();
            current_child = (next_child == first_child) ? nullptr : next_child;
        }
        current_node->m_son.reset();
    }
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

    node->m_left.reset();
    node->m_right.reset();
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
    if (vertex_parent_pairs.empty())
    {
        return {};
    }

    validate_parent_ordering(vertex_parent_pairs);
    std::vector<NodePtr> added_nodes = attach_all_new_nodes(vertex_parent_pairs);
    return added_nodes;
}

void Tree::remove_node(const NodePtr& node)
{
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

void Tree::accumulate_direction_neighbour_counts(const NodePtr& vertex_node,
                                                 const std::unordered_set<uint32_t>& path_set,
                                                 const bool is_reversed, CountsMap& counts) const
{
    const uint32_t pattern_index = vertex_node->m_depth - 1U;
    const std::pair<std::vector<uint32_t>::const_iterator, std::vector<uint32_t>::const_iterator>
        neighbour_range = m_graph.get_neighbours(vertex_node->m_index, is_reversed);

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

void Tree::accumulate_vertex_neighbour_counts(const NodePtr& vertex_node,
                                              const std::unordered_set<uint32_t>& path_set,
                                              CountsMap& counts) const
{
    accumulate_direction_neighbour_counts(vertex_node, path_set, false, counts);
    if (m_is_directed)
    {
        accumulate_direction_neighbour_counts(vertex_node, path_set, true, counts);
    }
}

void Tree::seed_path_from_leaf(const NodePtr& leaf, std::unordered_set<uint32_t>& path_set)
{
    NodePtr ancestor = leaf;
    while (ancestor && !ancestor->m_parent.expired())
    {
        path_set.insert(ancestor->m_index);
        ancestor = ancestor->m_parent.lock();
    }
}

void Tree::update_path_to_next_leaf(const NodePtr& prev_leaf, const NodePtr& curr_leaf,
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
        path_set.insert(node->m_index);
    }
}

void Tree::build_leaf_frontier_paths(const std::vector<NodePtr>& leaves,
                                     std::vector<NodePtr>& frontier_nodes,
                                     std::vector<std::unordered_set<uint32_t>>& frontier_paths)
{
    frontier_nodes = leaves;
    frontier_paths.reserve(leaves.size());

    std::unordered_set<uint32_t> running_path;
    seed_path_from_leaf(leaves.front(), running_path);
    frontier_paths.push_back(running_path);

    for (uint32_t leaf_idx = 1U; leaf_idx < static_cast<uint32_t>(leaves.size()); ++leaf_idx)
    {
        update_path_to_next_leaf(leaves[leaf_idx - 1U], leaves[leaf_idx], running_path);
        frontier_paths.push_back(running_path);
    }
}

void Tree::accumulate_frontier_neighbour_counts(
    const std::vector<NodePtr>& frontier_nodes,
    const std::vector<std::unordered_set<uint32_t>>& frontier_paths, CountsMap& counts) const
{
    for (uint32_t node_idx = 0U; node_idx < static_cast<uint32_t>(frontier_nodes.size());
         ++node_idx)
    {
        accumulate_vertex_neighbour_counts(frontier_nodes[node_idx], frontier_paths[node_idx],
                                           counts);
    }
}

void Tree::advance_frontier_to_parents(std::vector<NodePtr>& frontier_nodes,
                                       std::vector<std::unordered_set<uint32_t>>& frontier_paths)
{
    std::vector<NodePtr> parent_nodes;
    std::vector<std::unordered_set<uint32_t>> parent_paths;
    parent_nodes.reserve(frontier_nodes.size());
    parent_paths.reserve(frontier_paths.size());

    for (uint32_t node_idx = 0U; node_idx < static_cast<uint32_t>(frontier_nodes.size());
         ++node_idx)
    {
        const NodePtr parent_node = frontier_nodes[node_idx]->m_parent.lock();
        if (!parent_nodes.empty() && parent_nodes.back() == parent_node)
        {
            intersect_path_set_in_place(parent_paths.back(), frontier_paths[node_idx]);
        }
        else
        {
            parent_nodes.push_back(parent_node);
            parent_paths.push_back(frontier_paths[node_idx]);
        }
    }

    frontier_nodes = std::move(parent_nodes);
    frontier_paths = std::move(parent_paths);
}

void Tree::get_color_by_depth_neighbour_counts(const std::vector<NodePtr>& leaves,
                                               CountsMap& counts) const
{
    if (leaves.empty())
    {
        return;
    }

    std::vector<NodePtr> frontier_nodes;
    std::vector<std::unordered_set<uint32_t>> frontier_paths;
    build_leaf_frontier_paths(leaves, frontier_nodes, frontier_paths);

    while (true)
    {
        accumulate_frontier_neighbour_counts(frontier_nodes, frontier_paths, counts);
        if (frontier_nodes.front()->m_depth == 1U)
        {
            break;
        }
        advance_frontier_to_parents(frontier_nodes, frontier_paths);
    }
}

}  // namespace sgf
