#pragma once

#include <cstdint>
#include <vector>

namespace sgf
{

/**
 * @brief A half-open iterator range over a vertex's sorted neighbour list.
 *
 * For directed graphs, m_rev_begin / m_rev_end carry incoming neighbours.
 * Set rev_begin == rev_end for undirected graphs.
 */
struct CpuNeighbourRange
{
    std::vector<uint32_t>::const_iterator m_begin;  ///< First outgoing neighbour.
    std::vector<uint32_t>::const_iterator m_end;    ///< One past last outgoing neighbour.
    std::vector<uint32_t>::const_iterator
        m_rev_begin;                                  ///< First incoming neighbour (directed only).
    std::vector<uint32_t>::const_iterator m_rev_end;  ///< One past last incoming neighbour.
};

}  // namespace sgf
