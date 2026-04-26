#include "GroupEnumerationPreprocessor.h"

#include "ILogger.h"
#include "LogLevel.h"

namespace sgf
{

GroupEnmerationPreprocessor::GroupEnmerationPreprocessor(std::shared_ptr<ColoredGraph> graph,
     LoggerHandler logger)
    : m_graph(graph), m_logger(logger)
{
}

std::unordered_map<__uint128_t, uint32_t> GroupEnmerationPreprocessor::calculate()
{
    m_logger.log(LogLevel::INFO, "Starting graph enumeration calculation.");
    std::unordered_map<__uint128_t, uint32_t> motif_count;
    std::vector<std::vector<bool>> adjacency_matrix;
    graph_to_adjacency_matrix(adjacency_matrix);
    sort_nodes();
    Groups groups = find_groups();
    uint32_t groups_iterated_overs = 0;
    for (const auto& [motif_num, group] : groups)
    {
        vecotr<std::vector<bool>> group_adjacency_matrix = group_to_adjacency_matrix(group);
        motif_count[calculate_motif_number(motif_num, group_adjacency_matrix)] += 1;
        if (groups_iterated_overs % 1000 == 0 && VERBOSE)
        {
            m_logger.log(LogLevel::DEBUG, "Found groups: " + std::to_string(groups_iterated_overs));
        }
        groups_iterated_overs++;
    }
    m_logger.log(LogLevel::INFO, "Finished graph enumeration calculation.");
    return motif_count;
}

void GroupEnmerationPreprocessor::graph_to_adjacency_matrix(std::vector<std::vector<bool>>& adjacency_matrix)
{
    adjacency_matrix.resize(m_graph->num_vertices());
    for (size_t i = 0; i < m_graph->num_vertices(); i++)
    {
        adjacency_matrix[i].resize(m_graph->num_vertices(), false);
    }
    for (int node = 0; node < m_graph->num_vertices(); node++)
    {
        auto [neighbour_begin, neighbour_end] = m_graph->get_neighbours(node);
        for (auto neighbour_iterator = neighbour_begin; neighbour_iterator != neighbour_end; neighbour_iterator++)
        {
            adjacency_matrix[node][*neighbour_iterator] = true;
        }
    }
}

std::vector<std::vector<bool>>> group_to_adjacency_matrix(const std::vector<uint32_t>& group)
{
    std::vector<std::vector<bool>> group_adjacency_matrix;
    group_adjacency_matrix.resize(group.size());
    for (size_t vertex_index = 0; vertex_index < group.size(); vertex_index++)
    {
        group_adjacency_matrix[vertex_index].resize(group.size(), false);
    }
    for (size_t vertex_index = 0; vertex_index < group.size(); vertex_index++)
    {
        for(size_t neighbour_index = 0; neighbour_index < group.size(); neighbour_index++)
        {
            if (m_graph->is_edge(group[vertex_index], group[neighbour_index]))
            {
                group_adjacency_matrix[vertex_index][neighbour_index] = true;
            }
        }
    }
    return group_adjacency_matrix;
}

}  // namespace sgf
