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

#include <sstream>
#include <iomanip>

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
    for (auto vertex = depth_one.begin; vertex != depth_one.end; ++vertex)
    {
        // m_logger.log(LogLevel::DEBUG, "Marking depth-1 vertex " + std::to_string(*vertex) + " for root " + std::to_string(ctx.root));
        ctx.bfs_visited[*vertex] = ctx.run_id + 1U;
    }
    if (m_graph.is_directed())
    {
        for (auto vertex = depth_one.rev_begin; vertex != depth_one.rev_end; ++vertex)
        {
            // m_logger.log(LogLevel::DEBUG, "Marking depth-1 vertex " + std::to_string(*vertex) + " for root " + std::to_string(ctx.root));
            ctx.bfs_visited[*vertex] = ctx.run_id + 1U;
        }
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
        emit_depth_1_1_1_groups_first_vertex_chosen(ctx, depth_one, first, false);
    }
    if (m_graph.is_directed())
    {
        for (auto first = depth_one.rev_begin; first != depth_one.rev_end; ++first)
        {
            if (ctx.ignore_vertices[*first] || ctx.adjacency_matrix[ctx.root][*first])
            {
                continue;
            }
            emit_depth_1_1_1_groups_first_vertex_chosen(ctx, depth_one, first, true);
        }
    }
}

void MotifPreprocessor::emit_depth_1_1_1_groups_first_vertex_chosen(const KavoshContext& ctx,
                                                const NeighbourRange& depth_one,
                                                std::vector<uint32_t>::const_iterator first_neighbour,
                                                bool is_first_neighbour_reversed) const
{
    if (ctx.ignore_vertices[*first_neighbour])
    {
        return;
    }
    if (!is_first_neighbour_reversed)
    {
        for (auto second = first_neighbour + 1; second != depth_one.end; ++second)
        {
            if (ctx.ignore_vertices[*second])
            {
                continue;
            }
            emit_depth_1_1_1_groups_second_vertex_chosen(ctx, depth_one, first_neighbour, second, false);
        }
    }
    if (m_graph.is_directed())
    {
        auto second = is_first_neighbour_reversed ? first_neighbour + 1 : depth_one.rev_begin;
        for (; second != depth_one.rev_end; ++second)
        {
            if (ctx.ignore_vertices[*second] || ctx.adjacency_matrix[ctx.root][*second])
            {
                continue;
            }
            emit_depth_1_1_1_groups_second_vertex_chosen(ctx, depth_one, first_neighbour, second, true);
        }
    }
}

void MotifPreprocessor::emit_depth_1_1_1_groups_second_vertex_chosen(const KavoshContext& ctx,
                                            const NeighbourRange& depth_one,
                                            std::vector<uint32_t>::const_iterator first_neighbour,
                                            std::vector<uint32_t>::const_iterator second_neighbour,
                                            bool is_second_neighbour_reversed) const
                                            {
    if (!is_second_neighbour_reversed)
    {
        for (auto third = second_neighbour + 1; third != depth_one.end; ++third)
        {
            if (ctx.ignore_vertices[*third])
            {
                continue;
            }
            const std::vector<uint32_t> group = {ctx.root, *first_neighbour, *second_neighbour, *third};
            //m_logger.log(LogLevel::DEBUG, 
            //    "Found group of structure 1 1 1 vertices: " 
            //    + std::to_string(ctx.root) + ", " + std::to_string(*first_neighbour) + ", "+ std::to_string(*second_neighbour) + ", " + std::to_string(*third)); 
            ctx.count_group(compute_motif_descriptor(group, ctx.adjacency_matrix), group);
        }
    }
    if (m_graph.is_directed())
    {
        auto third = is_second_neighbour_reversed ? second_neighbour + 1 : depth_one.rev_begin;
        for (; third != depth_one.rev_end; ++third)
        {
            if (ctx.ignore_vertices[*third] || ctx.adjacency_matrix[ctx.root][*third])
            {
                continue;
            }
            const std::vector<uint32_t> group = {ctx.root, *first_neighbour, *second_neighbour, *third};
            //m_logger.log(LogLevel::DEBUG, 
            //        "Found group of structure 1 1 1 vertices: " 
            //        + std::to_string(ctx.root) + ", " + std::to_string(*first_neighbour) + ", "+ std::to_string(*second_neighbour) + ", " + std::to_string(*third)); 
            ctx.count_group(compute_motif_descriptor(group, ctx.adjacency_matrix), group);
        }
    }
}

void MotifPreprocessor::mark_depth_two_neighbours(KavoshContext& ctx,
                                                  const NeighbourRange& depth_two) const
{
    //m_logger.log(LogLevel::DEBUG, "Marking depth-2 neighbours, checking " + std::to_string(depth_two.end - depth_two.begin) + " vertices for root " + std::to_string(ctx.root));
    for (auto vertex = depth_two.begin; vertex != depth_two.end; ++vertex)
    {
        if ((ctx.bfs_visited[*vertex] >> BFS_VERTEX_RUN_SHIFT) != static_cast<int64_t>(ctx.root))
        {
            //m_logger.log(LogLevel::DEBUG, "Marking depth-2 vertex " + std::to_string(*vertex) + " for root " + std::to_string(ctx.root));
            ctx.bfs_visited[*vertex] = ctx.run_id + BFS_DEPTH_TWO_OFFSET;
        }
        else
        {
            //m_logger.log(LogLevel::DEBUG, "Already visited vertex " + std::to_string(*vertex) + " at depth " + std::to_string(ctx.bfs_visited[*vertex] % 4) + " for root " + std::to_string(ctx.root));
        }
    }
    if (m_graph.is_directed())
    {
        for (auto vertex = depth_two.rev_begin; vertex != depth_two.rev_end; ++vertex)
        {
            if ((ctx.bfs_visited[*vertex] >> BFS_VERTEX_RUN_SHIFT) != static_cast<int64_t>(ctx.root))
            {
                ctx.bfs_visited[*vertex] = ctx.run_id + BFS_DEPTH_TWO_OFFSET;
            }
        }
    }
}

void MotifPreprocessor::emit_depth_1_1_2_for_first_vertex(const KavoshContext& ctx,
                                                       std::vector<uint32_t>::const_iterator first_neighbour,
                                                       const NeighbourRange& depth_one,
                                                       const NeighbourRange& depth_two) const
{
    //m_logger.log(LogLevel::DEBUG, "Processing depth 1_1_2 groups");
    for (auto second_degree_neighbour = depth_two.begin; 
            second_degree_neighbour != depth_two.end;
             ++second_degree_neighbour)
    {
        //m_logger.log(LogLevel::DEBUG, "Processing depth-2 neighbour " + std::to_string(*second_degree_neighbour) + " for root " + std::to_string(ctx.root) + " and depth-1 neighbour " + std::to_string(*first_neighbour) + " for depth 1_1_2 groups");
        if (ctx.ignore_vertices[*(second_degree_neighbour)] ||
            ctx.bfs_visited[*(second_degree_neighbour)] != (ctx.run_id + BFS_DEPTH_TWO_OFFSET))
        {
            if (ctx.ignore_vertices[*(second_degree_neighbour)]) {
                //m_logger.log(LogLevel::DEBUG, "Skipping depth-2 neighbour " + std::to_string(*second_degree_neighbour) + " for depth 1_1_2 groups because it is ignored");
            }
            else
            {
                //m_logger.log(LogLevel::DEBUG, "Skipping depth-2 neighbour " + std::to_string(*second_degree_neighbour) + " for depth 1_1_2 groups because it was marked as " + std::to_string(ctx.bfs_visited[*(second_degree_neighbour)] % 4) + " instead of " + std::to_string(BFS_DEPTH_TWO_OFFSET));
            }

            continue;
        }
        emit_depth_1_1_2_for_second_vertex(ctx, first_neighbour, depth_one, second_degree_neighbour);
    }
    if (m_graph.is_directed())
    {
        for (auto second_degree_neighbour = depth_two.rev_begin; 
                second_degree_neighbour != depth_two.rev_end;
                ++second_degree_neighbour)
        {
            if (ctx.ignore_vertices[*(second_degree_neighbour)] ||
                ctx.bfs_visited[*(second_degree_neighbour)] != (ctx.run_id + 2) ||
                ctx.adjacency_matrix[(*first_neighbour)][*(second_degree_neighbour)])
            {
                continue;
            }
            emit_depth_1_1_2_for_second_vertex(ctx, first_neighbour, depth_one, second_degree_neighbour);
        }
    }
}

void MotifPreprocessor::emit_depth_1_1_2_for_second_vertex(const KavoshContext& ctx,
                                        std::vector<uint32_t>::const_iterator first_neighbour,
                                        const NeighbourRange& depth_one,
                                        std::vector<uint32_t>::const_iterator second_neighbour) const
{
    //m_logger.log(LogLevel::DEBUG, "Entered emit_depth_1_1_2_for_second_vertex");
    
    for(auto second_first_degree_negihbour = depth_one.begin;
            second_first_degree_negihbour != depth_one.end;
            ++second_first_degree_negihbour)
    {
        //m_logger.log(LogLevel::DEBUG, "Processing second depth-1 neighbour " + std::to_string(*second_first_degree_negihbour) + " for root " + std::to_string(ctx.root) + " and depth-1 neighbour " + std::to_string(*first_neighbour) + "and depth-2 neighbour " + + " for depth 1_1_2 groups");
        if (ctx.ignore_vertices[*(second_first_degree_negihbour)] || 
                *first_neighbour == *second_first_degree_negihbour)
            {
                if (ctx.ignore_vertices[*(second_first_degree_negihbour)])
                {
                    //m_logger.log(LogLevel::DEBUG, "Skipping second depth-1 neighbour " + std::to_string(*second_first_degree_negihbour) + " for depth 1_1_2 groups because it is ignored");
                }
                else
                {
                    //m_logger.log(LogLevel::DEBUG, "Skipping second depth-1 neighbour " + std::to_string(*second_first_degree_negihbour) + " for depth 1_1_2 groups because it is the same as the first depth-1 neighbour");
                }
                continue;
            }
            bool edge_exists = ctx.adjacency_matrix[*(second_first_degree_negihbour)][*(second_neighbour)] ||
                    ctx.adjacency_matrix[*(second_neighbour)][*(second_first_degree_negihbour)];
            //m_logger.log(LogLevel::DEBUG, "Edge exists between second depth-1 neighbour " + std::to_string(*second_first_degree_negihbour) + " and depth-2 neighbour " + std::to_string(*second_neighbour) + ": " + std::to_string(edge_exists));
            // avoid double-counting due to two paths from root to n2 - from n1 and from n11.
            //m_logger.log(LogLevel::DEBUG, "Is this group to count: " + std::to_string(edge_exists || (edge_exists && *(first_neighbour)<*(second_first_degree_negihbour))));
            if (!edge_exists || (edge_exists && *(first_neighbour)<*(second_first_degree_negihbour)))
            {
                std::vector<uint32_t> group = {ctx.root, *(first_neighbour), *(second_first_degree_negihbour), *(second_neighbour)};
                //m_logger.log(LogLevel::DEBUG, 
                //    "Found group of structure 1 1 2 vertices: " 
                //    + std::to_string(ctx.root) + ", " + std::to_string(*first_neighbour) + ", "+ std::to_string(*second_neighbour) + ", " + std::to_string(*second_first_degree_negihbour)); 

                ctx.count_group(compute_motif_descriptor(group, ctx.adjacency_matrix), group);
            }
    }
    if (m_graph.is_directed())
    {
        for(auto second_first_degree_negihbour = depth_one.rev_begin;
                second_first_degree_negihbour != depth_one.rev_end;
                ++second_first_degree_negihbour)
        {
            if (ctx.ignore_vertices[*(second_first_degree_negihbour)] ||
                    *first_neighbour == *second_first_degree_negihbour ||
                    ctx.adjacency_matrix[ctx.root][*(second_first_degree_negihbour)])
            {
                continue;
            }
            bool edge_exists = ctx.adjacency_matrix[*(second_first_degree_negihbour)][*(second_neighbour)] ||
                    ctx.adjacency_matrix[*(second_neighbour)][*(second_first_degree_negihbour)];
            // avoid double-counting due to two paths from root to n2 - from n1 and from n11.
            if (!edge_exists || (edge_exists && *(first_neighbour)<*(second_first_degree_negihbour)))
            {
                std::vector<uint32_t> group = {ctx.root, *(first_neighbour), *(second_first_degree_negihbour), *(second_neighbour)};
                //m_logger.log(LogLevel::DEBUG, 
                //    "Found group of structure 1 1 2 vertices: " 
                //    + std::to_string(ctx.root) + ", " + std::to_string(*first_neighbour) + ", "+ std::to_string(*second_neighbour) + ", " +  std::to_string(*second_first_degree_negihbour)); 
    
                ctx.count_group(compute_motif_descriptor(group, ctx.adjacency_matrix), group);
            }
        }
    }
}

void MotifPreprocessor::emit_depth_1_2_2_for_first_vertex(const KavoshContext& ctx,
                                                       std::vector<uint32_t>::const_iterator first_neighbour,
                                                        const NeighbourRange& depth_two) const
{
    //m_logger.log(LogLevel::DEBUG, "Processing depth 1_2_2 groups");
    for (auto first_second_degree_neighbour = depth_two.begin; 
            first_second_degree_neighbour != depth_two.end; 
            ++first_second_degree_neighbour)
    {
        if (ctx.ignore_vertices[*(first_second_degree_neighbour)] ||
            ctx.bfs_visited[*(first_second_degree_neighbour)] != ctx.run_id + BFS_DEPTH_TWO_OFFSET)
        {
            continue;
        }
        emit_depth_1_2_2_for_second_vertex(ctx, first_neighbour, depth_two, first_second_degree_neighbour, false);
    }
    if (m_graph.is_directed())
    {
        for (auto first_second_degree_neighbour = depth_two.rev_begin; 
            first_second_degree_neighbour != depth_two.rev_end; 
            ++first_second_degree_neighbour)
        {
            if (ctx.ignore_vertices[*(first_second_degree_neighbour)] ||
                ctx.bfs_visited[*(first_second_degree_neighbour)] != ctx.run_id + BFS_DEPTH_TWO_OFFSET ||
                ctx.adjacency_matrix[(*first_neighbour)][*(first_second_degree_neighbour)])
            {
                continue;
            }
            emit_depth_1_2_2_for_second_vertex(ctx, first_neighbour, depth_two, first_second_degree_neighbour, true);
        }
    }
}

void MotifPreprocessor::emit_depth_1_2_2_for_second_vertex(const KavoshContext& ctx,
                                                       std::vector<uint32_t>::const_iterator first_neighbour,
                                                       const NeighbourRange& depth_two,
                                                       std::vector<uint32_t>::const_iterator second_neighbour,
                                                       bool is_second_vertex_reversed) const
{
    if (!is_second_vertex_reversed)
    {
        for (auto second_second_degree_neighbour = second_neighbour+1; 
                second_second_degree_neighbour != depth_two.end; 
                ++second_second_degree_neighbour)
        {
            if (ctx.ignore_vertices[*(second_second_degree_neighbour)] ||
                ctx.bfs_visited[*(second_second_degree_neighbour)] != ctx.run_id + BFS_DEPTH_TWO_OFFSET)
            {
                continue;
            }
            std::vector<uint32_t> group = {ctx.root, *(first_neighbour), *(second_neighbour), *(second_second_degree_neighbour)};
            //m_logger.log(LogLevel::DEBUG,
            //            "Found group of structure 1 2 2 vertices: " 
            //            + std::to_string(ctx.root) + ", " + std::to_string(*first_neighbour) + ", "+ std::to_string(*second_neighbour) + ", " + std::to_string(*second_second_degree_neighbour)); 

            ctx.count_group(compute_motif_descriptor(group, ctx.adjacency_matrix), group);
        }
    }
    if (m_graph.is_directed())
    {
        auto second_second_degree_neighbour = is_second_vertex_reversed ? second_neighbour+1 : depth_two.rev_begin;
        for (; 
                second_second_degree_neighbour != depth_two.rev_end; 
                ++second_second_degree_neighbour)
        {
            if (ctx.ignore_vertices[*(second_second_degree_neighbour)] ||
                ctx.bfs_visited[*(second_second_degree_neighbour)] != ctx.run_id + BFS_DEPTH_TWO_OFFSET ||
                ctx.adjacency_matrix[*(first_neighbour)][*(second_second_degree_neighbour)])
            {
                continue;
            }
            std::vector<uint32_t> group = {ctx.root, *(first_neighbour), *(second_neighbour), *(second_second_degree_neighbour)};
            //m_logger.log(LogLevel::DEBUG, 
            //                "Found group of structure 1 2 2 vertices: " 
            //                + std::to_string(ctx.root) + ", " + std::to_string(*first_neighbour) + ", "+ std::to_string(*second_neighbour) + ", " + std::to_string(*second_second_degree_neighbour)); 

            ctx.count_group(compute_motif_descriptor(group, ctx.adjacency_matrix), group);
        }
    }
}

void MotifPreprocessor::process_first_neighbour_112_122(KavoshContext& ctx,
                                                        std::vector<uint32_t>::const_iterator first_neighbour,
                                                        const NeighbourRange& depth_one) const
{
    //m_logger.log(LogLevel::DEBUG, "Processing depth-1 neighbour " + std::to_string(*first_neighbour) + " for root " + std::to_string(ctx.root) + " for depth 1_1_2 or 1_2_2");
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
    for (auto first = depth_one.begin; first != depth_one.end; ++first)
    {
        if (ctx.ignore_vertices[*first])
        {
            continue;
        }
        process_first_neighbour_112_122(ctx, first, depth_one);
    }
    if (m_graph.is_directed())
    {
        for (auto first = depth_one.rev_begin; first != depth_one.rev_end; ++first)
        {
            if (ctx.ignore_vertices[*first] || ctx.adjacency_matrix[ctx.root][*first])
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
    //m_logger.log(LogLevel::DEBUG, "Processing group 1_2_3: " + std::to_string(ctx.root) + ", " + std::to_string(first_degree_vertex) + ", "+ std::to_string(second_degree_vertex) + ", " + std::to_string(third_degree_vertex));
    const std::vector<uint32_t> group = {ctx.root, first_degree_vertex, second_degree_vertex,
                                         third_degree_vertex};
    const bool is_new =
        (ctx.bfs_visited[third_degree_vertex] >> BFS_VERTEX_RUN_SHIFT) != static_cast<int64_t>(ctx.root);
    // Depth-2 vertex reachable via n2 with no direct edge to n1: genuine (1,2,3) path.
    // Depth-2 vertex with back-edge to n1: already counted by emit_depth_1_1_2, skip.
    const bool is_depth_two_no_back_edge =
        ctx.bfs_visited[third_degree_vertex] == ctx.run_id + BFS_DEPTH_TWO_OFFSET &&
        !ctx.adjacency_matrix[first_degree_vertex][third_degree_vertex] &&
        !ctx.adjacency_matrix[third_degree_vertex][first_degree_vertex];
    const bool is_depth_three =
        ctx.bfs_visited[third_degree_vertex] == ctx.run_id + BFS_DEPTH_THREE_OFFSET;
    if (is_new)
    {
        ctx.bfs_visited[third_degree_vertex] = ctx.run_id + BFS_DEPTH_THREE_OFFSET;
    }
    if (is_new || is_depth_two_no_back_edge || is_depth_three)
    {
        //m_logger.log(LogLevel::DEBUG, 
        //                "Found group of structure 1 2 3 vertices: " 
        //                + std::to_string(ctx.root) + ", " + std::to_string(first_degree_vertex) + ", "+ std::to_string(second_degree_vertex) + ", " + std::to_string(third_degree_vertex)); 

        ctx.count_group(compute_motif_descriptor(group, ctx.adjacency_matrix), group);
    }
}

void MotifPreprocessor::emit_depth_1_2_3_for_second_vertex(
    KavoshContext& ctx, uint32_t first_degree_vertex,
    uint32_t second_degree_vertex, const NeighbourRange& third_degree) const
{
    for (auto third_vertex = third_degree.begin; third_vertex != third_degree.end; ++third_vertex)
    {
        //m_logger.log(LogLevel::DEBUG, "Processing depth-3 vertex " + std::to_string(*third_vertex) + 
        //", depth-2 vertex " + std::to_string(second_degree_vertex) + ", depth-1 vertex " + std::to_string(first_degree_vertex) + " for depth-1 2 3 groups");
        if (ctx.ignore_vertices[*third_vertex])
        {
            continue;
        }
        emit_depth_1_2_3_for_third_vertex(ctx, first_degree_vertex, second_degree_vertex, *third_vertex);
    }
    if (m_graph.is_directed())
    {
        for (auto third_vertex = third_degree.rev_begin; third_vertex != third_degree.rev_end; ++third_vertex)
        {
            if (ctx.ignore_vertices[*third_vertex] || ctx.adjacency_matrix[second_degree_vertex][*third_vertex])
            {
                continue;
            }
            emit_depth_1_2_3_for_third_vertex(ctx, first_degree_vertex, second_degree_vertex, *third_vertex);
        }
    }
}

void MotifPreprocessor::emit_depth_1_2_3_for_first_vertex(
    KavoshContext& ctx, const uint32_t first_degree_vertex,
    const NeighbourRange& second_degree) const
{
    for (auto second_vertex = second_degree.begin; second_vertex != second_degree.end; ++second_vertex)
    {
        //m_logger.log(LogLevel::DEBUG, "Processing depth-2 vertex " + std::to_string(*second_vertex) + ", depth-1 vertex " + 
        //    std::to_string(first_degree_vertex) + " for depth-1 2 3 groups");
        if (ctx.ignore_vertices[*second_vertex] ||
            ctx.bfs_visited[*second_vertex] != ctx.run_id + BFS_DEPTH_TWO_OFFSET)
        {
            // if (ctx.ignore_vertices[*second_vertex])
            //     m_logger.log(LogLevel::DEBUG, "Skipping depth-2 vertex " + std::to_string(*second_vertex) + " for depth-1 2 3 groups because it is ignored.");
            // else
            //     m_logger.log(LogLevel::DEBUG, "Skipping depth-2 vertex " + std::to_string(*second_vertex) + " for depth-1 2 3 groups because it is not marked as depth 2.");
            continue;
        }
        const auto [three_fwd_begin, three_fwd_end] = m_graph.get_neighbours(*second_vertex);
        const auto [three_rev_begin, three_rev_end] = m_graph.is_directed()
            ? m_graph.get_neighbours(*second_vertex, true)
            : std::make_pair(three_fwd_end, three_fwd_end);
        emit_depth_1_2_3_for_second_vertex(
            ctx, first_degree_vertex, *second_vertex,
            NeighbourRange{three_fwd_begin, three_fwd_end, three_rev_begin, three_rev_end});
    }
    if (m_graph.is_directed())
    {
        for (auto second_vertex = second_degree.rev_begin; second_vertex != second_degree.rev_end; ++second_vertex)
        {
            if (ctx.ignore_vertices[*second_vertex] ||
                ctx.bfs_visited[*second_vertex] != ctx.run_id + BFS_DEPTH_TWO_OFFSET ||
                ctx.adjacency_matrix[first_degree_vertex][*second_vertex])
            {
                continue;
            }
            const auto [three_fwd_begin, three_fwd_end] = m_graph.get_neighbours(*second_vertex);
            const auto [three_rev_begin, three_rev_end] = m_graph.is_directed()
                ? m_graph.get_neighbours(*second_vertex, true)
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
    for (auto first_vertex = depth_one.begin; first_vertex != depth_one.end; ++first_vertex)
    {
        //m_logger.log(LogLevel::DEBUG, "Processing depth-1 vertex " + std::to_string(*first_vertex) + " for depth-1 2 3 groups");
        if (ctx.ignore_vertices[*first_vertex])
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
        for (auto first_vertex = depth_one.rev_begin; first_vertex != depth_one.rev_end; ++first_vertex)
        {
            if (ctx.ignore_vertices[*first_vertex] || ctx.adjacency_matrix[ctx.root][*first_vertex])
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
    }
}

void MotifPreprocessor::stream_groups_to_counter_for_vertex(
    const std::vector<std::vector<bool>>& graph_adjacency_matrix,
    const GroupCounterCallback& count_group,
    const std::vector<bool>& visited_vertices_to_ignore,
    std::vector<int64_t>& bfs_visited_vertices,
    const uint32_t root)
{
    // Pack root into upper bits; low 2 bits encode BFS depth. Stale entries from prior roots
    // are detected by checking the upper bits against the current root.
    const int64_t run_id = static_cast<int64_t>(root) << BFS_VERTEX_RUN_SHIFT;
    bfs_visited_vertices[root] = run_id;

    const auto [one_fwd_begin, one_fwd_end] = m_graph.get_neighbours(root);
    const auto [one_rev_begin, one_rev_end] = m_graph.is_directed()
        ? m_graph.get_neighbours(root, true)
        : std::make_pair(one_fwd_end, one_fwd_end);
    const NeighbourRange depth_one{one_fwd_begin, one_fwd_end, one_rev_begin, one_rev_end};

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
    //m_logger.log(LogLevel::DEBUG, "------------------ Calculating motif number for motif descriptor " + std::to_string(motif_descriptor) + " and node colors " + std::to_string(node_colors[0]) + ", " + std::to_string(node_colors[1]) + ", " + std::to_string(node_colors[2]) + ", " + std::to_string(node_colors[3]));
    __uint128_t minimal_colors = ~static_cast<__uint128_t>(0);
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
        // for (size_t color_index = 0; color_index < SgfConstants::MOTIF_SIZE; ++color_index)
        // {
        //     //__uint128_t shifted_color = static_cast<__uint128_t>(node_colors[color_permutation[color_index]]) << (color_index * SgfConstants::BITS_PER_COLOR);

        //     //std::ostringstream oss;
        //     //oss << "Color index: " << color_index
        //     //    << ", node color: " << node_colors[color_permutation[color_index]]
        //     //    << ", shifted color: "
        //     //    << std::setw(20) << std::setfill('0')
        //     //    << static_cast<uint64_t>(shifted_color >> 64)
        //     //    << std::setw(20) << std::setfill('0')
        //     //    << static_cast<uint64_t>(shifted_color);

        //     //m_logger.log(LogLevel::DEBUG, oss.str());            
        // }
        minimal_colors = std::min(minimal_colors, color_permutation_number);
    }
    // std::ostringstream oss;
    // oss << "Minimal motif number: " << minimal_motif_num
    //     << ", minimal colors: "
    //     << std::setw(20) << std::setfill('0')
    //     << static_cast<uint64_t>(minimal_colors >> 64);

    //m_logger.log(LogLevel::DEBUG, oss.str());
    //m_logger.log(LogLevel::DEBUG, "------------------------------------");
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
        // Directed: all pairs (both triangles). Undirected: upper triangle only (each pair once).
        size_t column_index = m_graph.is_directed() ? 0 : row_index + 1;
        for (; column_index < SgfConstants::MOTIF_SIZE; ++column_index)
        {
            if (row_index == column_index)
            {
                continue;
            }
            motif_descriptor <<= 1;
            motif_descriptor +=
                graph_adjacency_matrix[group[row_index]][group[column_index]] ? 1 : 0;
        }
    }
    return motif_descriptor;
}

}  // namespace sgf
