#include "MotifPreprocessor.h"

#include "ColoredGraph.h"
#include "Constants.h"
#include "GroupEnumerationPreprocessor.h"
#include "LoggerHandler.h"
#include "MotifMap.h"

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
                  return std::distance(left_neighbours.first, left_neighbours.second) >
                         std::distance(right_neighbours.first, right_neighbours.second);
              });
}

void MotifPreprocessor::stream_groups_to_counter(
    const std::vector<std::vector<bool>>& graph_adjacency_matrix,
    const GroupCounterCallback& count_group)
{
    std::vector<uint64_t> bfs_visited(m_graph.vertex_count(), 0);
    std::vector<bool> processed_vertices(m_graph.vertex_count(), false);
    for (const uint32_t vertex : m_node_order)
    {
        stream_groups_to_counter_for_vertex(graph_adjacency_matrix, count_group, processed_vertices,
                                            bfs_visited, vertex);
        processed_vertices[vertex] = true;
    }
}

void MotifPreprocessor::stream_groups_to_counter_for_vertex(
    const std::vector<std::vector<bool>>& graph_adjacency_matrix,
    const GroupCounterCallback& count_group, const std::vector<bool>& visited_vertices_to_ignore,
    std::vector<uint64_t>& bfs_visited_vertices, uint32_t root)
{
    uint64_t run_id = static_cast<uint64_t>(root) << 2;
    // implementing the "Kavosh" algorithm for subgroups of length 4
    bfs_visited_vertices[root] = run_id + 0;  // mark root as visited with depth 0

    // variation - three neighbors of the root
    auto [first_degree_neighbours_begin, first_degree_neighbours_end] =
        m_graph.get_neighbours(root);
    for (auto first_degree_neigbour = first_degree_neighbours_begin;
         first_degree_neigbour != first_degree_neighbours_end; ++first_degree_neigbour)
    {
        bfs_visited_vertices[*first_degree_neigbour] = run_id + 1;  // mark as visited with depth 1
    }
    for (auto first_neighbour = first_degree_neighbours_begin;
         first_neighbour != first_degree_neighbours_end; ++first_neighbour)
    {
        if (visited_vertices_to_ignore[*(first_neighbour)])
        {
            continue;
        }
        for (auto second_neighbour = first_neighbour + 1;
             second_neighbour != first_degree_neighbours_end; ++second_neighbour)
        {
            if (visited_vertices_to_ignore[*(second_neighbour)])
            {
                continue;
            }
            for (auto third_neighbour = second_neighbour + 1;
                 third_neighbour != first_degree_neighbours_end; ++third_neighbour)
            {
                if (visited_vertices_to_ignore[*(third_neighbour)])
                {
                    continue;
                }
                std::vector<uint32_t> group = {root, *(first_neighbour), *(second_neighbour),
                                               *(third_neighbour)};
                count_group(compute_motif_descriptor(group, graph_adjacency_matrix), group);
            }
        }
    }

    // variations - depths 1, 1, 2 and 1, 2, 2
    // mark visited depth 2

    for (auto first_neighbour = first_degree_neighbours_begin;
         first_neighbour != first_degree_neighbours_end; ++first_neighbour)
    {
        if (visited_vertices_to_ignore[*(first_neighbour)])
        {
            continue;
        }
        auto [second_degree_neighbours_begin, second_degree_neighbours_end] =
            m_graph.get_neighbours(*first_neighbour);
        for (auto second_degree_neighbour = second_degree_neighbours_begin;
             second_degree_neighbour != second_degree_neighbours_end; ++second_degree_neighbour)
        {
            if ((bfs_visited_vertices[*second_degree_neighbour] >> 2) != root)
            {
                bfs_visited_vertices[*second_degree_neighbour] =
                    run_id + 2;  // mark as visited with depth 2
            }
        }
        // variation - depths 1, 1, 2
        for (auto second_degree_neighbour = second_degree_neighbours_begin;
             second_degree_neighbour != second_degree_neighbours_end; ++second_degree_neighbour)
        {
            if (visited_vertices_to_ignore[*(second_degree_neighbour)] ||
                bfs_visited_vertices[*(second_degree_neighbour)] != (run_id + 2))
            {
                continue;
            }
            for (auto second_first_degree_negihbour = first_degree_neighbours_begin;
                 second_first_degree_negihbour != first_degree_neighbours_end;
                 ++second_first_degree_negihbour)
            {
                if (visited_vertices_to_ignore[*(second_first_degree_negihbour)] ||
                    *first_neighbour == *second_first_degree_negihbour)
                {
                    continue;
                }
                bool edge_exists = graph_adjacency_matrix[*(second_first_degree_negihbour)]
                                                         [*(second_degree_neighbour)] ||
                                   graph_adjacency_matrix[*(second_degree_neighbour)]
                                                         [*(second_first_degree_negihbour)];
                // avoid double-counting due to two paths from root to n2 - from n1 and from n11.
                if (!edge_exists ||
                    (edge_exists && *(first_neighbour) < *(second_first_degree_negihbour)))
                {
                    std::vector<uint32_t> group = {root, *(first_neighbour),
                                                   *(second_first_degree_negihbour),
                                                   *(second_degree_neighbour)};
                    count_group(compute_motif_descriptor(group, graph_adjacency_matrix), group);
                }
            }
        }

        // variation - depths 1, 2, 2
        for (auto first_second_degree_neighbour = second_degree_neighbours_begin;
             first_second_degree_neighbour != second_degree_neighbours_end;
             ++first_second_degree_neighbour)
        {
            if (visited_vertices_to_ignore[*(first_second_degree_neighbour)])
            {
                continue;
            }
            for (auto second_second_degree_neighbour = first_second_degree_neighbour + 1;
                 second_second_degree_neighbour != second_degree_neighbours_end;
                 ++second_second_degree_neighbour)
            {
                if (visited_vertices_to_ignore[*(second_second_degree_neighbour)])
                {
                    continue;
                }
                std::vector<uint32_t> group = {root, *(first_neighbour),
                                               *(first_second_degree_neighbour),
                                               *(second_second_degree_neighbour)};
                count_group(compute_motif_descriptor(group, graph_adjacency_matrix), group);
            }
        }
    }


    // variation - one vertex of each depth (root, 1, 2, 3)
    for (auto first_degree_neighbour = first_degree_neighbours_begin;
         first_degree_neighbour != first_degree_neighbours_end; ++first_degree_neighbour)
    {
        if (visited_vertices_to_ignore[*(first_degree_neighbour)])
        {
            continue;
        }
        auto [second_degree_neighbours_begin, second_degree_neighbours_end] =
            m_graph.get_neighbours(*first_degree_neighbour);
        for (auto second_degree_neighbour = second_degree_neighbours_begin;
             second_degree_neighbour != second_degree_neighbours_end; ++second_degree_neighbour)
        {
            if (visited_vertices_to_ignore[*(second_degree_neighbour)] ||
                bfs_visited_vertices[*(second_degree_neighbour)] != (run_id + 2))
            {
                continue;
            }
            auto [third_degree_neighbours_begin, third_degree_neighbours_end] =
                m_graph.get_neighbours(*second_degree_neighbour);
            for (auto third_degree_neighbour = third_degree_neighbours_begin;
                 third_degree_neighbour != third_degree_neighbours_end; ++third_degree_neighbour)
            {
                if (visited_vertices_to_ignore[*(third_degree_neighbour)])
                {
                    continue;
                }
                if ((bfs_visited_vertices[*third_degree_neighbour] >> 2) != root)
                {
                    bfs_visited_vertices[*third_degree_neighbour] =
                        run_id + 3;  // mark as visited with depth 3
                    std::vector<uint32_t> group = {root, *(first_degree_neighbour),
                                                   *(second_degree_neighbour),
                                                   *(third_degree_neighbour)};
                    count_group(compute_motif_descriptor(group, graph_adjacency_matrix), group);
                }
                else if (bfs_visited_vertices[*third_degree_neighbour] == (run_id + 1))
                {
                    continue;
                }
                else if (bfs_visited_vertices[*third_degree_neighbour] == (run_id + 2) &&
                         !(graph_adjacency_matrix[*(first_degree_neighbour)]
                                                 [*third_degree_neighbour] ||
                           graph_adjacency_matrix[*third_degree_neighbour]
                                                 [*(first_degree_neighbour)]))
                {
                    std::vector<uint32_t> group = {root, *(first_degree_neighbour),
                                                   *(second_degree_neighbour),
                                                   *(third_degree_neighbour)};
                    count_group(compute_motif_descriptor(group, graph_adjacency_matrix), group);
                }
                else if (bfs_visited_vertices[*third_degree_neighbour] == (run_id + 3))
                {
                    std::vector<uint32_t> group = {root, *(first_degree_neighbour),
                                                   *(second_degree_neighbour),
                                                   *(third_degree_neighbour)};
                    count_group(compute_motif_descriptor(group, graph_adjacency_matrix), group);
                }
            }
        }
    }
}

__uint128_t MotifPreprocessor::calculate_motif_number(const uint32_t motif_descriptor,
                                                      const std::vector<uint32_t>& node_colors)
{
    __uint128_t canonical_motif_num = 0;
    __uint128_t minimal_colors = ~static_cast<__uint128_t>(0);
    const std::unordered_map<uint32_t, MotifCanonical>& motif_map =
        m_graph.is_directed() ? DIRECTED_MOTIF_CANONICAL_MAP : UNDIRECTED_MOTIF_CANONICAL_MAP;
    const MotifCanonical motif_canonical = motif_map.at(motif_descriptor);
    uint32_t minimal_motif_num = motif_canonical.minimal_motif_num;
    for (std::array<uint32_t, SgfConstants::MOTIF_SIZE> color_permutation :
         motif_canonical.color_permutations)
    {
        __uint128_t color_permutation_number = 0;
        for (size_t color_index = 0; color_index < SgfConstants::MOTIF_SIZE; ++color_index)
        {
            color_permutation_number += node_colors[color_permutation[color_index]]
                                        << ((color_index)*SgfConstants::BITS_PER_COLOR);
        }
        minimal_colors = std::min(minimal_colors, color_permutation_number);
    }
    canonical_motif_num = (static_cast<__uint128_t>(minimal_motif_num)
                           << (SgfConstants::MOTIF_SIZE * SgfConstants::BITS_PER_COLOR)) |
                          minimal_colors;
    return canonical_motif_num;
}

uint32_t MotifPreprocessor::compute_motif_descriptor(
    const std::vector<uint32_t>& group,
    const std::vector<std::vector<bool>>& graph_adjacency_matrix) const
{
    uint32_t motif_descriptor = 0;
    for (size_t row_index = 0; row_index < SgfConstants::MOTIF_SIZE; ++row_index)
    {
        // Directed: start each row at column 0 (all pairs, both triangles).
        // Undirected: start at row_index + 1 (upper triangle only, each pair once).
        size_t column_index = m_graph.is_directed() ? 0 : row_index + 1;
        for (; column_index < SgfConstants::MOTIF_SIZE; ++column_index)
        {
            if (row_index == column_index)
            {
                continue;  // Skip the diagonal — self-loops are not supported.
            }
            // Shift the accumulated bits left to make room for the next bit,
            // then OR in the edge presence. First pair read ends up in the MSB.
            motif_descriptor <<= 1;
            motif_descriptor +=
                graph_adjacency_matrix[group[row_index]][group[column_index]] ? 1 : 0;
        }
    }
    return motif_descriptor;
}

}  // namespace sgf
