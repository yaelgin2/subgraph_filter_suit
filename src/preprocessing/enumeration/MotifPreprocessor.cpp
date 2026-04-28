#include "MotifPreprocessor.h"

#include "ColoredGraph.h"
#include "GroupEnumerationPreprocessor.h"
#include "LoggerHandler.h"

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <vector>

namespace sgf
{

MotifPreprocessor::MotifPreprocessor(const ColoredGraph& graph, LoggerHandler logger)
    : GroupEnmerationPreprocessor(graph, std::move(logger))
    , m_graph(graph)
{
}

void MotifPreprocessor::sort_nodes()
{
    const uint32_t vertex_count = m_graph.vertex_count();

    m_node_order.resize(vertex_count);
    std::iota(m_node_order.begin(), m_node_order.end(), 0U);

    std::sort(m_node_order.begin(), m_node_order.end(),
              [this](const uint32_t left_vertex, const uint32_t right_vertex)
              {
                  const std::pair<std::vector<uint32_t>::const_iterator,
                                  std::vector<uint32_t>::const_iterator>
                      left_neighbours = m_graph.get_neighbours(left_vertex);
                  const std::pair<std::vector<uint32_t>::const_iterator,
                                  std::vector<uint32_t>::const_iterator>
                      right_neighbours = m_graph.get_neighbours(right_vertex);
                  return std::distance(left_neighbours.first, left_neighbours.second)
                         > std::distance(right_neighbours.first, right_neighbours.second);
              });
}

void MotifPreprocessor::stream_groups_to_counter(
    const std::vector<std::vector<bool>>& graph_adjacency_matrix,
    const GroupCounterCallback& count_group)
{
    // TODO: enumerate all combinations of 4 vertices from m_node_order
    // TODO: for each combination, check that the induced subgraph is connected
    // TODO: compute the raw edge-structure descriptor via compute_motif_descriptor()
    // TODO: invoke count_group(descriptor, {v0, v1, v2, v3}) for each valid group
}

__uint128_t MotifPreprocessor::calculate_motif_number(const uint32_t motif_descriptor,
                                                       const std::vector<uint32_t>& node_colors)
{
    // TODO: select DIRECTED_MOTIF_CANONICAL_MAP or UNDIRECTED_MOTIF_CANONICAL_MAP
    //       based on m_graph.is_directed()
    // TODO: look up motif_descriptor in the selected map
    // TODO: if not found, encode motif_descriptor + node_colors directly (uncolored fallback)
    // TODO: retrieve the MotifCanonical entry (minimal_motif_num + color_permutations)
    // TODO: apply each permutation to node_colors to get the canonical color ordering
    // TODO: call encode_motif_key(minimal_motif_num, permuted_colors) and return the result
    return 0;
}

uint32_t MotifPreprocessor::compute_motif_descriptor(
    const std::vector<uint32_t>& group,
    const std::vector<std::vector<bool>>& graph_adjacency_matrix) const
{
    // TODO: iterate over all ordered pairs (i, j) of the 4 group vertices
    // TODO: for each pair, set a bit in the descriptor if graph_adjacency_matrix[group[i]][group[j]] is true
    // TODO: use a consistent bit-position scheme (e.g. bit = i * 4 + j, skipping i==j)
    // TODO: return the resulting bitmask
    return 0U;
}

__uint128_t MotifPreprocessor::encode_motif_key(const uint32_t canonical_motif_num,
                                                 const std::vector<uint32_t>& permuted_colors)
{
    // TODO: pack canonical_motif_num into the high bits of a __uint128_t
    // TODO: pack each element of permuted_colors into successive bit fields in the low bits
    // TODO: choose field widths wide enough to hold the maximum color value in use
    // TODO: return the combined 128-bit key
    return 0;
}

}  // namespace sgf
