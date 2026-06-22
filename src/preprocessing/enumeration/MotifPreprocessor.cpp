#include "MotifPreprocessor.h"

#include "ColoredGraph.h"
#include "Constants.h"
#include "GroupEnumerationPreprocessor.h"
#include "IGraphPreprocessor.h"
#include "Int128.h"
#include "LoggerHandler.h"
#include "MotifMap.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sgf
{


MotifPreprocessor::MotifPreprocessor(const ColoredGraph& graph, LoggerHandler logger,
                                     const uint32_t thread_number)
    : GroupEnumerationPreprocessor(graph, std::move(logger), thread_number)
{
}

void MotifPreprocessor::sort_nodes()
{
    GroupEnumerationPreprocessor::sort_nodes();
    m_order_index.assign(m_graph.vertex_count(), 0U);
    for (uint32_t idx = 0; idx < static_cast<uint32_t>(m_node_order.size()); ++idx)
    {
        m_order_index[m_node_order[idx]] = idx;
    }
}

// NOLINTNEXTLINE(readability-function-size)
EnumerationResult MotifPreprocessor::stream_groups_to_counter(
    const std::vector<std::vector<bool>>& graph_adjacency_matrix) const
{
    const uint32_t vertex_count = m_graph.vertex_count();
    const uint32_t order_size = static_cast<uint32_t>(m_node_order.size());
    const uint32_t thread_count = std::min(m_thread_number, order_size);

    std::atomic<uint32_t> next_idx{0U};
    std::vector<EnumerationResult> local_maps(thread_count);
    std::vector<std::exception_ptr> thread_exceptions(thread_count);
    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    for (uint32_t thread_idx = 0U; thread_idx < thread_count; ++thread_idx)
    {
        EnumerationResult& local_map = local_maps[thread_idx];
        std::exception_ptr& thread_exception = thread_exceptions[thread_idx];
        threads.emplace_back(
            [&]()
            {
                try
                {
                    std::vector<int64_t> bfs_visited(vertex_count, -1);
                    const GroupCounterCallback thread_count_fn =
                        [this, &local_map](const uint32_t desc, const std::vector<uint32_t>& group)
                    {
                        local_map[calculate_motif_number(desc, group_to_node_colors(group))] += 1U;
                    };
                    // fetch_add returns the old value, so the first thread gets idx=0
                    uint32_t idx = next_idx.fetch_add(1U, std::memory_order_relaxed);
                    while (idx < order_size)
                    {
                        stream_groups_to_counter_for_vertex(graph_adjacency_matrix, thread_count_fn,
                                                            bfs_visited, m_node_order[idx]);
                        idx = next_idx.fetch_add(1U, std::memory_order_relaxed);
                    }
                }
                catch (...)
                {
                    thread_exception = std::current_exception();
                }
            });
    }
    for (std::thread& thread : threads)
    {
        thread.join();
    }
    for (const std::exception_ptr& thread_exception : thread_exceptions)
    {
        if (thread_exception)
        {
            std::rethrow_exception(thread_exception);
        }
    }

    EnumerationResult merged;
    for (const EnumerationResult& local_map : local_maps)
    {
        for (const auto& [motif_id, count] : local_map)
        {
            merged[motif_id] += count;
        }
    }
    return merged;
}

void MotifPreprocessor::mark_depth_one_neighbours(CpuKavoshContext& ctx,
                                                  const CpuNeighbourRange& depth_one) const
{
    for (auto vertex = depth_one.m_begin; vertex != depth_one.m_end; ++vertex)
    {
        ctx.m_bfs_visited[*vertex] = ctx.m_run_id + 1;
    }
    if (m_graph.is_directed())
    {
        for (auto vertex = depth_one.m_rev_begin; vertex != depth_one.m_rev_end; ++vertex)
        {
            ctx.m_bfs_visited[*vertex] = ctx.m_run_id + 1;
        }
    }
}

void MotifPreprocessor::emit_depth_1_1_1_groups_cpu(const CpuKavoshContext& ctx,
                                                    const CpuNeighbourRange& depth_one) const
{
    for (auto first = depth_one.m_begin; first != depth_one.m_end; ++first)
    {
        if (m_order_index[*first] < m_order_index[ctx.m_root])
        {
            continue;
        }
        emit_depth_1_1_1_groups_first_vertex_chosen(ctx, depth_one, first, false);
    }
    if (m_graph.is_directed())
    {
        for (auto first = depth_one.m_rev_begin; first != depth_one.m_rev_end; ++first)
        {
            if (m_order_index[*first] < m_order_index[ctx.m_root] ||
                ctx.m_adjacency_matrix[ctx.m_root][*first])
            {
                continue;
            }
            emit_depth_1_1_1_groups_first_vertex_chosen(ctx, depth_one, first, true);
        }
    }
}

void MotifPreprocessor::mark_depth_two_neighbours(CpuKavoshContext& ctx,
                                                  const CpuNeighbourRange& depth_two) const
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


void MotifPreprocessor::emit_depth_1_1_2_and_1_2_2_groups_cpu(
    CpuKavoshContext& ctx, const CpuNeighbourRange& depth_one) const
{
    for (auto first = depth_one.m_begin; first != depth_one.m_end; ++first)
    {
        if (m_order_index[*first] < m_order_index[ctx.m_root])
        {
            continue;
        }
        process_first_neighbour_112_122(ctx, first, depth_one);
    }
    if (m_graph.is_directed())
    {
        for (auto first = depth_one.m_rev_begin; first != depth_one.m_rev_end; ++first)
        {
            if (m_order_index[*first] < m_order_index[ctx.m_root] ||
                ctx.m_adjacency_matrix[ctx.m_root][*first])
            {
                continue;
            }
            process_first_neighbour_112_122(ctx, first, depth_one);
        }
    }
}

void MotifPreprocessor::emit_depth_1_2_3_groups_cpu(CpuKavoshContext& ctx,
                                                    const CpuNeighbourRange& depth_one) const
{
    for (auto first_vertex = depth_one.m_begin; first_vertex != depth_one.m_end; ++first_vertex)
    {
        if (m_order_index[*first_vertex] < m_order_index[ctx.m_root])
        {
            continue;
        }
        const NeighbourIteratorPair sec_fwd = m_graph.get_neighbours(*first_vertex);
        const NeighbourIteratorPair sec_rev = m_graph.is_directed()
                                                  ? m_graph.get_neighbours(*first_vertex, true)
                                                  : std::make_pair(sec_fwd.second, sec_fwd.second);
        emit_depth_1_2_3_for_first_vertex(
            ctx, *first_vertex,
            CpuNeighbourRange{sec_fwd.first, sec_fwd.second, sec_rev.first, sec_rev.second});
    }
    if (m_graph.is_directed())
    {
        for (auto first_vertex = depth_one.m_rev_begin; first_vertex != depth_one.m_rev_end;
             ++first_vertex)
        {
            if (m_order_index[*first_vertex] < m_order_index[ctx.m_root] ||
                ctx.m_adjacency_matrix[ctx.m_root][*first_vertex])
            {
                continue;
            }
            const NeighbourIteratorPair sec_fwd = m_graph.get_neighbours(*first_vertex);
            const NeighbourIteratorPair sec_rev =
                m_graph.is_directed() ? m_graph.get_neighbours(*first_vertex, true)
                                      : std::make_pair(sec_fwd.second, sec_fwd.second);
            emit_depth_1_2_3_for_first_vertex(
                ctx, *first_vertex,
                CpuNeighbourRange{sec_fwd.first, sec_fwd.second, sec_rev.first, sec_rev.second});
        }
    }
}

void MotifPreprocessor::stream_groups_to_counter_for_vertex(
    const std::vector<std::vector<bool>>& graph_adjacency_matrix,
    const GroupCounterCallback& count_group, std::vector<int64_t>& bfs_visited_vertices,
    const uint32_t root) const
{
    // Pack root into upper bits; low 2 bits encode BFS depth. Stale entries from prior roots
    // are detected by checking the upper bits against the current root.
    const int64_t run_id =
        static_cast<int64_t>(static_cast<uint64_t>(root) << BFS_VERTEX_RUN_SHIFT);
    bfs_visited_vertices[root] = run_id;

    const NeighbourIteratorPair one_fwd = m_graph.get_neighbours(root);
    const NeighbourIteratorPair one_rev = m_graph.is_directed()
                                              ? m_graph.get_neighbours(root, true)
                                              : std::make_pair(one_fwd.second, one_fwd.second);
    const CpuNeighbourRange depth_one{one_fwd.first, one_fwd.second, one_rev.first, one_rev.second};

    CpuKavoshContext ctx{graph_adjacency_matrix, count_group, bfs_visited_vertices, run_id, root};
    mark_depth_one_neighbours(ctx, depth_one);
    emit_depth_1_1_1_groups_cpu(ctx, depth_one);
    emit_depth_1_1_2_and_1_2_2_groups_cpu(ctx, depth_one);
    emit_depth_1_2_3_groups_cpu(ctx, depth_one);
}

UInt128 MotifPreprocessor::calculate_motif_number(const uint32_t motif_descriptor,
                                                  const std::vector<uint32_t>& node_colors) const
{
    UInt128 minimal_colors = ~UInt128{};
    const std::unordered_map<uint32_t, MotifCanonical>& motif_map =
        m_graph.is_directed() ? DIRECTED_MOTIF_CANONICAL_MAP : UNDIRECTED_MOTIF_CANONICAL_MAP;
    const MotifCanonical motif_canonical = motif_map.at(motif_descriptor);
    const uint32_t minimal_motif_num = motif_canonical.m_minimal_motif_num;
    for (const auto& color_permutation : motif_canonical.m_color_permutations)
    {
        UInt128 color_permutation_number{};
        for (uint32_t color_index = 0; color_index < SgfConstants::MOTIF_SIZE; ++color_index)
        {
            color_permutation_number +=
                UInt128{node_colors.at(color_permutation.at(color_index))}
                << (color_index * static_cast<uint32_t>(SgfConstants::BITS_PER_COLOR));
        }
        minimal_colors = std::min(minimal_colors, color_permutation_number);
    }
    const UInt128 result =
        (UInt128{static_cast<uint64_t>(minimal_motif_num)}
         << static_cast<uint32_t>(SgfConstants::MOTIF_SIZE * SgfConstants::BITS_PER_COLOR)) |
        minimal_colors;
    return UInt128{result};
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
            motif_descriptor += static_cast<uint32_t>(
                graph_adjacency_matrix[group[row_index]][group[column_index]]);
        }
    }
    return motif_descriptor;
}

std::string MotifPreprocessor::entity_name() const
{
    return "motifs";
}

}  // namespace sgf
