#include "Tree.h"
#include <stdexcept>
#include <algorithm>
#include <vector>
#include <iostream>
#include <atomic>
#include <unordered_set>

/* ---------- Constructor ---------- */
Tree::Tree(int32_t s_index, bool is_directed,
           GeneralColorHist& general_hist, GeneralColorHist* reverse_general_hist)
    : depth(0),
      is_direcred(is_directed),
      hist(IndevidualColorHist(general_hist)),
      reverse_hist(reverse_general_hist
                   ? boost::optional<IndevidualColorHist>(IndevidualColorHist(*reverse_general_hist))
                   : boost::none)
{
    if (is_direcred && !reverse_hist)
        throw std::runtime_error("Reverse histogram is required for directed trees");
    m_root = std::make_shared<Node>(s_index, 0);
}

/* ---------- Destructor ---------- */

// Tree::~Tree()
// {
//     if (!m_root) return;

//     std::stack<NodePtr> stack;
//     stack.push(m_root);

//     while (!stack.empty()) {
//         NodePtr v = stack.top();
//         stack.pop();

//         NodePtr current_son = v->son;
//         while (current_son) {
//             stack.push(current_son);
//             current_son = current_son->left;
//         }

//         v->parent.reset();
//         v->son.reset();
//         v->left.reset();
//         v->right.reset();
//     }

//     m_root.reset();
// }

/* ---------- Private methods ---------- */

NodePtr Tree::_add_node(const NodePtr& node_parent, int32_t index_in_s)
{
    if (!node_parent)
        throw std::runtime_error("Parent is null");

    NodePtr new_node = std::make_shared<Node>(index_in_s,
                                              node_parent->depth + 1);
    new_node->parent = node_parent;

    // parent has no children
    if (!node_parent->son) {
        node_parent->son = new_node;
    }
    // parent has exactly one child
    else if (!node_parent->son->left) {
        node_parent->son->left = new_node;
        node_parent->son->right = new_node;
        new_node->left = node_parent->son;
        new_node->right = node_parent->son;
    }
    // parent has two or more children
    else {
        new_node->right = node_parent->son->right;
        new_node->right->left = new_node;
        node_parent->son->right = new_node;
        new_node->left = node_parent->son;
    }

    return new_node;
}

void Tree::_delete_node(const NodePtr& node)
{
    auto parent = node->parent.lock();

    if (node->son)
        throw std::runtime_error("Cannot delete node with children");

    if (node->left) {
        if (node->right == node->left)
            node->left->right.reset();
        else
            node->left->right = node->right;
    }

    if (node->right) {
        if (node->right == node->left)
            node->right->left.reset();
        else
            node->right->left = node->left;
    }

    if (parent != nullptr) {
        if (parent->son == node)
            parent->son = node->left;
        parent->previous_children.insert(node->index);
        parent->previous_children.insert(node->previous_children.begin(), node->previous_children.end());
    }

}

void Tree::_update_neighbours_in_tree_path(
    std::unordered_set<uint32_t> indexes_in_s, 
    const std::vector<Graph>& s_list,
    const std::unordered_map<uint32_t, uint32_t>& path_in_tree,
    std::vector<uint32_t>& found_neibours_in_tree_path,
    std::vector<bool>& vertex_already_processed,
    bool is_reversed)
{
    // For each vertex in the tree path, check if it's a neighbor of any new child
    const Graph& graph = s_list[this->m_root->index];
    
    for (const auto& vertex_depth_pair : path_in_tree)
    {
        uint32_t tree_vertex = vertex_depth_pair.first;
        uint32_t depth = vertex_depth_pair.second;
        
        // Skip if this vertex was already processed
        if (vertex_already_processed[tree_vertex])
        {
            continue;
        }
        
        // Check if this tree vertex is a neighbor of any new child
        auto[neighbour_begin, neighbour_end] = graph.get_neighbours(tree_vertex, is_reversed);
        for (auto neighbour_it = neighbour_begin; neighbour_it != neighbour_end; ++neighbour_it)
        {
            uint32_t neighbour = *neighbour_it;
            if (indexes_in_s.find(neighbour) != indexes_in_s.end())
            {
                // This tree vertex is a neighbor of at least one new child
                found_neibours_in_tree_path.push_back(depth - 1);
                vertex_already_processed[tree_vertex] = true;  // Mark this vertex as processed
                break;  // Only add once per tree vertex
            }
        }
    }
}


std::vector<uint32_t> Tree::_get_colors_of_neighbours_not_in_tree_path(
     std::unordered_set<uint32_t> indexes_in_s, 
     const std::vector<Graph>& s_list,
     std::unordered_map<uint32_t, uint32_t> path_in_tree,
    std::unordered_set<uint32_t>& previous_children,
    bool is_reversed)
{
    // return all the neighbours of the indexes in s that are also not in the tree path
    std::vector<uint32_t> neighbours_in_s_not_in_tree_path;
    for (uint32_t index_in_s : indexes_in_s)
    {
        auto src_vertex = index_in_s;
        auto[first_neigbhour, last_neighbour] = s_list[this->m_root->index].get_neighbours(src_vertex, is_reversed);
        for (auto edge = first_neigbhour; edge != last_neighbour; ++edge) {
            uint32_t neighbour_index = *edge;
            if (path_in_tree.find(neighbour_index) == path_in_tree.end() && 
                previous_children.find(neighbour_index) == previous_children.end())
            {
                neighbours_in_s_not_in_tree_path.push_back(s_list[this->m_root->index].get_vertex_color(neighbour_index));
            }
        }
    }     
    return neighbours_in_s_not_in_tree_path;
}


/* ---------- Public API ---------- */

std::unordered_map<uint32_t, uint32_t>
Tree::get_tree_path_map(const NodePtr& last_node_in_path)
{
    std::unordered_map<uint32_t, uint32_t> path;
    path.reserve(last_node_in_path->depth + 1); // Pre-allocate
    
    NodePtr current = last_node_in_path;

    while (current && !current->parent.expired()) {
        path[current->index] = current->depth;
        current = current->parent.lock();
    }

    return path;
}

NodePtr Tree::get_root()
{
    return m_root;
}

bool Tree::is_empty()
{
    return m_root == nullptr;
}

std::vector<NodePtr>
Tree::add_tree_level(const std::vector<std::pair<uint32_t, NodePtr>>& new_indexes,
                     const std::vector<Graph>& s_list, bool is_directed)
{
    std::vector<NodePtr> added_nodes;
    if (!new_indexes.empty()) {
        // initial path in tree
        std::unordered_map<uint32_t, uint32_t> path_in_tree =
            get_tree_path_map(new_indexes[0].second);

        depth = depth + 1;

        for (std::pair<uint32_t, NodePtr> idx : new_indexes)
        {
            added_nodes.push_back(_add_node(idx.second, idx.first));
        }

        // update histogram
        int new_child_index = 0;
        NodePtr last_parent_node = nullptr;
        std::vector<uint32_t> decrease_neighbours_in_hist;
        std::vector<uint32_t> decrease_neighbours_in_hist_reverse;
        std::unordered_set<uint32_t> empty_previous_children;
        
        // Track which tree path vertices have already been processed
        // Index: vertex index in S, Value: whether already added to decrease_neighbours_in_hist
        std::vector<bool> vertex_already_processed(s_list[this->m_root->index].vertex_count(), false);
        std::vector<bool> vertex_already_processed_reversed(s_list[this->m_root->index].vertex_count(), false);
        
        while (new_child_index < new_indexes.size())
        {
            // get all children of the same parent
            std::unordered_set<uint32_t> new_indexes_same_parent;
            new_indexes_same_parent.insert(new_indexes[new_child_index].first);
            NodePtr current_parent = new_indexes[new_child_index].second;
            while(new_child_index + 1 < new_indexes.size() && new_indexes[new_child_index + 1].second == current_parent)
            {
                new_indexes_same_parent.insert(new_indexes[new_child_index + 1].first);
                new_child_index++;
            }
            new_child_index++;

            // update tree path for parent
            if (last_parent_node != nullptr)
            {
                NodePtr last_parent_iterate = last_parent_node;
                NodePtr current_parent_iterate = current_parent;
                std::unordered_set<uint32_t> replaced_value_in_key;
                while(last_parent_iterate != current_parent_iterate)
                {
                    path_in_tree[current_parent_iterate->index] = current_parent_iterate->depth;
                    // Reset processed flag for vertices that changed in the tree path
                    vertex_already_processed[current_parent_iterate->index] = false;
                    vertex_already_processed_reversed[current_parent_iterate->index] = false;
                    replaced_value_in_key.insert(current_parent_iterate->index);
                    if (replaced_value_in_key.find(last_parent_iterate->index) == replaced_value_in_key.end())
                    {
                        path_in_tree.erase(last_parent_iterate->index);
                    }
                    
                    current_parent_iterate = current_parent_iterate->parent.lock();
                    last_parent_iterate = last_parent_iterate->parent.lock();
                }
            }

            last_parent_node = current_parent;
            _update_neighbours_in_tree_path(new_indexes_same_parent, s_list, path_in_tree, decrease_neighbours_in_hist, 
                vertex_already_processed, false);
            
            if (is_direcred)
            {
                _update_neighbours_in_tree_path(new_indexes_same_parent, s_list, path_in_tree, decrease_neighbours_in_hist_reverse, 
                    vertex_already_processed_reversed, true);
            }
            const std::vector<uint32_t> update_in_hist =
                _get_colors_of_neighbours_not_in_tree_path(new_indexes_same_parent, s_list, path_in_tree, empty_previous_children, false);

            hist.update_neigbours_add_node_add_neighbours_to_hist(
                this->depth-1, update_in_hist);

            if (is_direcred)
            {
                const std::vector<uint32_t> update_in_hist_reverse =
                    _get_colors_of_neighbours_not_in_tree_path(new_indexes_same_parent, s_list, path_in_tree, empty_previous_children, true);

                reverse_hist.value().update_neigbours_add_node_add_neighbours_to_hist(
                    this->depth-1, update_in_hist_reverse);
            }
        }

        uint32_t color = s_list[this->m_root->index].get_vertex_color(new_indexes[0].first);
        hist.update_hist_decrease_from_neighbours(
            color, decrease_neighbours_in_hist);

        if (is_directed)
        {
            reverse_hist.value().update_hist_decrease_from_neighbours(
                color, decrease_neighbours_in_hist_reverse);
        }
    }

    return added_nodes;
}

void Tree::remove_node(const NodePtr& node,
                       const std::vector<Graph>& s_list,
                       bool is_directed)
{
    NodePtr node_to_remove = node;
    std::unordered_map<uint32_t, uint32_t> path_in_tree =
        get_tree_path_map(node_to_remove);

    while (node_to_remove) {
        NodePtr parent = nullptr;

        if (node_to_remove->depth != 0)
        {
            const std::vector<uint32_t> update_in_hist =
                _get_colors_of_neighbours_not_in_tree_path({node_to_remove->index}, s_list, path_in_tree, node_to_remove->previous_children, false);

            hist.update_neigbours_remove_node_decrease_neighbours_from_hist(
                node_to_remove->depth-1, update_in_hist);

            if (is_direcred)
            {
                const std::vector<uint32_t> update_in_hist_reverse =
                    _get_colors_of_neighbours_not_in_tree_path({node_to_remove->index}, s_list, path_in_tree, node_to_remove->previous_children, true);

                reverse_hist.value().update_neigbours_remove_node_decrease_neighbours_from_hist(
                    node_to_remove->depth-1, update_in_hist_reverse);
            }
            
            parent = node_to_remove->parent.lock();
            _delete_node(node_to_remove);
        }
        else{
            m_root.reset();
        }

        if (parent && !parent->son)
        {
            path_in_tree.erase(node_to_remove->index);
            node_to_remove = parent;
        }
        else
            break;
    }
}

NodePtr Tree::get_node_by_depth(const NodePtr& lowest_node_in_match,
                                int32_t target_depth)
{
    NodePtr current = lowest_node_in_match;

    while (current && current->depth != target_depth)
        current = current->parent.lock();

    return current;
}