#pragma once

#include "TreeStats.h"

#include <cstdint>
#include <memory>
#include <utility>

namespace sgf
{

/**
 * @struct Node
 * @brief Node of a pattern-expansion tree using first-child / left-right-sibling layout.
 *
 * Children of a parent form a doubly-linked circular sibling ring reachable from
 * `m_son`. The `m_parent` back-reference is a `weak_ptr` to break ownership cycles.
 */
struct Node
{
    uint32_t m_index;                       ///< Source-graph vertex index.
    uint32_t m_depth;                       ///< Depth within the tree (root = 0).
    std::weak_ptr<TreeStats> m_tree_stats;  ///< Non-owning handle to the owning Tree's counters.

    std::shared_ptr<Node> m_left;   ///< Left sibling in the child ring.
    std::shared_ptr<Node> m_right;  ///< Right sibling in the child ring.
    std::shared_ptr<Node> m_son;    ///< First child of this node.
    std::weak_ptr<Node> m_parent;   ///< Non-owning back-reference to parent.

    /**
     * @brief Construct a tree node and record it as live in @p tree_stats.
     * @param vertex_index Index of the source-graph vertex this node represents.
     * @param tree_depth Depth of this node within the tree (root is 0).
     * @param tree_stats Non-owning handle to the owning Tree's live-memory counters.
     */
    explicit Node(const uint32_t vertex_index, const uint32_t tree_depth,
                  std::weak_ptr<TreeStats> tree_stats)
        : m_index(vertex_index)
        , m_depth(tree_depth)
        , m_tree_stats(std::move(tree_stats))
    {
        if (const std::shared_ptr<TreeStats> stats = m_tree_stats.lock())
        {
            ++stats->m_node_count;
            stats->m_total_bytes += sizeof(Node);
        }
    }

    /**
     * @brief Destructor — decrements the owning Tree's live-node counters.
     *
     * This runs exactly when the node's memory is actually reclaimed, so it reflects
     * a real free rather than a logical detach from the tree.
     */
    ~Node()
    {
        if (const std::shared_ptr<TreeStats> stats = m_tree_stats.lock())
        {
            --stats->m_node_count;
            stats->m_total_bytes -= sizeof(Node);
        }
    }

    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;
    Node(Node&&) = delete;
    Node& operator=(Node&&) = delete;
};

using NodePtr = std::shared_ptr<Node>;

}  // namespace sgf
