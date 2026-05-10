#pragma once

#include "Node.h"
#include "Graph.h"
#include "IndevidualColorHist.h"
#include <boost/optional.hpp>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <stack>
#include <memory>
#include <cstdint>
#include <atomic>

/**
 * @class Tree
 * @brief Dynamic tree structure for pattern expansion and backtracking.
 *
 * Uses a first-child / sibling representation:
 * - son   : first child
 * - left  : left sibling
 * - right : right sibling
 * - parent: weak_ptr to avoid ownership cycles
 */
class Tree {
private:
    NodePtr m_root;          ///< Root node of the tree
    int32_t depth;           ///< Maximum depth reached
    bool is_direcred;        ///< Whether the graphs are directed
    IndevidualColorHist hist;       ///< Color histogram for heuristic updates
    boost::optional<IndevidualColorHist> reverse_hist;       ///< Color histogram for heuristic updates in reverse direction

private:

    /**
     * @brief Add a node as a child of a given parent.
     */
    NodePtr _add_node(const NodePtr& node_parent, int32_t index_in_s);

    /**
     * @brief Delete a leaf node from the tree.
     */
    void _delete_node(const NodePtr& node);

    /**
    * @brief Get neighbors in S that are already in the tree path.
    * @return Vector of pairs (depth in pattern, color pf vertex)
    */
    void _update_neighbours_in_tree_path(
        std::unordered_set<uint32_t> indexes_in_s, 
        const std::vector<Graph>& s_list,
        const std::unordered_map<uint32_t, uint32_t>& path_in_tree,
        std::vector<uint32_t>& found_neibours_in_tree_path,
        std::vector<bool>& vertex_already_processed,
        bool is_reversed);

    /**
    * @brief Get neighbors in S that are not in the tree path.
    * @return Vector ( color of vertex)
    */
    std::vector<uint32_t> _get_colors_of_neighbours_not_in_tree_path(
        std::unordered_set<uint32_t> indexes_in_s, 
        const std::vector<Graph>& s_list,
        std::unordered_map<uint32_t, uint32_t> path_in_tree,
        std::unordered_set<uint32_t>& previous_children,
        bool is_reversed);


public:
    /**
     * @brief Construct tree with a root node.
     */
    Tree(int32_t s_index, bool is_directed,
         GeneralColorHist& general_hist, GeneralColorHist* reverse_general_hist = nullptr);

    /**
     * @brief Destructor clears the entire tree.
     */
    //~Tree();
    ~Tree()=default;

    /**
     * @brief Get map of pattern index → depth along a tree path.
     */
    std::unordered_map<uint32_t, uint32_t>
    get_tree_path_map(const NodePtr& last_node_in_path);

    /// @return Root node
    NodePtr get_root();

    /// @return True if tree is empty
    bool is_empty();

    /**
     * @brief Add a new level under a parent node.
     */
    std::vector<NodePtr>
    add_tree_level(const std::vector<std::pair<uint32_t, NodePtr>>& new_indexes,
                   const std::vector<Graph>& s_list,
                   bool is_directed);

    /**
     * @brief Remove a node and backtrack if needed.
     */
    void remove_node(const NodePtr& node,
                     const std::vector<Graph>& s_list,
                     bool is_directed);

    /**
     * @brief Get ancestor of a node at a given depth.
     */
    NodePtr get_node_by_depth(const NodePtr& lowest_node_in_match,
                              int32_t depth);
};