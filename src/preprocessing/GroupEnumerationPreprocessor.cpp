#include "GroupEnumerationPreprocessor.h"

#include "ColoredGraph.h"
#include "LogLevel.h"
#include "LoggerHandler.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

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
    std::vector<std::vector<bool>> adjacency_matrix;
    graph_to_adjacency_matrix(adjacency_matrix);
    sort_nodes();
    const Groups groups = find_groups();
    uint32_t groups_iterated_over = 0;
    for (const auto& [motif_num, group] : groups)
    {
        const std::vector<std::vector<bool>> group_adjacency_matrix =
            group_to_adjacency_matrix(group);
        motif_count[calculate_motif_number(motif_num, group_adjacency_matrix)] += 1;
        if (groups_iterated_over % LOG_INTERVAL == 0)
        {
            m_logger.log(LogLevel::DEBUG, "Found groups: " + std::to_string(groups_iterated_over));
        }
        groups_iterated_over++;
    }
    m_logger.log(LogLevel::INFO, "Finished graph enumeration calculation.");
    return motif_count;
}

void GroupEnmerationPreprocessor::graph_to_adjacency_matrix(
    std::vector<std::vector<bool>>& adjacency_matrix)
{
    const uint32_t vertex_count = m_graph.vertex_count();
    adjacency_matrix.resize(vertex_count);
    for (size_t vertex_index = 0; vertex_index < vertex_count; vertex_index++)
    {
        adjacency_matrix[vertex_index].resize(vertex_count, false);
    }
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

std::vector<std::vector<bool>>
GroupEnmerationPreprocessor::group_to_adjacency_matrix(const std::vector<uint32_t>& group)
{
    std::vector<std::vector<bool>> group_adjacency_matrix;
    const size_t group_size = group.size();
    group_adjacency_matrix.resize(group_size);
    for (size_t vertex_index = 0; vertex_index < group_size; vertex_index++)
    {
        group_adjacency_matrix[vertex_index].resize(group_size, false);
    }
    for (size_t vertex_index = 0; vertex_index < group_size; vertex_index++)
    {
        for (size_t neighbour_index = 0; neighbour_index < group_size; neighbour_index++)
        {
            if (m_graph.is_edge(group[vertex_index], group[neighbour_index]))
            {
                group_adjacency_matrix[vertex_index][neighbour_index] = true;
            }
        }
    }
    return group_adjacency_matrix;
}

}  // namespace sgf
