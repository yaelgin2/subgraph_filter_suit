#include "MultiGraphPatternPreprocessor.h"

#include "ColoredGraph.h"
#include "IPatternPreprocessor.h"
#include "LogLevel.h"
#include "LoggerHandler.h"
#include "MultiGraphPatternFinder.h"

#include <cstdint>
#include <string>
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

std::vector<PatternPreprocessorResult> MultiGraphPatternPreprocessor::calculate()
{
    MultiGraphPatternFinder pattern_finder(m_graph_library, M_IS_DIRECTED, m_logger);
    std::vector<PatternPreprocessorResult> pattern_result(M_PATTERN_NUMBER);
    for (uint32_t pattern_index = 0U; pattern_index < M_PATTERN_NUMBER; ++pattern_index)
    {
        m_logger.log(LogLevel::INFO, "Extracting pattern " + std::to_string(pattern_index + 1U) +
                                         "/" + std::to_string(M_PATTERN_NUMBER) + "...");
        pattern_result[pattern_index] = pattern_finder.find_pattern(M_ALIVE_PRECENT, true);
    }
    return pattern_result;
}

}  // namespace sgf
