#include "PathProcessor.h"

#include "ColoredGraph.h"
#include "GroupEnumerationPreprocessor.h"
#include "Int128.h"
#include "LoggerHandler.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <exception>
#include <iterator>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace sgf
{

PathProcessor::PathProcessor(const ColoredGraph& graph, LoggerHandler logger,
                             const uint32_t thread_number)
    : GroupEnumerationPreprocessor(graph, std::move(logger), thread_number)
{
}

// NOLINTNEXTLINE(readability-function-size)
EnumerationResult PathProcessor::stream_groups_to_counter(
    [[maybe_unused]] const std::vector<std::vector<bool>>& graph_adjacency_matrix) const
{
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
                    const GroupCounterCallback count_group =
                        [this, &local_map](const uint32_t desc, const std::vector<uint32_t>& group)
                    {
                        local_map[calculate_motif_number(desc, group_to_node_colors(group))] += 1U;
                    };
                    uint32_t idx = next_idx.fetch_add(1U, std::memory_order_relaxed);
                    while (idx < order_size)
                    {
                        stream_groups_to_counter_for_vertex(count_group, m_node_order[idx]);
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
    for (const std::exception_ptr& ex : thread_exceptions)
    {
        if (ex)
        {
            std::rethrow_exception(ex);
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

UInt128 PathProcessor::calculate_motif_number(const uint32_t motif_descriptor,
                                              const std::vector<uint32_t>& node_colors) const
{
    UInt128 forward_color_sequence{};
    for (const uint32_t vertex_color : node_colors)
    {
        forward_color_sequence <<= COLOR_BITS_PER_SLOT;
        forward_color_sequence |= vertex_color;
    }
    UInt128 reversed_color_sequence{};
    for (std::vector<uint32_t>::const_reverse_iterator it = node_colors.crbegin();
         it != node_colors.crend(); ++it)
    {
        reversed_color_sequence <<= COLOR_BITS_PER_SLOT;
        reversed_color_sequence |= *it;
    }
    if (!m_graph.is_directed())
    {
        return UInt128{std::min(forward_color_sequence, reversed_color_sequence)};
    }
    UInt128 motif_number{};
    if (forward_color_sequence <= reversed_color_sequence)
    {
        motif_number = forward_color_sequence;
        motif_number |= UInt128{static_cast<uint64_t>(motif_descriptor)}
                        << (PATH_VERTEX_COUNT * COLOR_BITS_PER_SLOT);
    }
    else
    {
        motif_number = reversed_color_sequence;
        motif_number |=
            UInt128{static_cast<uint64_t>(compute_reversed_descriptor(motif_descriptor))}
            << (PATH_VERTEX_COUNT * COLOR_BITS_PER_SLOT);
    }
    return UInt128{motif_number};
}

uint32_t PathProcessor::compute_reversed_descriptor(const uint32_t motif_descriptor)
{
    uint32_t reversed = 0U;
    for (uint32_t bit_index = 0U; bit_index < PATH_EDGE_COUNT; ++bit_index)
    {
        reversed |= (((motif_descriptor >> bit_index) & 1U) ^ 1U)
                    << (PATH_EDGE_COUNT - 1U - bit_index);
    }
    return reversed;
}

void PathProcessor::stream_groups_for_out_neighbours(const GroupCounterCallback& count_group,
                                                     const uint32_t root,
                                                     const NeighbourRange depth_one_out,
                                                     const NeighbourRange depth_one_in) const
{
    for (std::vector<uint32_t>::const_iterator first_neighbour_it = depth_one_out.m_begin;
         first_neighbour_it != depth_one_out.m_end; ++first_neighbour_it)
    {
        for (std::vector<uint32_t>::const_iterator second_neighbour_it = first_neighbour_it + 1;
             second_neighbour_it != depth_one_out.m_end; ++second_neighbour_it)
        {
            stream_groups_to_counter_for_two_depth_one_neighbours(
                count_group, root, first_neighbour_it, second_neighbour_it);
        }
        if (m_graph.is_directed())
        {
            for (std::vector<uint32_t>::const_iterator second_neighbour_it = depth_one_in.m_begin;
                 second_neighbour_it != depth_one_in.m_end; ++second_neighbour_it)
            {
                stream_groups_to_counter_for_two_depth_one_neighbours(
                    count_group, root, first_neighbour_it, second_neighbour_it);
            }
        }
    }
}

void PathProcessor::stream_groups_for_in_neighbours(
    const GroupCounterCallback& count_group, const uint32_t root,
    std::vector<uint32_t>::const_iterator depth_one_in_start,
    std::vector<uint32_t>::const_iterator depth_one_in_end) const
{
    for (std::vector<uint32_t>::const_iterator first_neighbour_it = depth_one_in_start;
         first_neighbour_it != depth_one_in_end; ++first_neighbour_it)
    {
        for (std::vector<uint32_t>::const_iterator second_neighbour_it = first_neighbour_it + 1;
             second_neighbour_it != depth_one_in_end; ++second_neighbour_it)
        {
            stream_groups_to_counter_for_two_depth_one_neighbours(
                count_group, root, first_neighbour_it, second_neighbour_it);
        }
    }
}

void PathProcessor::stream_groups_to_counter_for_vertex(const GroupCounterCallback& count_group,
                                                        const uint32_t root) const
{
    const NeighbourIteratorPair out_range = m_graph.get_neighbours(root, false);
    const NeighbourIteratorPair in_range = m_graph.get_neighbours(root, true);

    stream_groups_for_out_neighbours(count_group, root, {out_range.first, out_range.second},
                                     {in_range.first, in_range.second});

    if (m_graph.is_directed())
    {
        stream_groups_for_in_neighbours(count_group, root, in_range.first, in_range.second);
    }
}

void PathProcessor::stream_groups_to_counter_for_two_depth_one_neighbours(
    const GroupCounterCallback& count_group, const uint32_t root,
    std::vector<uint32_t>::const_iterator first_depth_one_neighbour,
    std::vector<uint32_t>::const_iterator second_depth_one_neighbour) const
{
    stream_groups_to_counter_for_two_depth_one_neighbours(
        count_group, root, DepthOneNeighbourInfo{first_depth_one_neighbour, false},
        DepthOneNeighbourInfo{second_depth_one_neighbour, false});
    if (m_graph.is_directed())
    {
        stream_groups_to_counter_for_two_depth_one_neighbours(
            count_group, root, DepthOneNeighbourInfo{first_depth_one_neighbour, false},
            DepthOneNeighbourInfo{second_depth_one_neighbour, true});
        stream_groups_to_counter_for_two_depth_one_neighbours(
            count_group, root, DepthOneNeighbourInfo{first_depth_one_neighbour, true},
            DepthOneNeighbourInfo{second_depth_one_neighbour, false});
        stream_groups_to_counter_for_two_depth_one_neighbours(
            count_group, root, DepthOneNeighbourInfo{first_depth_one_neighbour, true},
            DepthOneNeighbourInfo{second_depth_one_neighbour, true});
    }
}

void PathProcessor::stream_groups_to_counter_for_two_depth_one_neighbours(
    const GroupCounterCallback& count_group, const uint32_t root,
    const DepthOneNeighbourInfo& first_neighbour_info,
    const DepthOneNeighbourInfo& second_neighbour_info) const
{
    const NeighbourIteratorPair depth_two_one =
        m_graph.get_neighbours(*first_neighbour_info.m_iterator, first_neighbour_info.m_direction);
    const NeighbourIteratorPair depth_two_two = m_graph.get_neighbours(
        *second_neighbour_info.m_iterator, second_neighbour_info.m_direction);

    for (std::vector<uint32_t>::const_iterator first_depth_two = depth_two_one.first;
         first_depth_two != depth_two_one.second; ++first_depth_two)
    {
        if (*first_depth_two == root)
        {
            continue;
        }
        for (std::vector<uint32_t>::const_iterator second_depth_two = depth_two_two.first;
             second_depth_two != depth_two_two.second; ++second_depth_two)
        {
            if (*second_depth_two == root)
            {
                continue;
            }
            const PathInformation full_first_path = {
                {*first_neighbour_info.m_iterator, first_neighbour_info.m_direction},
                {*first_depth_two, first_neighbour_info.m_direction}};
            const PathInformation full_second_path = {
                {*second_neighbour_info.m_iterator, second_neighbour_info.m_direction},
                {*second_depth_two, second_neighbour_info.m_direction}};

            if (!check_path_intersection(full_first_path, full_second_path))
            {
                const std::vector<uint32_t> path =
                    concatenate_path(root, full_first_path, full_second_path);
                count_group(compute_motif_descriptor(full_first_path, full_second_path), path);
            }
        }
    }
}

bool PathProcessor::check_path_intersection(const PathInformation& first_path,
                                            const PathInformation& second_path)
{
    return first_path[0].first == second_path[1].first ||
           first_path[1].first == second_path[0].first ||
           first_path[1].first == second_path[1].first;
}

std::vector<uint32_t> PathProcessor::concatenate_path(const uint32_t root,
                                                      const PathInformation& first_path,
                                                      const PathInformation& second_path)
{
    std::vector<uint32_t> path;
    path.reserve(PATH_VERTEX_COUNT);
    path.push_back(first_path[1].first);
    path.push_back(first_path[0].first);
    path.push_back(root);
    path.push_back(second_path[0].first);
    path.push_back(second_path[1].first);
    return path;
}

uint32_t PathProcessor::compute_motif_descriptor(const PathInformation& first_path,
                                                 const PathInformation& second_path) const
{
    uint32_t motif_descriptor = 0U;
    if (m_graph.is_directed())
    {
        for (const std::pair<uint32_t, bool>& node : first_path)
        {
            motif_descriptor <<= 1U;
            motif_descriptor |= node.second ? 1U : 0U;
        }
        for (const std::pair<uint32_t, bool>& node : second_path)
        {
            motif_descriptor <<= 1U;
            motif_descriptor |= node.second ? 0U : 1U;
        }
    }
    return motif_descriptor;
}

std::string PathProcessor::entity_name() const
{
    return "paths";
}

}  // namespace sgf
