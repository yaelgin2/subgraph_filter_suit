#include "PathProcessor.h"

#include "ColoredGraph.h"
#include "CpuNeighbourRange.h"
#include "CpuPathContext.h"
#include "GroupEnumerationPreprocessor.h"
#include "IGraphPreprocessor.h"
#include "Int128.h"
#include "LoggerHandler.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <exception>
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

void PathProcessor::cpu_add_path_to_count(CpuPathContext& ctx, const UInt128 motif_id) noexcept
{
    ctx.m_result[motif_id] += 1U;
}

// NOLINTNEXTLINE(readability-function-size)
EnumerationResult PathProcessor::stream_groups_to_counter() const
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
                    uint32_t idx = next_idx.fetch_add(1U, std::memory_order_relaxed);
                    while (idx < order_size)
                    {
                        CpuPathContext ctx{m_node_order[idx], m_graph, local_map,
                                           cpu_add_path_to_count};
                        stream_groups_to_counter_for_vertex<CpuPathContext, CpuNeighbourRange,
                                                            std::vector<uint32_t>::const_iterator>(
                            ctx);
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
    for (const std::exception_ptr& pending_exception : thread_exceptions)
    {
        if (pending_exception)
        {
            std::rethrow_exception(pending_exception);
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

std::string PathProcessor::entity_name() const
{
    return "paths";
}

}  // namespace sgf
