#pragma once

#include "ColoredGraph.h"
#include "GeneralColorHist.h"
#include "IndividualColorHist.h"
#include "Node.h"

#include <boost/optional.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sgf
{

/**
 * @class Tree
 * @brief Dynamic tree structure for pattern expansion and backtracking.
 *
 * Uses a first-child / sibling representation:
 * - m_son:    first child
 * - m_left:   left sibling
 * - m_right:  right sibling
 * - m_parent: weak_ptr to avoid ownership cycles
 *
 * For directed graphs a reverse histogram must be supplied alongside the forward one.
 */
class Tree
{
public:
    /**
     * @brief Construct a tree.
     * @param root_vertex_index Source-graph vertex index for the root node.
     * @param is_directed Whether the underlying graph is directed.
     * @param general_hist Shared general histogram for forward edges.
     * @param reverse_general_hist Shared general histogram for reverse edges (required when directed).
     */
    Tree(uint32_t root_vertex_index,
         bool is_directed,
         GeneralColorHist& general_hist,
         std::optional<std::reference_wrapper<GeneralColorHist>> reverse_general_hist = std::nullopt);

    Tree() = delete;
    Tree(const Tree&) = delete;
    Tree& operator=(const Tree&) = delete;
    Tree(Tree&&) = delete;
    Tree& operator=(Tree&&) = delete;

    /**
     * @brief Destructor — default; shared_ptr cleanup handles the tree nodes.
     */
    ~Tree() = default;

    /**
     * @brief Return the root node.
     * @return Shared pointer to the root node.
     */
    NodePtr get_root() const;

    /**
     * @brief Return whether the tree has no root (is empty).
     * @return True if the root has been cleared.
     */
    bool is_empty() const;

    /**
     * @brief Add a new level of children and update the histograms.
     * @param new_indexes Pairs of (vertex index, parent node) for each new child.
     * @param s_list Graph library used to look up neighbour colors.
     * @return Vector of newly created nodes.
     */
    std::vector<NodePtr>
    add_tree_level(const std::vector<std::pair<uint32_t, NodePtr>>& new_indexes,
                   const std::vector<ColoredGraph>& s_list);

    /**
     * @brief Remove a node and backtrack up the tree while parents become childless.
     * @param node Node to remove.
     * @param s_list Graph library used to look up neighbour colors.
     */
    void remove_node(const NodePtr& node, const std::vector<ColoredGraph>& s_list);

    /**
     * @brief Walk up from a node until the given depth is reached.
     * @param lowest_node_in_match Starting node.
     * @param target_depth Target depth to stop at.
     * @return Node at @p target_depth on the ancestor chain, or nullptr if not found.
     */
    NodePtr get_node_by_depth(const NodePtr& lowest_node_in_match,
                               uint32_t target_depth) const;

private:
    NodePtr m_root;                                       ///< Root node of the tree.
    uint32_t m_depth;                                     ///< Maximum depth reached.
    bool m_is_directed;                                   ///< Whether the graph is directed.
    IndividualColorHist m_hist;                           ///< Forward-edge color histogram.
    boost::optional<IndividualColorHist> m_reverse_hist;  ///< Reverse-edge color histogram (directed only).

    /**
     * @brief Build a map of vertex index → depth for every node on the path to the root.
     * @param last_node_in_path Deepest node of the path to trace.
     * @return Map from vertex index to its depth along the path.
     */
    std::unordered_map<uint32_t, uint32_t>
    get_tree_path_map(const NodePtr& last_node_in_path) const;

    /**
     * @brief Insert a new child node under @p parent.
     * @param parent Parent node to attach to.
     * @param vertex_index Source-graph vertex index for the new node.
     * @return Newly created node.
     */
    NodePtr add_node(const NodePtr& parent, uint32_t vertex_index);

    /**
     * @brief Remove a leaf node from the sibling ring and record it in the parent.
     * @param node Leaf node to remove.
     */
    void delete_node(const NodePtr& node);

    /**
     * @brief Collect depths of tree-path vertices that are neighbours of the given set.
     *
     * For each vertex in @p path_in_tree not yet processed, checks whether it
     * neighbours any vertex in @p candidate_indexes. If so, its depth is appended
     * to @p found_depths and it is marked processed.
     *
     * @param candidate_indexes Vertex indices of the newly added children.
     * @param s_list Graph library.
     * @param path_in_tree Map of vertex index → depth for the current tree path.
     * @param found_depths Output vector to append matched depths to.
     * @param vertex_already_processed Per-vertex flag to avoid duplicate entries.
     * @param is_reversed If true, use incoming edges; otherwise outgoing.
     */
    void update_neighbours_in_tree_path(
        const std::unordered_set<uint32_t>& candidate_indexes,
        const std::vector<ColoredGraph>& s_list,
        const std::unordered_map<uint32_t, uint32_t>& path_in_tree,
        std::vector<uint32_t>& found_depths,
        std::vector<bool>& vertex_already_processed,
        bool is_reversed) const;

    /**
     * @brief Collect colors of neighbours of @p candidate_indexes not in the tree path.
     *
     * For each vertex in @p candidate_indexes, iterates its neighbours. Any neighbour
     * absent from both @p path_in_tree and @p excluded_previous_children contributes
     * its color to the result.
     *
     * @param candidate_indexes Vertex indices to expand from.
     * @param s_list Graph library.
     * @param path_in_tree Map of vertex index → depth for the current tree path.
     * @param excluded_previous_children Previously removed child indices to skip.
     * @param is_reversed If true, use incoming edges; otherwise outgoing.
     * @return Colors of all qualifying neighbours.
     */
    std::vector<uint32_t> get_colors_of_neighbours_not_in_tree_path(
        const std::unordered_set<uint32_t>& candidate_indexes,
        const std::vector<ColoredGraph>& s_list,
        const std::unordered_map<uint32_t, uint32_t>& path_in_tree,
        const std::unordered_set<uint32_t>& excluded_previous_children,
        bool is_reversed) const;
};

}  // namespace sgf
