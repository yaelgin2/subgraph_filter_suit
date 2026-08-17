#pragma once

#include <cstdint>

namespace sgf
{

/**
 * @struct TreeStats
 * @brief Live memory counters shared between a Tree and its Node instances.
 *
 * Owned by the Tree via a shared_ptr; each Node holds a non-owning weak_ptr back to
 * this struct so that ~Node can decrement counts exactly when a node's memory is
 * actually freed, not merely when it is unlinked from the tree.
 */
struct TreeStats
{
    uint64_t m_node_count = 0U;   ///< Number of currently live nodes.
    uint64_t m_total_bytes = 0U;  ///< Total bytes occupied by currently live nodes.
};

}  // namespace sgf
