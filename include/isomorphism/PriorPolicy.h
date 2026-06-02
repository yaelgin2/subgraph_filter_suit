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
    SUBGRAPH_DEGREE_SQUARED,
    /** Score = negated sum of candidate degrees in the host graph. */
    GRAPH_DEGREE_SQUARED,
    /** Score = negated restriction-set size (choose most constrained). */
    CONSTANT,
    /** Score = uniform random value. */
    RANDOM,
    /** Score = degree in the subgraph. */
    SUBGRAPH_DEGREE,
    /** Runs SUBGRAPH_DEGREE_SQUARED and SUBGRAPH_DEGREE in parallel. */
    COMBINED,
};

}  // namespace sgf
