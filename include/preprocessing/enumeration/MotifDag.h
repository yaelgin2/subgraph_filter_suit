#pragma once

#include "Constants.h"

#include <array>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sgf
{

/**
 * @brief One color permutation of size MOTIF_SIZE applied along a DAG edge.
 */
using DagPermutation = std::array<uint32_t, SgfConstants::MOTIF_SIZE>;

/**
 * @brief All out-edges from a single motif structure node.
 *
 * Each entry is a destination motif structure number paired with the list of
 * color permutations that map the source to the destination.
 */
using DagEdgeList = std::vector<std::pair<uint32_t, std::vector<DagPermutation>>>;

/**
 * @brief Adjacency map for a motif inclusion DAG.
 *
 * Maps each canonical motif structure number to its list of out-edges.
 * Used by MotifDagExpander to propagate enumeration counts from induced to
 * non-induced motif representations.
 */
using DagAdjacency = std::unordered_map<uint32_t, DagEdgeList>;

/**
 * @brief Motif inclusion DAG for 4-node undirected colored motifs.
 */
extern const DagAdjacency UNDIRECTED_MOTIF_DAG;

/**
 * @brief Motif inclusion DAG for 4-node directed colored motifs.
 */
extern const DagAdjacency DIRECTED_MOTIF_DAG;

}  // namespace sgf
