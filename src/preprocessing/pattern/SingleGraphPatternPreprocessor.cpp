#include "SingleGraphPatternPreprocessor.h"

#include "BoostGraph.h"
#include "ColorRemapper.h"
#include "ColoredGraph.h"
#include "IPatternPreprocessor.h"
#include "LoggerHandler.h"
#include "PatternUtils.h"
#include "SingleGraphPatternFinder.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <exception>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

namespace sgf
{

SingleGraphPatternPreprocessor::SingleGraphPatternPreprocessor(
    std::vector<ColoredGraph>& graph_library, const bool is_directed, ColoredGraph background_graph,
    const std::vector<bool>& to_process, const double score_threshold,
    const SingleGraphFinderConfig config, LoggerHandler logger)
    : m_graph_library(graph_library)
    , M_IS_DIRECTED(is_directed)
    , m_background_graph(std::move(background_graph))
    , M_SCORE_THRESHOLD(score_threshold)
    , m_to_process(to_process)
    , M_CONFIG(config)
    , m_logger(std::move(logger))
{
}

// NOLINTNEXTLINE(readability-function-size)
std::vector<PatternPreprocessorResult> SingleGraphPatternPreprocessor::calculate()
{
    const std::vector<int32_t> color_map =
        ColorRemapper::map_colors(m_background_graph, m_graph_library);
    const uint32_t color_count = static_cast<uint32_t>(color_map.size());
    const uint32_t graph_count = static_cast<uint32_t>(m_graph_library.size());
    const uint32_t graphs_to_process =
        static_cast<uint32_t>(std::count(m_to_process.begin(), m_to_process.end(), true));
    if (graphs_to_process == 0U)
    {
        ColorRemapper::restore_colors(m_background_graph, m_graph_library, color_map);
        return {};
    }
    const uint32_t thread_count = std::min(M_CONFIG.m_thread_number, graphs_to_process);

    std::atomic<uint32_t> next_graph_index{0U};
    std::vector<std::vector<PatternPreprocessorResult>> local_results(thread_count);
    std::vector<std::exception_ptr> thread_exceptions(thread_count);
    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    for (uint32_t thread_idx = 0U; thread_idx < thread_count; ++thread_idx)
    {
        std::vector<PatternPreprocessorResult>& local_vec = local_results[thread_idx];
        std::exception_ptr& thread_exception = thread_exceptions[thread_idx];
        threads.emplace_back(
            [&]()
            {
                try
                {
                    SingleGraphPatternFinder finder(m_background_graph, color_count, M_IS_DIRECTED,
                                                    M_CONFIG.m_max_active_patterns,
                                                    M_CONFIG.m_alpha_0, M_CONFIG.m_alpha_decay,
                                                    m_logger);
                    uint32_t graph_idx = next_graph_index.fetch_add(1U, std::memory_order_relaxed);
                    while (graph_idx < graph_count)
                    {
                        if (m_to_process[graph_idx])
                        {
                            std::vector<BoostGraph> patterns =
                                finder.find_pattern(m_graph_library[graph_idx], M_SCORE_THRESHOLD);
                            for (BoostGraph& pattern : patterns)
                            {
                                PatternUtils::recolor_pattern(pattern, color_map);
                                local_vec.emplace_back(std::move(pattern),
                                                       std::unordered_set<uint32_t>{graph_idx});
                            }
                        }
                        graph_idx = next_graph_index.fetch_add(1U, std::memory_order_relaxed);
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

    std::vector<PatternPreprocessorResult> all_results;
    for (std::vector<PatternPreprocessorResult>& local_vec : local_results)
    {
        for (PatternPreprocessorResult& result : local_vec)
        {
            all_results.push_back(std::move(result));
        }
    }
    ColorRemapper::restore_colors(m_background_graph, m_graph_library, color_map);
    return all_results;
}

}  // namespace sgf
