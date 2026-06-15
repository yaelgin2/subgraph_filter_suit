#include "MultiGraphPatternPreprocessor.h"

#include "ColorRemapper.h"
#include "ColoredGraph.h"
#include "IPatternPreprocessor.h"
#include "LogLevel.h"
#include "LoggerHandler.h"
#include "MultiGraphPatternFinder.h"
#include "PatternUtils.h"

#include <atomic>
#include <cstdint>
#include <exception>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace sgf
{

// NOLINTNEXTLINE(readability-function-size)
MultiGraphPatternPreprocessor::MultiGraphPatternPreprocessor(
    std::vector<ColoredGraph>& graph_library, const bool is_directed, const uint32_t pattern_number,
    const double alive_precent, const uint32_t thread_number, LoggerHandler logger)
    : m_graph_library(graph_library)
    , M_IS_DIRECTED(is_directed)
    , M_PATTERN_NUMBER(pattern_number)
    , M_ALIVE_PRECENT(alive_precent)
    , m_thread_number(thread_number)
    , m_logger(std::move(logger))
{
}

// NOLINTNEXTLINE(readability-function-size)
std::vector<PatternPreprocessorResult> MultiGraphPatternPreprocessor::calculate()
{
    const uint32_t thread_count = std::min(m_thread_number, M_PATTERN_NUMBER);
    const std::vector<int32_t> color_map = ColorRemapper::map_colors(m_graph_library);
    const uint32_t color_count = static_cast<uint32_t>(color_map.size());

    std::atomic<uint32_t> patterns_generated{0U};
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
                    MultiGraphPatternFinder local_finder(m_graph_library, M_IS_DIRECTED,
                                                         color_count, m_logger);
                    uint32_t claimed =
                        patterns_generated.fetch_add(1U, std::memory_order_relaxed);
                    while (claimed < M_PATTERN_NUMBER)
                    {
                        m_logger.log(LogLevel::INFO,
                                     "Extracting pattern " + std::to_string(claimed + 1U) +
                                         "/" + std::to_string(M_PATTERN_NUMBER) + "...");
                        local_vec.push_back(local_finder.find_pattern(M_ALIVE_PRECENT, true));
                        PatternUtils::recolor_pattern(local_vec.back().first, color_map);
                        claimed = patterns_generated.fetch_add(1U, std::memory_order_relaxed);
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

    std::vector<PatternPreprocessorResult> all_results;
    for (std::vector<PatternPreprocessorResult>& local_vec : local_results)
    {
        for (PatternPreprocessorResult& result : local_vec)
        {
            all_results.push_back(std::move(result));
        }
    }
    ColorRemapper::restore_colors(m_graph_library, color_map);
    return all_results;
}

}  // namespace sgf
