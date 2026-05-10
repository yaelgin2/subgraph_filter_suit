#pragma once
#include <memory>
#include <unordered_set>
#include <cstdint>

struct Node {
    std::shared_ptr<Node> left;     // sibling
    std::shared_ptr<Node> right;    // sibling
    std::shared_ptr<Node> son;      // first child
    std::weak_ptr<Node> parent;     // back-reference (NO ownership)

    std::unordered_set<uint32_t> previous_children;

    uint32_t index;
    int32_t  depth;
    uint32_t match_edge_count = 0; ///< Accumulated matching-edge count along path from root.

    Node(int32_t idx, int32_t d = 0)
        : index(idx), depth(d) {}
};

using NodePtr = std::shared_ptr<Node>;