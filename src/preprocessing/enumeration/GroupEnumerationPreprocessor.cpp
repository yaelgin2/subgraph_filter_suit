#include "GroupEnumerationPreprocessor.h"

#include "ColoredGraph.h"
#include "LogLevel.h"
#include "LoggerHandler.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <utility>

namespace sgf
{

namespace
{

/// @brief Number of groups between progress log messages.
constexpr uint32_t LOG_INTERVAL = 1000;

}  // namespace

GroupEnmerationPreprocessor::GroupEnmerationPreprocessor(const ColoredGraph& graph,
                                                         LoggerHandler logger)
    : m_graph(graph)
    , m_logger(std::move(logger))
{
}

std::unordered_map<__uint128_t, uint32_t> GroupEnmerationPreprocessor::calculate()
{
    m_logger.log(LogLevel::INFO, "Starting graph enumeration calculation.");
    std::unordered_map<__uint128_t, uint32_t> motif_count;

    // Build full adjacency matrix and establish node traversal order.
    std::vector<std::vector<bool>> graph_adjacency_matrix;
    graph_to_adjacency_matrix(graph_adjacency_matrix);
    sort_nodes();

    // Count each group immediately as it is discovered — no intermediate storage.
    uint32_t groups_counted = 0;
    const GroupCounterCallback count_group =
        [&motif_count, &groups_counted, this](const uint32_t group_structure_descriptor,
                                              const std::vector<uint32_t>& group_vertex_ids)
    {
        const std::vector<uint32_t> node_colors = group_to_node_colors(group_vertex_ids);
        motif_count[calculate_motif_number(group_structure_descriptor, node_colors)] += 1;

        if (groups_counted % LOG_INTERVAL == 0)
        {
            m_logger.log(LogLevel::DEBUG, "Counted groups: " + std::to_string(groups_counted));
        }
        groups_counted++;
    };

    stream_groups_to_counter(graph_adjacency_matrix, count_group);
    m_logger.log(LogLevel::INFO, "Finished graph enumeration calculation.");
    return motif_count;
}

void GroupEnmerationPreprocessor::graph_to_adjacency_matrix(
    std::vector<std::vector<bool>>& adjacency_matrix)
{
    const uint32_t vertex_count = m_graph.vertex_count();

    // Allocate an N×N matrix, initialised to false (no edges).
    adjacency_matrix.resize(vertex_count);
    for (size_t vertex_index = 0; vertex_index < vertex_count; vertex_index++)
    {
        adjacency_matrix[vertex_index].resize(vertex_count, false);
    }

    // Mark every directed edge present in the graph.
    for (uint32_t node = 0; node < vertex_count; node++)
    {
        const auto [neighbour_begin, neighbour_end] = m_graph.get_neighbours(node);
        for (auto neighbour_iterator = neighbour_begin; neighbour_iterator != neighbour_end;
             neighbour_iterator++)
        {
            adjacency_matrix[node][*neighbour_iterator] = true;
        }
    }
}

std::vector<uint32_t>
GroupEnmerationPreprocessor::group_to_node_colors(const std::vector<uint32_t>& group)
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
