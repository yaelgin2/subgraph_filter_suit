#include "Tree.h"

#include <algorithm>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace sgf
{

Tree::Tree(const uint32_t root_vertex_index,
           const bool is_directed,
           GeneralColorHist& general_hist,
           const std::optional<std::reference_wrapper<GeneralColorHist>> reverse_general_hist)
    : m_root(std::make_shared<Node>(root_vertex_index, 0U))
    , m_depth(0U)
    , m_is_directed(is_directed)
    , m_hist(general_hist)
    // Wrap the optional reference in an IndividualColorHist so the member owns a stable copy;
    // boost::none signals "no reverse histogram" for undirected trees.
    , m_reverse_hist(reverse_general_hist
                         ? boost::optional<IndividualColorHist>(
                               IndividualColorHist(reverse_general_hist->get()))
                         : boost::none)
{
    // A directed tree must track in-edges through the reverse histogram;
    // failing to supply one is a programming error, not a runtime condition.
    if (m_is_directed && !m_reverse_hist)
    {
        throw std::runtime_error("Reverse histogram required for directed trees");
    }
}

NodePtr Tree::add_node(const NodePtr& parent, const uint32_t vertex_index)
{
    if (!parent)
    {
        throw std::runtime_error("Parent is null");
    }

    NodePtr new_node = std::make_shared<Node>(vertex_index, parent->m_depth + 1U);
    new_node->m_parent = parent;

    // Siblings are kept in a circular doubly-linked ring.
    // m_son always points to the first (leftmost) sibling;
    // m_left / m_right are null only when there is exactly one child.

    // parent has no children
    if (!parent->m_son)
    {
        parent->m_son = new_node;
    }
    // parent has exactly one child — form a 2-node ring
    else if (!parent->m_son->m_left)
    {
        parent->m_son->m_left = new_node;
        parent->m_son->m_right = new_node;
        new_node->m_left = parent->m_son;
        new_node->m_right = parent->m_son;
    }
    // parent has two or more children — insert new_node immediately before m_son
    // (i.e. at the right end of the ring, adjacent to the current last sibling)
    else
    {
        new_node->m_right = parent->m_son->m_right;
        new_node->m_right->m_left = new_node;
        parent->m_son->m_right = new_node;
        new_node->m_left = parent->m_son;
    }

    return new_node;
}

void Tree::delete_node(const NodePtr& node)
{
    // Only leaf nodes may be removed; removing an internal node would
    // orphan its subtree without updating the histogram.
    if (node->m_son)
    {
        throw std::runtime_error("Cannot delete node with children");
    }

    // Splice node out of the circular sibling ring.
    // When the ring has exactly two nodes, m_left == m_right == the sole sibling,
    // so after removal that sibling has no ring partners — both pointers become null.
    if (node->m_left)
    {
        if (node->m_right == node->m_left)
        {
            // Only one other sibling remains; collapse the ring.
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
            // Symmetric collapse for the right pointer.
            node->m_right->m_left.reset();
        }
        else
        {
            node->m_right->m_left = node->m_left;
        }
    }

    NodePtr parent = node->m_parent.lock();
    if (parent)
    {
        // If this was the first child, advance m_son to the next sibling
        // (m_left is the right-most sibling, so it becomes the new head, or null
        // if node was the only child).
        if (parent->m_son == node)
        {
            parent->m_son = node->m_left;
        }
        // Accumulate this node's index and all its prior children in the parent so
        // future histogram queries can exclude previously visited vertices.
        parent->m_previous_children.insert(node->m_index);
        parent->m_previous_children.insert(
            node->m_previous_children.begin(), node->m_previous_children.end());
    }
}

void Tree::update_neighbours_in_tree_path(
    const std::unordered_set<uint32_t>& candidate_indexes,
    const std::vector<ColoredGraph>& s_list,
    const std::unordered_map<uint32_t, uint32_t>& path_in_tree,
    std::vector<uint32_t>& found_depths,
    std::vector<bool>& vertex_already_processed,
    const bool is_reversed) const
{
    // For each vertex in the tree path, check if it's a neighbor of any new child
    const ColoredGraph& graph = s_list[m_root->m_index];

    for (const auto& vertex_depth_pair : path_in_tree)
    {
        const uint32_t tree_vertex = vertex_depth_pair.first;
        const uint32_t tree_depth = vertex_depth_pair.second;

        // Skip if this vertex was already processed
        if (vertex_already_processed[tree_vertex])
        {
            continue;
        }
        // Check if this tree vertex is a neighbor of any new child
        auto [neighbour_begin, neighbour_end] = graph.get_neighbours(tree_vertex, is_reversed);
        for (auto neighbour_it = neighbour_begin; neighbour_it != neighbour_end; ++neighbour_it)
        {
            if (candidate_indexes.find(*neighbour_it) != candidate_indexes.end())
            {
                // This tree vertex is a neighbor of at least one new child
                found_depths.push_back(tree_depth - 1U);
                vertex_already_processed[tree_vertex] = true;  // Mark this vertex as processed
                break;  // Only add once per tree vertex

            }
        }
    }
}

std::vector<uint32_t> Tree::get_colors_of_neighbours_not_in_tree_path(
    const std::unordered_set<uint32_t>& candidate_indexes,
    const std::vector<ColoredGraph>& s_list,
    const std::unordered_map<uint32_t, uint32_t>& path_in_tree,
    const std::unordered_set<uint32_t>& excluded_previous_children,
    const bool is_reversed) const
{
    // return all the neighbours of the indexes in s that are also not in the tree path
    std::vector<uint32_t> colours;
    const ColoredGraph& graph = s_list[m_root->m_index];

    for (const uint32_t candidate_vertex : candidate_indexes)
    {
        auto [first_neighbour, last_neighbour] = graph.get_neighbours(candidate_vertex, is_reversed);
        for (auto edge = first_neighbour; edge != last_neighbour; ++edge)
        {
            const uint32_t neighbour_index = *edge;
            if (path_in_tree.find(neighbour_index) == path_in_tree.end() &&
                excluded_previous_children.find(neighbour_index) == excluded_previous_children.end())
            {
                colours.push_back(graph.get_vertex_color(neighbour_index));
            }
        }
    }

    return colours;
}

std::unordered_map<uint32_t, uint32_t>
Tree::get_tree_path_map(const NodePtr& last_node_in_path) const
{
    std::unordered_map<uint32_t, uint32_t> path;
    // Reserve exact capacity: path length equals depth + 1 (inclusive of starting node).
    path.reserve(last_node_in_path->m_depth + 1U);

    NodePtr current = last_node_in_path;
    // Walk up the ancestor chain; stop at the root (whose m_parent is expired/null).
    while (current && !current->m_parent.expired())
    {
        path[current->m_index] = current->m_depth;
        current = current->m_parent.lock();
    }

    return path;
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
Tree::add_tree_level(const std::vector<std::pair<uint32_t, NodePtr>>& new_indexes,
                     const std::vector<ColoredGraph>& s_list)
{
    std::vector<NodePtr> added_nodes;

    if (new_indexes.empty())
    {
        return added_nodes;
    }

    // Build the tree-path map from the first new child's parent up to the root.
    // All new children share the same tree path initially; the map is incrementally
    // updated as we move between different parents below.
    std::unordered_map<uint32_t, uint32_t> path_in_tree =
        get_tree_path_map(new_indexes[0].second);

    ++m_depth;

    // Attach all new nodes to the tree before updating the histograms.
    for (const std::pair<uint32_t, NodePtr>& idx : new_indexes)
    {
        added_nodes.push_back(add_node(idx.second, idx.first));
    }

    // update histogram
    const uint32_t vertex_count = s_list[m_root->m_index].vertex_count();
    // Track which tree path vertices have already been processed
    // Index: vertex index in S, Value: whether already added to decrease_neighbours_in_hist
    std::vector<bool> vertex_already_processed(vertex_count, false);
    std::vector<bool> vertex_already_processed_reversed(vertex_count, false);
    // Depths of tree-path nodes whose neighbour counts must be decremented because a
    // new child is now inside the neighbourhood (and no longer qualifies as "external").
    std::vector<uint32_t> decrease_depths;
    std::vector<uint32_t> decrease_depths_reverse;
    const std::unordered_set<uint32_t> empty_excluded;

    uint32_t new_child_index = 0U;
    NodePtr last_parent_node;

    // Process each group of siblings that share the same parent together so that
    // a single call to update_neighbours_in_tree_path covers all siblings at once,
    // avoiding double-counting neighbours shared between siblings.
    while (new_child_index < new_indexes.size())
    {
        // get all children of the same parent
        std::unordered_set<uint32_t> same_parent_indexes;
        same_parent_indexes.insert(new_indexes[new_child_index].first);
        const NodePtr current_parent = new_indexes[new_child_index].second;

        while (new_child_index + 1U < new_indexes.size() &&
               new_indexes[new_child_index + 1U].second == current_parent)
        {
            ++new_child_index;
            same_parent_indexes.insert(new_indexes[new_child_index].first);
        }
        ++new_child_index;

        // update tree path for parent
        // Walk the diverging ancestor chains of last_parent and current_parent
        // simultaneously until they converge; nodes on current_parent's exclusive
        // branch are added to the path map, while nodes that were only on
        // last_parent's branch are removed.
        if (last_parent_node)
        {
            NodePtr last_iter = last_parent_node;
            NodePtr current_iter = current_parent;
            std::unordered_set<uint32_t> updated_keys;

            while (last_iter != current_iter)
            {
                path_in_tree[current_iter->m_index] = current_iter->m_depth;
                // Reset processed flag for vertices that changed in the tree path
                vertex_already_processed[current_iter->m_index] = false;
                vertex_already_processed_reversed[current_iter->m_index] = false;
                updated_keys.insert(current_iter->m_index);

                // Only erase from the path if this node was not just added on the
                // current_parent side (it may appear in both chains).
                if (updated_keys.find(last_iter->m_index) == updated_keys.end())
                {
                    path_in_tree.erase(last_iter->m_index);
                }

                current_iter = current_iter->m_parent.lock();
                last_iter = last_iter->m_parent.lock();
            }
        }

        last_parent_node = current_parent;

        // Identify tree-path vertices that are neighbours of the new children;
        // those vertices must have their histogram entry decremented (they are now
        // "internal" to the tree path).
        update_neighbours_in_tree_path(
            same_parent_indexes, s_list, path_in_tree,
            decrease_depths, vertex_already_processed, false);

        if (m_is_directed)
        {
            update_neighbours_in_tree_path(
                same_parent_indexes, s_list, path_in_tree,
                decrease_depths_reverse, vertex_already_processed_reversed, true);
        }

        // Add the colors of external neighbours (not in the tree path) of the new
        // children to the histogram so they are counted at the new depth.
        const std::vector<uint32_t> add_colours =
            get_colors_of_neighbours_not_in_tree_path(
                same_parent_indexes, s_list, path_in_tree, empty_excluded, false);

        m_hist.update_neigbours_add_node_add_neighbours_to_hist(m_depth - 1U, add_colours);

        if (m_is_directed)
        {
            const std::vector<uint32_t> add_colours_reverse =
                get_colors_of_neighbours_not_in_tree_path(
                    same_parent_indexes, s_list, path_in_tree, empty_excluded, true);

            m_reverse_hist.value().update_neigbours_add_node_add_neighbours_to_hist(
                m_depth - 1U, add_colours_reverse);
        }
    }

    // All new children share the same color (same level, same s_list graph);
    // use the first one as representative.
    const uint32_t new_vertex_color =
        s_list[m_root->m_index].get_vertex_color(new_indexes[0].first);

    // Decrement the histogram at the recorded depths to reflect that the tree-path
    // vertices whose depths were collected are no longer external neighbours.
    m_hist.update_hist_decrease_from_neighbours(new_vertex_color, decrease_depths);

    if (m_is_directed)
    {
        m_reverse_hist.value().update_hist_decrease_from_neighbours(
            new_vertex_color, decrease_depths_reverse);
    }

    return added_nodes;
}

void Tree::remove_node(const NodePtr& node, const std::vector<ColoredGraph>& s_list)
{
    NodePtr node_to_remove = node;
    // Snapshot the path from the starting node to the root before any structural
    // changes so that histogram queries during backtracking use a consistent view.
    std::unordered_map<uint32_t, uint32_t> path_in_tree =
        get_tree_path_map(node_to_remove);

    // Backtrack up the ancestor chain as long as each parent becomes childless after
    // its last child is deleted, because an internal node with no children is
    // semantically equivalent to a leaf and must also be removed.
    while (node_to_remove)
    {
        // Reached the root — clear the tree entirely.
        if (node_to_remove->m_depth == 0U)
        {
            m_root.reset();
            break;
        }

        // Undo the histogram contribution of this node: remove the colors of
        // neighbours that are external to the tree path (those were added when
        // this node was inserted and must be subtracted now).
        const std::vector<uint32_t> remove_colours =
            get_colors_of_neighbours_not_in_tree_path(
                {node_to_remove->m_index}, s_list, path_in_tree,
                node_to_remove->m_previous_children, false);

        m_hist.update_neigbours_remove_node_decrease_neighbours_from_hist(
            node_to_remove->m_depth - 1U, remove_colours);

        if (m_is_directed)
        {
            const std::vector<uint32_t> remove_colours_reverse =
                get_colors_of_neighbours_not_in_tree_path(
                    {node_to_remove->m_index}, s_list, path_in_tree,
                    node_to_remove->m_previous_children, true);

            m_reverse_hist.value().update_neigbours_remove_node_decrease_neighbours_from_hist(
                node_to_remove->m_depth - 1U, remove_colours_reverse);
        }

        NodePtr parent = node_to_remove->m_parent.lock();
        delete_node(node_to_remove);

        // If the parent has no remaining children, it is now a leaf — continue
        // backtracking upward. Remove the just-deleted node from the path map so
        // subsequent queries see the updated tree path.
        if (parent && !parent->m_son)
        {
            path_in_tree.erase(node_to_remove->m_index);
            node_to_remove = parent;
        }
        else
        {
            // Parent still has other children; stop here.
            break;
        }
    }
}

NodePtr Tree::get_node_by_depth(const NodePtr& lowest_node_in_match,
                                 const uint32_t target_depth) const
{
    NodePtr current = lowest_node_in_match;
    while (current && current->m_depth != target_depth)
    {
        current = current->m_parent.lock();
    }
    return current;
}

}  // namespace sgf
