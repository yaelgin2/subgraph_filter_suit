#pragma once

#include "ColoredGraph.h"

#include <cstdint>
#include <utility>
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

/**
 * @brief Build a CpuNeighbourRange covering forward and reverse neighbours of @p vertex.
 *
 * For undirected graphs, the reverse range is set to an empty range at the end
 * of the forward range, matching ColoredGraph::get_neighbours' own fallback
 * behaviour for undirected reverse queries.
 *
 * @param graph  Graph to query.
 * @param vertex Vertex id to look up.
 */
inline CpuNeighbourRange get_neighbour_range(const ColoredGraph& graph, const uint32_t vertex)
{
    using IterPair =
        std::pair<std::vector<uint32_t>::const_iterator, std::vector<uint32_t>::const_iterator>;
    const IterPair fwd = graph.get_neighbours(vertex);
    const IterPair rev = graph.is_directed() ? graph.get_neighbours(vertex, true)
                                             : std::make_pair(fwd.second, fwd.second);
    return CpuNeighbourRange{fwd.first, fwd.second, rev.first, rev.second};
}

}  // namespace sgf
