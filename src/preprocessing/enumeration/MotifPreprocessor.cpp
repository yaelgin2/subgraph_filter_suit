#include "MotifPreprocessor.h"

#include "ColoredGraph.h"
#include "Constants.h"
#include "GroupEnumerationPreprocessor.h"
#include "LoggerHandler.h"
#include "MotifMap.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sgf
{

MotifPreprocessor::MotifPreprocessor(const ColoredGraph& graph, LoggerHandler logger)
    : GroupEnmerationPreprocessor(graph, std::move(logger))
{
}

size_t MotifPreprocessor::combined_degree(const uint32_t vertex) const
{
    const auto [out_begin, out_end] = m_graph.get_neighbours(vertex);
    const size_t out_degree = static_cast<size_t>(out_end - out_begin);
    if (!m_graph.is_directed())
    {
        return out_degree;
    }
    const auto [in_begin, in_end] = m_graph.get_neighbours(vertex, true);
    return out_degree + static_cast<size_t>(in_end - in_begin);
}

void MotifPreprocessor::sort_nodes()
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

void MotifPreprocessor::stream_groups_to_counter(
    const std::vector<std::vector<bool>>& graph_adjacency_matrix,
    const GroupCounterCallback& count_group)
{
    std::vector<int64_t> bfs_visited(m_graph.vertex_count(), -1);
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
    for (auto vertex = depth_one.m_begin; vertex != depth_one.m_end; ++vertex)
    {
        ctx.m_bfs_visited[*vertex] = ctx.m_run_id + 1U;
    }
    if (m_graph.is_directed())
    {
        for (auto vertex = depth_one.m_rev_begin; vertex != depth_one.m_rev_end; ++vertex)
        {
            ctx.m_bfs_visited[*vertex] = ctx.m_run_id + 1U;
        }
    }
}

void MotifPreprocessor::emit_depth_1_1_1_groups(const KavoshContext& ctx,
                                                const NeighbourRange& depth_one) const
{
    for (auto first = depth_one.m_begin; first != depth_one.m_end; ++first)
    {
        if (ctx.m_ignore_vertices[*first])
        {
            continue;
        }
        emit_depth_1_1_1_groups_first_vertex_chosen(ctx, depth_one, first, false);
    }
    if (m_graph.is_directed())
    {
        for (auto first = depth_one.m_rev_begin; first != depth_one.m_rev_end; ++first)
        {
            if (ctx.m_ignore_vertices[*first] || ctx.m_adjacency_matrix[ctx.m_root][*first])
            {
                continue;
            }
            emit_depth_1_1_1_groups_first_vertex_chosen(ctx, depth_one, first, true);
        }
    }
}

// NOLINTNEXTLINE(readability-function-size)
void MotifPreprocessor::emit_depth_1_1_1_groups_first_vertex_chosen(
    const KavoshContext& ctx, const NeighbourRange& depth_one,
    std::vector<uint32_t>::const_iterator first_neighbour, bool is_first_neighbour_reversed) const
{
    if (ctx.m_ignore_vertices[*first_neighbour])
    {
        return;
    }
    if (!is_first_neighbour_reversed)
    {
        for (auto second = first_neighbour + 1; second != depth_one.m_end; ++second)
        {
            if (ctx.m_ignore_vertices[*second])
            {
                continue;
            }
            emit_depth_1_1_1_groups_second_vertex_chosen(ctx, depth_one, first_neighbour, second,
                                                         false);
        }
    }
    if (m_graph.is_directed())
    {
        auto second = is_first_neighbour_reversed ? first_neighbour + 1 : depth_one.m_rev_begin;
        for (; second != depth_one.m_rev_end; ++second)
        {
            if (ctx.m_ignore_vertices[*second] || ctx.m_adjacency_matrix[ctx.m_root][*second])
            {
                continue;
            }
            emit_depth_1_1_1_groups_second_vertex_chosen(ctx, depth_one, first_neighbour, second,
                                                         true);
        }
    }
}

// NOLINTNEXTLINE(readability-function-size)
void MotifPreprocessor::emit_depth_1_1_1_groups_second_vertex_chosen(
    const KavoshContext& ctx, const NeighbourRange& depth_one,
    std::vector<uint32_t>::const_iterator first_neighbour,
    std::vector<uint32_t>::const_iterator second_neighbour, bool is_second_neighbour_reversed) const
{
    if (!is_second_neighbour_reversed)
    {
        for (auto third = second_neighbour + 1; third != depth_one.m_end; ++third)
        {
            if (ctx.m_ignore_vertices[*third])
            {
                continue;
            }
            const std::vector<uint32_t> group = {ctx.m_root, *first_neighbour, *second_neighbour,
                                                 *third};
            ctx.m_count_group(compute_motif_descriptor(group, ctx.m_adjacency_matrix), group);
        }
    }
    if (m_graph.is_directed())
    {
        auto third = is_second_neighbour_reversed ? second_neighbour + 1 : depth_one.m_rev_begin;
        for (; third != depth_one.m_rev_end; ++third)
        {
            if (ctx.m_ignore_vertices[*third] || ctx.m_adjacency_matrix[ctx.m_root][*third])
            {
                continue;
            }
            const std::vector<uint32_t> group = {ctx.m_root, *first_neighbour, *second_neighbour,
                                                 *third};
            ctx.m_count_group(compute_motif_descriptor(group, ctx.m_adjacency_matrix), group);
        }
    }
}

void MotifPreprocessor::mark_depth_two_neighbours(KavoshContext& ctx,
                                                  const NeighbourRange& depth_two) const
{
    for (auto vertex = depth_two.m_begin; vertex != depth_two.m_end; ++vertex)
    {
        if ((static_cast<uint64_t>(ctx.m_bfs_visited[*vertex]) >> BFS_VERTEX_RUN_SHIFT) !=
            static_cast<uint64_t>(ctx.m_root))
        {
            ctx.m_bfs_visited[*vertex] = ctx.m_run_id + static_cast<int64_t>(BFS_DEPTH_TWO_OFFSET);
        }
    }
    if (m_graph.is_directed())
    {
        for (auto vertex = depth_two.m_rev_begin; vertex != depth_two.m_rev_end; ++vertex)
        {
            if ((static_cast<uint64_t>(ctx.m_bfs_visited[*vertex]) >> BFS_VERTEX_RUN_SHIFT) !=
                static_cast<uint64_t>(ctx.m_root))
            {
                ctx.m_bfs_visited[*vertex] =
                    ctx.m_run_id + static_cast<int64_t>(BFS_DEPTH_TWO_OFFSET);
            }
        }
    }
}

void MotifPreprocessor::emit_depth_1_1_2_for_first_vertex(
    const KavoshContext& ctx, std::vector<uint32_t>::const_iterator first_neighbour,
    const NeighbourRange& depth_one, const NeighbourRange& depth_two) const
{
    for (auto second_degree_neighbour = depth_two.m_begin;
         second_degree_neighbour != depth_two.m_end; ++second_degree_neighbour)
    {
        if (ctx.m_ignore_vertices[*(second_degree_neighbour)] ||
            ctx.m_bfs_visited[*(second_degree_neighbour)] !=
                static_cast<int64_t>(ctx.m_run_id + BFS_DEPTH_TWO_OFFSET))
        {
            continue;
        }
        emit_depth_1_1_2_for_second_vertex(ctx, first_neighbour, depth_one,
                                           second_degree_neighbour);
    }
    if (m_graph.is_directed())
    {
        for (auto second_degree_neighbour = depth_two.m_rev_begin;
             second_degree_neighbour != depth_two.m_rev_end; ++second_degree_neighbour)
        {
            if (ctx.m_ignore_vertices[*(second_degree_neighbour)] ||
                ctx.m_bfs_visited[*(second_degree_neighbour)] != (ctx.m_run_id + 2) ||
                ctx.m_adjacency_matrix[(*first_neighbour)][*(second_degree_neighbour)])
            {
                continue;
            }
            emit_depth_1_1_2_for_second_vertex(ctx, first_neighbour, depth_one,
                                               second_degree_neighbour);
        }
    }
}

// NOLINTNEXTLINE(readability-function-size)
void MotifPreprocessor::emit_depth_1_1_2_for_second_vertex(
    const KavoshContext& ctx, std::vector<uint32_t>::const_iterator first_neighbour,
    const NeighbourRange& depth_one, std::vector<uint32_t>::const_iterator second_neighbour) const
{
    for (auto second_first_degree_negihbour = depth_one.m_begin;
         second_first_degree_negihbour != depth_one.m_end; ++second_first_degree_negihbour)
    {
        if (ctx.m_ignore_vertices[*(second_first_degree_negihbour)] ||
            *first_neighbour == *second_first_degree_negihbour)
        {
            continue;
        }
        const bool edge_exists =
            ctx.m_adjacency_matrix[*(second_first_degree_negihbour)][*(second_neighbour)] ||
            ctx.m_adjacency_matrix[*(second_neighbour)][*(second_first_degree_negihbour)];
        // avoid double-counting due to two paths from root to n2 - from n1 and from n11.
        if (!edge_exists || (edge_exists && *(first_neighbour) < *(second_first_degree_negihbour)))
        {
            const std::vector<uint32_t> group = {ctx.m_root, *(first_neighbour),
                                                 *(second_first_degree_negihbour),
                                                 *(second_neighbour)};
            ctx.m_count_group(compute_motif_descriptor(group, ctx.m_adjacency_matrix), group);
        }
    }
    if (m_graph.is_directed())
    {
        for (auto second_first_degree_negihbour = depth_one.m_rev_begin;
             second_first_degree_negihbour != depth_one.m_rev_end; ++second_first_degree_negihbour)
        {
            if (ctx.m_ignore_vertices[*(second_first_degree_negihbour)] ||
                *first_neighbour == *second_first_degree_negihbour ||
                ctx.m_adjacency_matrix[ctx.m_root][*(second_first_degree_negihbour)])
            {
                continue;
            }
            const bool edge_exists =
                ctx.m_adjacency_matrix[*(second_first_degree_negihbour)][*(second_neighbour)] ||
                ctx.m_adjacency_matrix[*(second_neighbour)][*(second_first_degree_negihbour)];
            // avoid double-counting due to two paths from root to n2 - from n1 and from n11.
            if (!edge_exists ||
                (edge_exists && *(first_neighbour) < *(second_first_degree_negihbour)))
            {
                const std::vector<uint32_t> group = {ctx.m_root, *(first_neighbour),
                                                     *(second_first_degree_negihbour),
                                                     *(second_neighbour)};
                ctx.m_count_group(compute_motif_descriptor(group, ctx.m_adjacency_matrix), group);
            }
        }
    }
}

void MotifPreprocessor::emit_depth_1_2_2_for_first_vertex(
    const KavoshContext& ctx, std::vector<uint32_t>::const_iterator first_neighbour,
    const NeighbourRange& depth_two) const
{
    for (auto first_second_degree_neighbour = depth_two.m_begin;
         first_second_degree_neighbour != depth_two.m_end; ++first_second_degree_neighbour)
    {
        if (ctx.m_ignore_vertices[*(first_second_degree_neighbour)] ||
            ctx.m_bfs_visited[*(first_second_degree_neighbour)] !=
                static_cast<int64_t>(ctx.m_run_id + BFS_DEPTH_TWO_OFFSET))
        {
            continue;
        }
        emit_depth_1_2_2_for_second_vertex(ctx, first_neighbour, depth_two,
                                           first_second_degree_neighbour, false);
    }
    if (m_graph.is_directed())
    {
        for (auto first_second_degree_neighbour = depth_two.m_rev_begin;
             first_second_degree_neighbour != depth_two.m_rev_end; ++first_second_degree_neighbour)
        {
            if (ctx.m_ignore_vertices[*(first_second_degree_neighbour)] ||
                (ctx.m_bfs_visited[*(first_second_degree_neighbour)] !=
                 static_cast<int64_t>(ctx.m_run_id + BFS_DEPTH_TWO_OFFSET)) ||
                ctx.m_adjacency_matrix[(*first_neighbour)][*(first_second_degree_neighbour)])
            {
                continue;
            }
            emit_depth_1_2_2_for_second_vertex(ctx, first_neighbour, depth_two,
                                               first_second_degree_neighbour, true);
        }
    }
}

// NOLINTNEXTLINE(readability-function-size)
void MotifPreprocessor::emit_depth_1_2_2_for_second_vertex(
    const KavoshContext& ctx, std::vector<uint32_t>::const_iterator first_neighbour,
    const NeighbourRange& depth_two, std::vector<uint32_t>::const_iterator second_neighbour,
    bool is_second_vertex_reversed) const
{
    if (!is_second_vertex_reversed)
    {
        for (auto second_second_degree_neighbour = second_neighbour + 1;
             second_second_degree_neighbour != depth_two.m_end; ++second_second_degree_neighbour)
        {
            if (ctx.m_ignore_vertices[*(second_second_degree_neighbour)] ||
                (ctx.m_bfs_visited[*(second_second_degree_neighbour)] !=
                 static_cast<int64_t>(ctx.m_run_id + BFS_DEPTH_TWO_OFFSET)))
            {
                continue;
            }
            const std::vector<uint32_t> group = {ctx.m_root, *(first_neighbour),
                                                 *(second_neighbour),
                                                 *(second_second_degree_neighbour)};
            ctx.m_count_group(compute_motif_descriptor(group, ctx.m_adjacency_matrix), group);
        }
    }
    if (m_graph.is_directed())
    {
        auto second_second_degree_neighbour =
            is_second_vertex_reversed ? second_neighbour + 1 : depth_two.m_rev_begin;
        for (; second_second_degree_neighbour != depth_two.m_rev_end;
             ++second_second_degree_neighbour)
        {
            if (ctx.m_ignore_vertices[*(second_second_degree_neighbour)] ||
                (ctx.m_bfs_visited[*(second_second_degree_neighbour)] !=
                 static_cast<int64_t>(ctx.m_run_id + BFS_DEPTH_TWO_OFFSET)) ||
                ctx.m_adjacency_matrix[*(first_neighbour)][*(second_second_degree_neighbour)])
            {
                continue;
            }
            const std::vector<uint32_t> group = {ctx.m_root, *(first_neighbour),
                                                 *(second_neighbour),
                                                 *(second_second_degree_neighbour)};
            ctx.m_count_group(compute_motif_descriptor(group, ctx.m_adjacency_matrix), group);
        }
    }
}

void MotifPreprocessor::process_first_neighbour_112_122(
    KavoshContext& ctx, std::vector<uint32_t>::const_iterator first_neighbour,
    const NeighbourRange& depth_one) const
{
    const auto [two_fwd_begin, two_fwd_end] = m_graph.get_neighbours(*first_neighbour);
    const auto [two_rev_begin, two_rev_end] = m_graph.is_directed()
                                                  ? m_graph.get_neighbours(*first_neighbour, true)
                                                  : std::make_pair(two_fwd_end, two_fwd_end);
    const NeighbourRange depth_two{two_fwd_begin, two_fwd_end, two_rev_begin, two_rev_end};
    mark_depth_two_neighbours(ctx, depth_two);
    emit_depth_1_1_2_for_first_vertex(ctx, first_neighbour, depth_one, depth_two);
    emit_depth_1_2_2_for_first_vertex(ctx, first_neighbour, depth_two);
}

void MotifPreprocessor::emit_depth_1_1_2_and_1_2_2_groups(KavoshContext& ctx,
                                                          const NeighbourRange& depth_one) const
{
    for (auto first = depth_one.m_begin; first != depth_one.m_end; ++first)
    {
        if (ctx.m_ignore_vertices[*first])
        {
            continue;
        }
        process_first_neighbour_112_122(ctx, first, depth_one);
    }
    if (m_graph.is_directed())
    {
        for (auto first = depth_one.m_rev_begin; first != depth_one.m_rev_end; ++first)
        {
            if (ctx.m_ignore_vertices[*first] || ctx.m_adjacency_matrix[ctx.m_root][*first])
            {
                continue;
            }
            process_first_neighbour_112_122(ctx, first, depth_one);
        }
    }
}

void MotifPreprocessor::emit_depth_1_2_3_for_third_vertex(KavoshContext& ctx,
                                                          uint32_t first_degree_vertex,
                                                          uint32_t second_degree_vertex,
                                                          uint32_t third_degree_vertex) const
{
    const std::vector<uint32_t> group = {ctx.m_root, first_degree_vertex, second_degree_vertex,
                                         third_degree_vertex};
    const bool is_new = (static_cast<uint64_t>(ctx.m_bfs_visited[third_degree_vertex]) >>
                         BFS_VERTEX_RUN_SHIFT) != static_cast<uint64_t>(ctx.m_root);
    // Depth-2 vertex reachable via n2 with no direct edge to n1: genuine (1,2,3) path.
    // Depth-2 vertex with back-edge to n1: already counted by emit_depth_1_1_2, skip.
    const bool is_depth_two_no_back_edge =
        (ctx.m_bfs_visited[third_degree_vertex] ==
         static_cast<int64_t>(ctx.m_run_id + BFS_DEPTH_TWO_OFFSET)) &&
        !ctx.m_adjacency_matrix[first_degree_vertex][third_degree_vertex] &&
        !ctx.m_adjacency_matrix[third_degree_vertex][first_degree_vertex];
    const bool is_depth_three = ctx.m_bfs_visited[third_degree_vertex] ==
                                static_cast<int64_t>(ctx.m_run_id + BFS_DEPTH_THREE_OFFSET);
    if (is_new)
    {
        ctx.m_bfs_visited[third_degree_vertex] =
            ctx.m_run_id + static_cast<int64_t>(BFS_DEPTH_THREE_OFFSET);
    }
    if (is_new || is_depth_two_no_back_edge || is_depth_three)
    {
        ctx.m_count_group(compute_motif_descriptor(group, ctx.m_adjacency_matrix), group);
    }
}

void MotifPreprocessor::emit_depth_1_2_3_for_second_vertex(KavoshContext& ctx,
                                                           uint32_t first_degree_vertex,
                                                           uint32_t second_degree_vertex,
                                                           const NeighbourRange& third_degree) const
{
    for (auto third_vertex = third_degree.m_begin; third_vertex != third_degree.m_end;
         ++third_vertex)
    {
        if (ctx.m_ignore_vertices[*third_vertex])
        {
            continue;
        }
        emit_depth_1_2_3_for_third_vertex(ctx, first_degree_vertex, second_degree_vertex,
                                          *third_vertex);
    }
    if (m_graph.is_directed())
    {
        for (auto third_vertex = third_degree.m_rev_begin; third_vertex != third_degree.m_rev_end;
             ++third_vertex)
        {
            if (ctx.m_ignore_vertices[*third_vertex] ||
                ctx.m_adjacency_matrix[second_degree_vertex][*third_vertex])
            {
                continue;
            }
            emit_depth_1_2_3_for_third_vertex(ctx, first_degree_vertex, second_degree_vertex,
                                              *third_vertex);
        }
    }
}

void MotifPreprocessor::emit_depth_1_2_3_for_first_vertex(KavoshContext& ctx,
                                                          const uint32_t first_degree_vertex,
                                                          const NeighbourRange& second_degree) const
{
    for (auto second_vertex = second_degree.m_begin; second_vertex != second_degree.m_end;
         ++second_vertex)
    {
        if (ctx.m_ignore_vertices[*second_vertex] ||
            ctx.m_bfs_visited[*second_vertex] !=
                static_cast<int64_t>(ctx.m_run_id + BFS_DEPTH_TWO_OFFSET))
        {
            continue;
        }
        const auto [three_fwd_begin, three_fwd_end] = m_graph.get_neighbours(*second_vertex);
        const auto [three_rev_begin, three_rev_end] =
            m_graph.is_directed() ? m_graph.get_neighbours(*second_vertex, true)
                                  : std::make_pair(three_fwd_end, three_fwd_end);
        emit_depth_1_2_3_for_second_vertex(
            ctx, first_degree_vertex, *second_vertex,
            NeighbourRange{three_fwd_begin, three_fwd_end, three_rev_begin, three_rev_end});
    }
    if (m_graph.is_directed())
    {
        for (auto second_vertex = second_degree.m_rev_begin;
             second_vertex != second_degree.m_rev_end; ++second_vertex)
        {
            if (ctx.m_ignore_vertices[*second_vertex] ||
                ctx.m_bfs_visited[*second_vertex] !=
                    static_cast<int64_t>(ctx.m_run_id + BFS_DEPTH_TWO_OFFSET) ||
                ctx.m_adjacency_matrix[first_degree_vertex][*second_vertex])
            {
                continue;
            }
            const auto [three_fwd_begin, three_fwd_end] = m_graph.get_neighbours(*second_vertex);
            const auto [three_rev_begin, three_rev_end] =
                m_graph.is_directed() ? m_graph.get_neighbours(*second_vertex, true)
                                      : std::make_pair(three_fwd_end, three_fwd_end);
            emit_depth_1_2_3_for_second_vertex(
                ctx, first_degree_vertex, *second_vertex,
                NeighbourRange{three_fwd_begin, three_fwd_end, three_rev_begin, three_rev_end});
        }
    }
}

void MotifPreprocessor::emit_depth_1_2_3_groups(KavoshContext& ctx,
                                                const NeighbourRange& depth_one) const
{
    for (auto first_vertex = depth_one.m_begin; first_vertex != depth_one.m_end; ++first_vertex)
    {
        if (ctx.m_ignore_vertices[*first_vertex])
        {
            continue;
        }
        const auto [sec_fwd_begin, sec_fwd_end] = m_graph.get_neighbours(*first_vertex);
        const auto [sec_rev_begin, sec_rev_end] = m_graph.is_directed()
                                                      ? m_graph.get_neighbours(*first_vertex, true)
                                                      : std::make_pair(sec_fwd_end, sec_fwd_end);
        emit_depth_1_2_3_for_first_vertex(
            ctx, *first_vertex,
            NeighbourRange{sec_fwd_begin, sec_fwd_end, sec_rev_begin, sec_rev_end});
    }
    if (m_graph.is_directed())
    {
        for (auto first_vertex = depth_one.m_rev_begin; first_vertex != depth_one.m_rev_end;
             ++first_vertex)
        {
            if (ctx.m_ignore_vertices[*first_vertex] ||
                ctx.m_adjacency_matrix[ctx.m_root][*first_vertex])
            {
                continue;
            }
            const auto [sec_fwd_begin, sec_fwd_end] = m_graph.get_neighbours(*first_vertex);
            const auto [sec_rev_begin, sec_rev_end] =
                m_graph.is_directed() ? m_graph.get_neighbours(*first_vertex, true)
                                      : std::make_pair(sec_fwd_end, sec_fwd_end);
            emit_depth_1_2_3_for_first_vertex(
                ctx, *first_vertex,
                NeighbourRange{sec_fwd_begin, sec_fwd_end, sec_rev_begin, sec_rev_end});
        }
    }
}

void MotifPreprocessor::stream_groups_to_counter_for_vertex(
    const std::vector<std::vector<bool>>& graph_adjacency_matrix,
    const GroupCounterCallback& count_group, const std::vector<bool>& visited_vertices_to_ignore,
    std::vector<int64_t>& bfs_visited_vertices, const uint32_t root)
{
    // Pack root into upper bits; low 2 bits encode BFS depth. Stale entries from prior roots
    // are detected by checking the upper bits against the current root.
    const int64_t run_id =
        static_cast<int64_t>(static_cast<uint64_t>(root) << BFS_VERTEX_RUN_SHIFT);
    bfs_visited_vertices[root] = run_id;

    const auto [one_fwd_begin, one_fwd_end] = m_graph.get_neighbours(root);
    const auto [one_rev_begin, one_rev_end] = m_graph.is_directed()
                                                  ? m_graph.get_neighbours(root, true)
                                                  : std::make_pair(one_fwd_end, one_fwd_end);
    const NeighbourRange depth_one{one_fwd_begin, one_fwd_end, one_rev_begin, one_rev_end};

    KavoshContext ctx{graph_adjacency_matrix, count_group, visited_vertices_to_ignore,
                      bfs_visited_vertices,   run_id,      root};
    mark_depth_one_neighbours(ctx, depth_one);
    emit_depth_1_1_1_groups(ctx, depth_one);
    emit_depth_1_1_2_and_1_2_2_groups(ctx, depth_one);
    emit_depth_1_2_3_groups(ctx, depth_one);
}

__uint128_t MotifPreprocessor::calculate_motif_number(const uint32_t motif_descriptor,
                                                      const std::vector<uint32_t>& node_colors)
{
    __uint128_t minimal_colors = ~static_cast<__uint128_t>(0);
    const std::unordered_map<uint32_t, MotifCanonical>& motif_map =
        m_graph.is_directed() ? DIRECTED_MOTIF_CANONICAL_MAP : UNDIRECTED_MOTIF_CANONICAL_MAP;
    const MotifCanonical motif_canonical = motif_map.at(motif_descriptor);
    const uint32_t minimal_motif_num = motif_canonical.m_minimal_motif_num;
    for (std::array<uint32_t, SgfConstants::MOTIF_SIZE> color_permutation :
         motif_canonical.m_color_permutations)
    {
        __uint128_t color_permutation_number = 0;
        for (size_t color_index = 0; color_index < SgfConstants::MOTIF_SIZE; ++color_index)
        {
            color_permutation_number +=
                static_cast<__uint128_t>(node_colors.at(color_permutation.at(color_index)))
                << (color_index * SgfConstants::BITS_PER_COLOR);
        }
        minimal_colors = std::min(minimal_colors, color_permutation_number);
    }
    return (static_cast<__uint128_t>(minimal_motif_num)
            << static_cast<uint32_t>(SgfConstants::MOTIF_SIZE * SgfConstants::BITS_PER_COLOR)) |
           minimal_colors;
}

uint32_t MotifPreprocessor::compute_motif_descriptor(
    const std::vector<uint32_t>& group,
    const std::vector<std::vector<bool>>& graph_adjacency_matrix) const
{
    uint32_t motif_descriptor = 0;
    for (size_t row_index = 0; row_index < SgfConstants::MOTIF_SIZE; ++row_index)
    {
        // Directed: all pairs (both triangles). Undirected: upper triangle only (each pair once).
        size_t column_index = m_graph.is_directed() ? 0 : row_index + 1;
        for (; column_index < SgfConstants::MOTIF_SIZE; ++column_index)
        {
            if (row_index == column_index)
            {
                continue;
            }
            motif_descriptor <<= 1U;
            motif_descriptor +=
                graph_adjacency_matrix[group[row_index]][group[column_index]] ? 1 : 0;
        }
    }
    return motif_descriptor;
}

}  // namespace sgf
