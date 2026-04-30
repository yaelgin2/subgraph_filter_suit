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

void MotifPreprocessor::mark_depth_one_neighbours(KavoshContext& ctx,
                                                  const NeighbourRange& depth_one) const
{
    for (auto vertex = depth_one.begin; vertex != depth_one.end; ++vertex)
    {
        ctx.bfs_visited[*vertex] = ctx.run_id + 1U;
    }
}

void MotifPreprocessor::emit_depth_1_1_1_groups(const KavoshContext& ctx,
                                                const NeighbourRange& depth_one) const
{
    for (auto first = depth_one.begin; first != depth_one.end; ++first)
    {
        if (ctx.ignore_vertices[*first])
        {
            continue;
        }
        for (auto second = first + 1; second != depth_one.end; ++second)
        {
            if (ctx.ignore_vertices[*second])
            {
                continue;
            }
            for (auto third = second + 1; third != depth_one.end; ++third)
            {
                if (ctx.ignore_vertices[*third])
                {
                    continue;
                }
                const std::vector<uint32_t> group = {ctx.root, *first, *second, *third};
                ctx.count_group(compute_motif_descriptor(group, ctx.adjacency_matrix), group);
            }
        }
    }
}

void MotifPreprocessor::mark_depth_two_neighbours(KavoshContext& ctx,
                                                  const NeighbourRange& depth_two) const
{
    for (auto vertex = depth_two.begin; vertex != depth_two.end; ++vertex)
    {
        // Only mark vertices not already seen in this run; depth-1 marks must not be overwritten.
        if ((ctx.bfs_visited[*vertex] >> BFS_VERTEX_RUN_SHIFT) != static_cast<uint64_t>(ctx.root))
        {
            ctx.bfs_visited[*vertex] = ctx.run_id + BFS_DEPTH_TWO_OFFSET;
        }
    }
}

void MotifPreprocessor::emit_depth_1_1_2_for_neighbour(const KavoshContext& ctx,
                                                       const uint32_t first_neighbour,
                                                       const NeighbourRange& depth_one,
                                                       const NeighbourRange& depth_two) const
{
    for (auto n2 = depth_two.begin; n2 != depth_two.end; ++n2)
    {
        if (ctx.ignore_vertices[*n2] ||
            ctx.bfs_visited[*n2] != ctx.run_id + BFS_DEPTH_TWO_OFFSET)
        {
            continue;
        }
        for (auto n11 = depth_one.begin; n11 != depth_one.end; ++n11)
        {
            if (ctx.ignore_vertices[*n11] || first_neighbour == *n11)
            {
                continue;
            }
            const bool edge_exists =
                ctx.adjacency_matrix[*n11][*n2] || ctx.adjacency_matrix[*n2][*n11];
            // When n11–n2 edge exists the group appears under both first_neighbour and n11 as anchor;
            // emit only once by requiring first_neighbour < n11.
            if (!edge_exists || first_neighbour < *n11)
            {
                const std::vector<uint32_t> group = {ctx.root, first_neighbour, *n11, *n2};
                ctx.count_group(compute_motif_descriptor(group, ctx.adjacency_matrix), group);
            }
        }
    }
}

void MotifPreprocessor::emit_depth_1_2_2_for_neighbour(const KavoshContext& ctx,
                                                       const uint32_t first_neighbour,
                                                       const NeighbourRange& depth_two) const
{
    for (auto s1 = depth_two.begin; s1 != depth_two.end; ++s1)
    {
        if (ctx.ignore_vertices[*s1] ||
            ctx.bfs_visited[*s1] != ctx.run_id + BFS_DEPTH_TWO_OFFSET)
        {
            continue;
        }
        for (auto s2 = s1 + 1; s2 != depth_two.end; ++s2)
        {
            if (ctx.ignore_vertices[*s2] ||
                ctx.bfs_visited[*s2] != ctx.run_id + BFS_DEPTH_TWO_OFFSET)
            {
                continue;
            }
            const std::vector<uint32_t> group = {ctx.root, first_neighbour, *s1, *s2};
            ctx.count_group(compute_motif_descriptor(group, ctx.adjacency_matrix), group);
        }
    }
}

void MotifPreprocessor::emit_depth_1_1_2_and_1_2_2_groups(KavoshContext& ctx,
                                                           const NeighbourRange& depth_one) const
{
    for (auto first = depth_one.begin; first != depth_one.end; ++first)
    {
        if (ctx.ignore_vertices[*first])
        {
            continue;
        }
        const auto [depth_two_begin, depth_two_end] = m_graph.get_neighbours(*first);
        const NeighbourRange depth_two{depth_two_begin, depth_two_end};
        mark_depth_two_neighbours(ctx, depth_two);
        emit_depth_1_1_2_for_neighbour(ctx, *first, depth_one, depth_two);
        emit_depth_1_2_2_for_neighbour(ctx, *first, depth_two);
    }
}

void MotifPreprocessor::emit_depth_1_2_3_for_second_degree(
    KavoshContext& ctx, const uint32_t first_degree_vertex,
    const uint32_t second_degree_vertex, const NeighbourRange& third_degree) const
{
    for (auto n3 = third_degree.begin; n3 != third_degree.end; ++n3)
    {
        if (ctx.ignore_vertices[*n3])
        {
            continue;
        }
        const std::vector<uint32_t> group = {ctx.root, first_degree_vertex, second_degree_vertex,
                                             *n3};
        const bool is_new =
            (ctx.bfs_visited[*n3] >> BFS_VERTEX_RUN_SHIFT) != static_cast<uint64_t>(ctx.root);
        // A depth-2 vertex reachable via n2 but with no direct edge to n1 forms a genuine
        // (1,2,3) path and must be emitted; a depth-2 vertex with a back-edge to n1 was
        // already counted by emit_depth_1_1_2, so skip it here to avoid double-counting.
        const bool is_depth_two_no_back_edge =
            ctx.bfs_visited[*n3] == ctx.run_id + BFS_DEPTH_TWO_OFFSET &&
            !ctx.adjacency_matrix[first_degree_vertex][*n3] &&
            !ctx.adjacency_matrix[*n3][first_degree_vertex];
        const bool is_depth_three = ctx.bfs_visited[*n3] == ctx.run_id + BFS_DEPTH_THREE_OFFSET;
        if (is_new)
        {
            ctx.bfs_visited[*n3] = ctx.run_id + BFS_DEPTH_THREE_OFFSET;
        }
        if (is_new || is_depth_two_no_back_edge || is_depth_three)
        {
            ctx.count_group(compute_motif_descriptor(group, ctx.adjacency_matrix), group);
        }
    }
}

void MotifPreprocessor::emit_depth_1_2_3_for_first_degree(KavoshContext& ctx,
                                                          const uint32_t first_degree_vertex,
                                                          const NeighbourRange& second_degree) const
{
    for (auto n2 = second_degree.begin; n2 != second_degree.end; ++n2)
    {
        if (ctx.ignore_vertices[*n2] ||
            ctx.bfs_visited[*n2] != ctx.run_id + BFS_DEPTH_TWO_OFFSET)
        {
            continue;
        }
        const auto [third_begin, third_end] = m_graph.get_neighbours(*n2);
        emit_depth_1_2_3_for_second_degree(ctx, first_degree_vertex, *n2,
                                           NeighbourRange{third_begin, third_end});
    }
}

void MotifPreprocessor::emit_depth_1_2_3_groups(KavoshContext& ctx,
                                                const NeighbourRange& depth_one) const
{
    for (auto n1 = depth_one.begin; n1 != depth_one.end; ++n1)
    {
        if (ctx.ignore_vertices[*n1])
        {
            continue;
        }
        const auto [second_begin, second_end] = m_graph.get_neighbours(*n1);
        emit_depth_1_2_3_for_first_degree(ctx, *n1, NeighbourRange{second_begin, second_end});
    }
}

void MotifPreprocessor::stream_groups_to_counter_for_vertex(
    const std::vector<std::vector<bool>>& graph_adjacency_matrix,
    const GroupCounterCallback& count_group,
    const std::vector<bool>& visited_vertices_to_ignore,
    std::vector<uint64_t>& bfs_visited_vertices,
    const uint32_t root)
{
    // Pack root into the upper bits so every bfs_visited entry encodes both its run (root) and
    // its BFS depth (low 2 bits), allowing stale entries from prior runs to be detected cheaply.
    const uint64_t run_id = static_cast<uint64_t>(root) << BFS_VERTEX_RUN_SHIFT;
    bfs_visited_vertices[root] = run_id;

    const auto [depth_one_begin, depth_one_end] = m_graph.get_neighbours(root);
    const NeighbourRange depth_one{depth_one_begin, depth_one_end};

    KavoshContext ctx{graph_adjacency_matrix, count_group, visited_vertices_to_ignore,
                      bfs_visited_vertices, run_id, root};
    mark_depth_one_neighbours(ctx, depth_one);
    emit_depth_1_1_1_groups(ctx, depth_one);
    emit_depth_1_1_2_and_1_2_2_groups(ctx, depth_one);
    emit_depth_1_2_3_groups(ctx, depth_one);
}

__uint128_t MotifPreprocessor::calculate_motif_number(const uint32_t motif_descriptor,
                                                      const std::vector<uint32_t>& node_colors)
{
    __uint128_t minimal_colors = ~static_cast<__uint128_t>(0);  // Start at max; reduced by std::min.
    const std::unordered_map<uint32_t, MotifCanonical>& motif_map =
        m_graph.is_directed() ? DIRECTED_MOTIF_CANONICAL_MAP : UNDIRECTED_MOTIF_CANONICAL_MAP;
    const MotifCanonical motif_canonical = motif_map.at(motif_descriptor);
    const uint32_t minimal_motif_num = motif_canonical.minimal_motif_num;
    for (std::array<uint32_t, SgfConstants::MOTIF_SIZE> color_permutation :
         motif_canonical.color_permutations)
    {
        __uint128_t color_permutation_number = 0;
        for (size_t color_index = 0; color_index < SgfConstants::MOTIF_SIZE; ++color_index)
        {
            color_permutation_number += static_cast<__uint128_t>(
                                            node_colors[color_permutation[color_index]])
                                        << (color_index * SgfConstants::BITS_PER_COLOR);
        }
        minimal_colors = std::min(minimal_colors, color_permutation_number);
    }
    return (static_cast<__uint128_t>(minimal_motif_num)
            << (SgfConstants::MOTIF_SIZE * SgfConstants::BITS_PER_COLOR)) |
           minimal_colors;
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
