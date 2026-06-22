#include "MotifPreprocessor.h"

#include "ColoredGraph.h"
#include "Constants.h"
#include "GroupEnumerationPreprocessor.h"
#include "Int128.h"
#include "LoggerHandler.h"
#include "MotifMap.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <exception>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sgf
{

namespace
{

using NeighbourIteratorPair =
    std::pair<std::vector<uint32_t>::const_iterator, std::vector<uint32_t>::const_iterator>;

}  // namespace

MotifPreprocessor::MotifPreprocessor(const ColoredGraph& graph, LoggerHandler logger,
                                     const uint32_t thread_number)
    : GroupEnumerationPreprocessor(graph, std::move(logger), thread_number)
{
}

void MotifPreprocessor::sort_nodes()
{
    GroupEnumerationPreprocessor::sort_nodes();
    m_order_index.assign(m_graph.vertex_count(), 0U);
    for (uint32_t idx = 0U; idx < static_cast<uint32_t>(m_node_order.size()); ++idx)
    {
        m_order_index[m_node_order[idx]] = idx;
    }
}

uint32_t MotifPreprocessor::build_canonical_array(
    const std::unordered_map<uint32_t, MotifCanonical>& canonical_map,
    std::vector<MotifCanonical>& out_array)
{
    uint32_t max_descriptor = 0U;
    for (const auto& [descriptor, entry] : canonical_map)
    {
        if (descriptor > max_descriptor)
        {
            max_descriptor = descriptor;
        }
    }
    const uint32_t array_size = max_descriptor + 1U;
    out_array.resize(array_size);
    for (const auto& [descriptor, entry] : canonical_map)
    {
        out_array[descriptor] = entry;
    }
    return array_size;
}

// NOLINTNEXTLINE(readability-function-size)
EnumerationResult MotifPreprocessor::stream_groups_to_counter() const
{
    const uint32_t vertex_count = m_graph.vertex_count();
    const uint32_t order_size   = static_cast<uint32_t>(m_node_order.size());
    const uint32_t thread_count = std::min(m_thread_number, order_size);

    // Build flat canonical array once — shared read-only across all threads.
    const std::unordered_map<uint32_t, MotifCanonical>& canonical_map =
        m_graph.is_directed() ? DIRECTED_MOTIF_CANONICAL_MAP : UNDIRECTED_MOTIF_CANONICAL_MAP;
    std::vector<MotifCanonical> canonical_array;
    const uint32_t canonical_size = build_canonical_array(canonical_map, canonical_array);
    const MotifCanonical* canonical_ptr = canonical_array.data();

    std::atomic<uint32_t> next_idx{0U};
    std::vector<EnumerationResult> local_maps(thread_count);
    std::vector<std::exception_ptr> thread_exceptions(thread_count);
    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    for (uint32_t thread_idx = 0U; thread_idx < thread_count; ++thread_idx)
    {
        EnumerationResult& local_map      = local_maps[thread_idx];
        std::exception_ptr& thread_exception = thread_exceptions[thread_idx];
        threads.emplace_back(
            [&]()
            {
                try
                {
                    std::vector<int64_t> bfs_visited(vertex_count, -1);
                    const GroupCounterCallback thread_count_fn =
                        [canonical_ptr, canonical_size, &local_map, this](
                            const uint32_t desc, const std::vector<uint32_t>& group)
                    {
                        uint32_t node_colors[SgfConstants::MOTIF_SIZE];
                        for (uint32_t ci = 0U; ci < SgfConstants::MOTIF_SIZE; ++ci)
                        {
                            node_colors[ci] = m_graph.get_vertex_color(group[ci]);
                        }
                        local_map[calculate_motif_number_from_arrays(
                            desc, node_colors, canonical_ptr, canonical_size)] += 1U;
                    };
                    uint32_t idx = next_idx.fetch_add(1U, std::memory_order_relaxed);
                    while (idx < order_size)
                    {
                        stream_groups_to_counter_for_vertex(thread_count_fn, bfs_visited,
                                                            canonical_ptr, canonical_size,
                                                            m_node_order[idx]);
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
                ctx.has_fwd_edge(ctx.m_root, *first))
            {
                continue;
            }
            emit_depth_1_1_1_groups_first_vertex_chosen(ctx, depth_one, first, true);
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
                ctx.has_fwd_edge(ctx.m_root, *first))
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
        const NeighbourIteratorPair sec_rev =
            m_graph.is_directed() ? m_graph.get_neighbours(*first_vertex, true)
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
                ctx.has_fwd_edge(ctx.m_root, *first_vertex))
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
    const GroupCounterCallback& count_group,
    std::vector<int64_t>& bfs_visited_vertices,
    const MotifCanonical* const canonical,
    const uint32_t canonical_size,
    const uint32_t root) const
{
    const int64_t run_id =
        static_cast<int64_t>(static_cast<uint64_t>(root) << BFS_VERTEX_RUN_SHIFT);
    bfs_visited_vertices[root] = run_id;

    const NeighbourIteratorPair one_fwd = m_graph.get_neighbours(root);
    const NeighbourIteratorPair one_rev = m_graph.is_directed()
                                              ? m_graph.get_neighbours(root, true)
                                              : std::make_pair(one_fwd.second, one_fwd.second);
    const CpuNeighbourRange depth_one{one_fwd.first, one_fwd.second, one_rev.first, one_rev.second};

    CpuKavoshContext ctx{m_graph, count_group, bfs_visited_vertices, run_id, root,
                         canonical, canonical_size, m_order_index};
    ctx.mark_neighbours(depth_one, BFS_DEPTH_ONE_OFFSET);
    emit_depth_1_1_1_groups_cpu(ctx, depth_one);
    emit_depth_1_1_2_and_1_2_2_groups_cpu(ctx, depth_one);
    emit_depth_1_2_3_groups_cpu(ctx, depth_one);
}

// NOLINTNEXTLINE(readability-function-size)
SGF_HD UInt128 MotifPreprocessor::calculate_motif_number_from_arrays(
    const uint32_t descriptor,
    const uint32_t* const node_colors,
    const MotifCanonical* const canonical_array,
    const uint32_t canonical_size) noexcept
{
    if (descriptor >= canonical_size || canonical_array[descriptor].m_permutation_count == 0U)
    {
        return UInt128{};
    }
    const MotifCanonical& canonical = canonical_array[descriptor];
    UInt128 minimal_colors = ~UInt128{};
    for (uint32_t perm = 0U; perm < canonical.m_permutation_count; ++perm)
    {
        UInt128 encoded{};
        for (uint32_t ci = 0U; ci < SgfConstants::MOTIF_SIZE; ++ci)
        {
            encoded += UInt128{node_colors[canonical.m_color_permutations[perm][ci]]}
                       << (ci * static_cast<uint32_t>(SgfConstants::BITS_PER_COLOR));
        }
        if (encoded < minimal_colors)
        {
            minimal_colors = encoded;
        }
    }
    return (UInt128{static_cast<uint64_t>(canonical.m_minimal_motif_num)}
            << static_cast<uint32_t>(SgfConstants::MOTIF_SIZE * SgfConstants::BITS_PER_COLOR)) |
           minimal_colors;
}

UInt128 MotifPreprocessor::calculate_motif_number(const uint32_t motif_descriptor,
                                                   const std::vector<uint32_t>& node_colors) const
{
    // Build a one-shot flat array from the unordered_map for the single lookup.
    // In normal usage this path is not called — stream_groups_to_counter uses
    // the pre-built flat array via calculate_motif_number_from_arrays directly.
    const std::unordered_map<uint32_t, MotifCanonical>& canonical_map =
        m_graph.is_directed() ? DIRECTED_MOTIF_CANONICAL_MAP : UNDIRECTED_MOTIF_CANONICAL_MAP;
    std::vector<MotifCanonical> canonical_array;
    const uint32_t canonical_size = build_canonical_array(canonical_map, canonical_array);
    uint32_t colors[SgfConstants::MOTIF_SIZE];
    for (uint32_t ci = 0U; ci < SgfConstants::MOTIF_SIZE; ++ci)
    {
        colors[ci] = node_colors[ci];
    }
    return calculate_motif_number_from_arrays(motif_descriptor, colors,
                                              canonical_array.data(), canonical_size);
}

std::string MotifPreprocessor::entity_name() const
{
    return "motifs";
}

}  // namespace sgf
