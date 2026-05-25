#pragma once

#include <cstdint>

namespace sgf
{

/**
 * @brief Vertex ordering heuristic used during subgraph search.
 *
 * Controls how the search picks the next subgraph vertex to match and which
 * graph vertex to try first. Each policy assigns a score to candidates; the
 * vertex with the highest score is chosen next.
 */
enum class PriorPolicy : uint32_t
{
    /** Score = sum of neighbour degrees in the subgraph (second-order degree). */
    SUBGRAPH_DEGREE_SQUARED = 0U,
    /** Score = negated sum of candidate degrees in the host graph. */
    GRAPH_DEGREE_SQUARED = 1U,
    /** Score = negated restriction-set size (choose most constrained). */
    CONSTANT = 2U,
    /** Score = uniform random value. */
    RANDOM = 3U,
    /** Score = degree in the subgraph. */
    SUBGRAPH_DEGREE = 4U,
    /** Runs SUBGRAPH_DEGREE_SQUARED and SUBGRAPH_DEGREE in parallel. */
    COMBINED = 5U,
};

}  // namespace sgf
