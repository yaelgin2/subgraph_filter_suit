#pragma once

#include "ColoredGraph.h"
#include "GeneralColorHist.h"
#include "IndividualColorHist.h"
#include "Node.h"
#include "LoggerHandler.h"

#include <boost/optional.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
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
     * @brief Construct a tree rooted at @p root_vertex_index.
     * @param root_vertex_index Source-graph vertex index for the root node.
     * @param graph The source graph; directionality is inferred from it.
     * @param logger Logger instance.
     * @param general_hist Shared general histogram for forward edges.
     * @param reverse_general_hist Shared general histogram for reverse edges (required when directed).
     */
    Tree(uint32_t root_vertex_index,
         const ColoredGraph& graph,
         const LoggerHandler& logger,
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
     * @param vertex_parent_pairs Pairs of (vertex index, parent node) for each new child.
     * @return Vector of newly created nodes.
     */
    std::vector<NodePtr>
    add_tree_level(const std::vector<std::pair<uint32_t, NodePtr>>& vertex_parent_pairs);

    /**
     * @brief Remove a node and backtrack up the tree while parents become childless.
     * @param node Node to remove.
     */
    void remove_node(const NodePtr& node);

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
    LoggerHandler m_logger;                               ///< Logger.
    const ColoredGraph& m_graph;                          ///< Source graph for neighbour lookups.
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
     * @brief Splice @p node out of its circular sibling ring, updating neighbour pointers.
     * @param node The node to remove from the ring.
     */
    void splice_out_of_sibling_ring(const NodePtr& node) const;

    /**
     * @brief Remove a leaf node from the sibling ring and record it in the parent.
     * @param node Leaf node to remove.
     */
    void delete_node(const NodePtr& node);

    /**
     * @brief Validate that @p vertex_parent_pairs are grouped by parent with no repeats.
     * @param vertex_parent_pairs Ordered list of (vertex, parent) pairs to validate.
     * @throws AddNodeException if any parent reappears after being closed.
     */
    void validate_parent_ordering(
        const std::vector<std::pair<uint32_t, NodePtr>>& vertex_parent_pairs) const;

    /**
     * @brief Attach all new nodes to the tree and return them.
     * @param vertex_parent_pairs Ordered list of (vertex, parent) pairs to attach.
     * @return Vector of newly created nodes in the same order.
     */
    std::vector<NodePtr> attach_all_new_nodes(
        const std::vector<std::pair<uint32_t, NodePtr>>& vertex_parent_pairs);

    /**
     * @brief Collect vertex indexes sharing the same parent and advance @p current_pair_index.
     * @param vertex_parent_pairs Full ordered list of (vertex, parent) pairs.
     * @param current_pair_index Index into @p vertex_parent_pairs; advanced past the collected group.
     * @param current_parent_node Set to the shared parent of the collected group.
     * @return Set of vertex indexes belonging to the current sibling group.
     */
    std::unordered_set<uint32_t> collect_sibling_vertex_indexes(
        const std::vector<std::pair<uint32_t, NodePtr>>& vertex_parent_pairs,
        size_t& current_pair_index,
        NodePtr& current_parent_node) const;

    /**
     * @brief Update the tree-path map when moving from @p previous_parent to @p current_parent.
     *
     * Walks the two ancestor chains simultaneously until they converge. Nodes exclusive
     * to @p current_parent's chain are added; nodes exclusive to @p previous_parent's chain
     * are removed. Processed flags for changed vertices are reset.
     *
     * @param previous_parent_node The parent processed in the previous sibling group.
     * @param current_parent_node  The parent for the current sibling group.
     * @param tree_path_depths     Map of vertex index → depth; updated in place.
     * @param forward_processed_flags Per-vertex forward-processing flags; reset for changed nodes.
     * @param reverse_processed_flags Per-vertex reverse-processing flags; reset for changed nodes.
     */
    void advance_tree_path_to_parent(
        const NodePtr& previous_parent_node,
        const NodePtr& current_parent_node,
        std::unordered_map<uint32_t, uint32_t>& tree_path_depths,
        std::vector<bool>& forward_processed_flags,
        std::vector<bool>& reverse_processed_flags) const;

    /**
     * @brief Run the per-sibling-group histogram loop for an entire new level.
     *
     * For each sibling group in @p vertex_parent_pairs: updates the tree-path map,
     * records depths whose neighbour counts must be decremented, and adds external
     * neighbour colors to the histograms.
     *
     * @param vertex_parent_pairs   Ordered list of (vertex, parent) pairs for the new level.
     * @param forward_decrease_depths Output: depths to decrement in the forward histogram.
     * @param reverse_decrease_depths Output: depths to decrement in the reverse histogram.
     */
    void process_new_level_histogram(
        const std::vector<std::pair<uint32_t, NodePtr>>& vertex_parent_pairs,
        std::vector<uint32_t>& forward_decrease_depths,
        std::vector<uint32_t>& reverse_decrease_depths);

    /**
     * @brief Collect depths of tree-path vertices that are neighbours of @p candidate_indexes.
     *
     * For each unprocessed vertex in @p tree_path_depths, checks whether it neighbours
     * any vertex in @p candidate_indexes. If so, its depth is appended to @p found_depths
     * and it is marked processed.
     *
     * @param candidate_indexes Vertex indices of the newly added children.
     * @param tree_path_depths Map of vertex index → depth for the current tree path.
     * @param found_depths Output vector to append matched depths to.
     * @param vertex_processed_flags Per-vertex flag to avoid duplicate entries.
     * @param is_reversed If true, use incoming edges; otherwise outgoing.
     */
    void update_neighbours_in_tree_path(
        const std::unordered_set<uint32_t>& candidate_indexes,
        const std::unordered_map<uint32_t, uint32_t>& tree_path_depths,
        std::vector<uint32_t>& found_depths,
        std::vector<bool>& vertex_processed_flags,
        bool is_reversed) const;

    /**
     * @brief Collect colors of all external neighbours of a single candidate vertex.
     *
     * Neighbours in @p tree_path_depths are skipped. Neighbours in
     * @p excluded_previous_children contribute their color once per unique (depth, color) pair.
     * All remaining neighbours contribute their color directly.
     *
     * @param candidate_vertex The vertex whose external neighbours are collected.
     * @param tree_path_depths Map of vertex index → depth for the current tree path.
     * @param excluded_previous_children Previously removed child indices to skip (vertex → depth).
     * @param is_reversed If true, use incoming edges; otherwise outgoing.
     * @return Colors of all qualifying neighbours.
     */
    std::vector<uint32_t> collect_vertex_external_colors(
        uint32_t candidate_vertex,
        const std::unordered_map<uint32_t, uint32_t>& tree_path_depths,
        const std::unordered_map<uint32_t, uint32_t>& excluded_previous_children,
        bool is_reversed) const;

    /**
     * @brief Collect colors of neighbours of @p candidate_indexes not in the tree path.
     *
     * Delegates per-vertex collection to collect_vertex_external_colors and concatenates results.
     *
     * @param candidate_indexes Vertex indices to expand from.
     * @param tree_path_depths Map of vertex index → depth for the current tree path.
     * @param excluded_previous_children Previously removed child indices to skip.
     * @param is_reversed If true, use incoming edges; otherwise outgoing.
     * @return Colors of all qualifying neighbours.
     */
    std::vector<uint32_t> get_colors_of_neighbours_not_in_tree_path(
        const std::unordered_set<uint32_t>& candidate_indexes,
        const std::unordered_map<uint32_t, uint32_t>& tree_path_depths,
        const std::unordered_map<uint32_t, uint32_t>& excluded_previous_children,
        bool is_reversed) const;
};

}  // namespace sgf
