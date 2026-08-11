#pragma once

#include "ColoredGraph.h"
#include "CountsMap.h"
#include "LoggerHandler.h"
#include "Node.h"

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
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
 */
class Tree
{
public:
    /**
     * @brief Construct a tree rooted at @p root_vertex_index.
     * @param root_vertex_index Source-graph vertex index for the root node.
     * @param graph The source graph; directionality is inferred from it.
     * @param logger Logger instance.
     */
    Tree(uint32_t root_vertex_index, const ColoredGraph& graph, LoggerHandler logger);

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
     * @brief Return the total number of nodes currently in the tree, including the root.
     * @return Current node count.
     */
    uint32_t size() const;

    /**
     * @brief Add a new level of children to the tree.
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
    static NodePtr get_node_by_depth(const NodePtr& lowest_node_in_match, uint32_t target_depth);

    /**
     * @brief Build a map of vertex index → depth for every node on the path to the root.
     * @param last_node_in_path Deepest node of the path to trace.
     * @return Map from vertex index to its depth along the path.
     */
    static std::unordered_map<uint32_t, uint32_t>
    get_tree_path_map(const NodePtr& last_node_in_path);

    /**
     * @brief Count graph neighbours of the current match frontier, keyed by
     *        (neighbour colour, ancestor depth).
     *
     * @p leaves must be the current frontier nodes, sorted so siblings are
     * consecutive (the caller owns the ordering — typically the leaf_matches
     * vector maintained by MultiGraphPatternFinder).
     *
     * For each leaf the running ancestor-path map is advanced incrementally
     * to the new parent by walking both ancestor chains to their common
     * ancestor, removing old nodes and adding new ones.  Every path vertex's
     * external graph neighbours (not already in the path) are then counted
     * by {colour, depth}.
     *
     * @param leaves Current frontier nodes, siblings consecutive.
     * @return Map from {colour, depth} to total unexplored-neighbour count.
     */
    void get_color_by_depth_neighbour_counts(const std::vector<NodePtr>& leaves,
                                             CountsMap& counts) const;

private:
    NodePtr m_root;               ///< Root node of the tree.
    LoggerHandler m_logger;       ///< Logger.
    const ColoredGraph& m_graph;  ///< Source graph for neighbour lookups.
    bool m_is_directed;           ///< Whether the source graph is directed (inferred from m_graph).
    uint32_t m_node_count;        ///< Total number of nodes currently in the tree.

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
    static void splice_out_of_sibling_ring(const NodePtr& node);

    /**
     * @brief Remove a leaf node from the sibling ring and update the parent.
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
    std::vector<NodePtr>
    attach_all_new_nodes(const std::vector<std::pair<uint32_t, NodePtr>>& vertex_parent_pairs);

    /**
     * @brief Accumulate neighbour counts in one edge direction for a single frontier vertex.
     *
     * Every graph neighbour of @p vertex_node reachable via @p is_reversed edges that is NOT
     * already in @p path_set is counted under {colour, depth-1, is_reversed}.
     *
     * @param vertex_node Frontier node whose neighbours are counted.
     * @param path_set    Set of vertex indices to exclude.
     * @param is_reversed False for out-edges, true for in-edges.
     * @param counts      Accumulator updated in-place.
     */
    void accumulate_direction_neighbour_counts(const NodePtr& vertex_node,
                                               const std::unordered_set<uint32_t>& path_set,
                                               bool is_reversed, CountsMap& counts) const;

    /**
     * @brief Accumulate neighbour counts for both directions (undirected: out only).
     *
     * Calls accumulate_direction_neighbour_counts for out-edges and, when the
     * source graph is directed, again for in-edges.
     *
     * @param vertex_node Frontier node whose neighbours are counted.
     * @param path_set    Set of vertex indices to exclude.
     * @param counts      Accumulator updated in-place.
     */
    void accumulate_vertex_neighbour_counts(const NodePtr& vertex_node,
                                            const std::unordered_set<uint32_t>& path_set,
                                            CountsMap& counts) const;

    /**
     * @brief Build the initial per-leaf frontier: one entry per leaf, each with its own
     *        full ancestor path (root excluded).
     * @param leaves         Current match-tree leaves, siblings consecutive.
     * @param frontier_nodes Filled with a copy of @p leaves.
     * @param frontier_paths Filled with each leaf's own ancestor-path set.
     */
    void build_leaf_frontier_paths(const std::vector<NodePtr>& leaves,
                                   std::vector<NodePtr>& frontier_nodes,
                                   std::vector<std::unordered_set<uint32_t>>& frontier_paths) const;

    /**
     * @brief Count every frontier vertex's unreached neighbours into @p counts.
     * @param frontier_nodes Current frontier vertices.
     * @param frontier_paths Matching per-vertex exclusion sets.
     * @param counts         Accumulator updated in-place.
     */
    void accumulate_frontier_neighbour_counts(
        const std::vector<NodePtr>& frontier_nodes,
        const std::vector<std::unordered_set<uint32_t>>& frontier_paths, CountsMap& counts) const;

    /**
     * @brief Climb the frontier one level, merging siblings that share a parent.
     *
     * Consecutive frontier entries with the same parent collapse into one entry
     * whose path set is the intersection of the merged entries' path sets.
     *
     * @param frontier_nodes Updated in-place to the parent frontier.
     * @param frontier_paths Updated in-place to match.
     */
    void advance_frontier_to_parents(std::vector<NodePtr>& frontier_nodes,
                                     std::vector<std::unordered_set<uint32_t>>& frontier_paths) const;

    /**
     * @brief Seed path_set from a leaf's ancestor chain (root excluded).
     * @param leaf      Leaf node to walk up from.
     * @param path_set  Filled with the leaf's ancestor-chain vertex indices.
     */
    static void seed_path_from_leaf(const NodePtr& leaf, std::unordered_set<uint32_t>& path_set);

    /**
     * @brief Advance path_set from @p prev_leaf to @p curr_leaf via their common ancestor.
     * @param prev_leaf  Leaf from the previous iteration.
     * @param curr_leaf  Leaf for the current iteration.
     * @param path_set   Updated in-place.
     */
    static void update_path_to_next_leaf(const NodePtr& prev_leaf, const NodePtr& curr_leaf,
                                         std::unordered_set<uint32_t>& path_set);
};

}  // namespace sgf
