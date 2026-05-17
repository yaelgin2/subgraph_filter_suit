#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>

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
    /**
     * @brief Construct a tree node.
     * @param vertex_index Index of the source-graph vertex this node represents.
     * @param tree_depth Depth of this node within the tree (root is 0).
     */
    Node(const uint32_t vertex_index, const uint32_t tree_depth = 0U)
        : m_index(vertex_index), m_depth(tree_depth)
    {
    }

    std::shared_ptr<Node> m_left;   ///< Left sibling in the child ring.
    std::shared_ptr<Node> m_right;  ///< Right sibling in the child ring.
    std::shared_ptr<Node> m_son;    ///< First child of this node.
    std::weak_ptr<Node> m_parent;   ///< Non-owning back-reference to parent.

    std::unordered_map<uint32_t, uint32_t> m_previous_children;  ///< Indices of previously-removed children.

    uint32_t m_index;                ///< Source-graph vertex index.
    uint32_t m_depth;                ///< Depth within the tree (root = 0).
    uint32_t m_match_edge_count = 0U;  ///< Accumulated matching-edge count along path from root.
};

using NodePtr = std::shared_ptr<Node>;

}  // namespace sgf
