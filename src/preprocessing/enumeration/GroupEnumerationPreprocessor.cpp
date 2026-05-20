#include "GroupEnumerationPreprocessor.h"

#include "ColoredGraph.h"
#include "DebugLog.h"
#include "EnumerationOverflowException.h"
#include "Int128.h"
#include "LogLevel.h"
#include "LoggerHandler.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sgf
{

namespace
{

/// @brief Number of groups between progress log messages.
constexpr uint32_t LOG_INTERVAL = 1000U;

using NeighbourIteratorPair =
    std::pair<std::vector<uint32_t>::const_iterator, std::vector<uint32_t>::const_iterator>;

}  // namespace

GroupEnumerationPreprocessor::GroupEnumerationPreprocessor(const ColoredGraph& graph,
                                                           LoggerHandler logger)
    : m_graph(graph)
    , m_logger(std::move(logger))
{
}

size_t GroupEnumerationPreprocessor::combined_degree(const uint32_t vertex) const
{
    const NeighbourIteratorPair out_range = m_graph.get_neighbours(vertex);
    const size_t out_degree = static_cast<size_t>(out_range.second - out_range.first);
    if (!m_graph.is_directed())
    {
        return out_degree;
    }
    const NeighbourIteratorPair in_range = m_graph.get_neighbours(vertex, true);
    return out_degree + static_cast<size_t>(in_range.second - in_range.first);
}

void GroupEnumerationPreprocessor::sort_nodes()
{
    const uint32_t vertex_count = m_graph.vertex_count();
    m_node_order.resize(vertex_count);
    std::iota(m_node_order.begin(), m_node_order.end(), 0U);
    std::sort(m_node_order.begin(), m_node_order.end(),
              [this](const uint32_t left_vertex, const uint32_t right_vertex)
              {
                  return combined_degree(left_vertex) > combined_degree(right_vertex);
              });
}

std::unordered_map<UInt128, uint32_t, UInt128Hash> GroupEnumerationPreprocessor::calculate()
{
    m_logger.log(LogLevel::INFO, "Starting graph enumeration calculation.");
    std::unordered_map<UInt128, uint32_t, UInt128Hash> motif_count;

    std::vector<std::vector<bool>> graph_adjacency_matrix;
    graph_to_adjacency_matrix(graph_adjacency_matrix);
    sort_nodes();

    uint32_t groups_counted = 0;
    const GroupCounterCallback count_group =
        [&motif_count, &groups_counted, this](const uint32_t group_structure_descriptor,
                                              const std::vector<uint32_t>& group_vertex_ids)
    {
        const std::vector<uint32_t> node_colors = group_to_node_colors(group_vertex_ids);
        uint32_t& count =
            motif_count[calculate_motif_number(group_structure_descriptor, node_colors)];
        if (count == std::numeric_limits<uint32_t>::max())
        {
            throw EnumerationOverflowException(
                "Motif count overflow: occurrence count exceeded uint32_t capacity.");
        }
        count += 1U;

        if (groups_counted % LOG_INTERVAL == 0)
        {
            SGF_DEBUG_LOG(m_logger, "Counted groups: " + std::to_string(groups_counted));
        }
        groups_counted++;
    };

    stream_groups_to_counter(graph_adjacency_matrix, count_group);
    m_logger.log(LogLevel::INFO, "Finished graph enumeration calculation.");
    return motif_count;
}

void GroupEnumerationPreprocessor::graph_to_adjacency_matrix(
    std::vector<std::vector<bool>>& adjacency_matrix) const
{
    const uint32_t vertex_count = m_graph.vertex_count();

    adjacency_matrix.resize(vertex_count);
    for (uint32_t vertex_index = 0; vertex_index < vertex_count; ++vertex_index)
    {
        adjacency_matrix[vertex_index].resize(vertex_count, false);
    }

    for (uint32_t node = 0; node < vertex_count; ++node)
    {
        const NeighbourIteratorPair neighbours = m_graph.get_neighbours(node);
        for (std::vector<uint32_t>::const_iterator neighbour_it = neighbours.first;
             neighbour_it != neighbours.second; ++neighbour_it)
        {
            adjacency_matrix[node][*neighbour_it] = true;
        }
    }
}

std::vector<uint32_t>
GroupEnumerationPreprocessor::group_to_node_colors(const std::vector<uint32_t>& group) const
{
    std::vector<uint32_t> node_colors;
    node_colors.reserve(group.size());
    for (const auto& vertex : group)
    {
        node_colors.push_back(m_graph.get_vertex_color(vertex));
    }
    return node_colors;
}

}  // namespace sgf
