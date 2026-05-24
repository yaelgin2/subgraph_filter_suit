#include "MultiGraphPatternPreprocessor.h"

#include "ColoredGraph.h"
#include "LoggerHandler.h"
#include "MultiGraphPatternFinder.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace sgf
{

MultiGraphPatternPreprocessor::MultiGraphPatternPreprocessor(
    std::vector<ColoredGraph>& graph_library, const bool is_directed, const uint32_t pattern_number,
    const uint32_t alive_precent, LoggerHandler logger)
    : m_graph_library(graph_library)
    , M_IS_DIRECTED(is_directed)
    , m_pattern_number(pattern_number)
    , m_alive_precent(alive_precent)
    , m_logger(std::move(logger))
{
}

std::vector<PatternPreprocessorResult> MultiGraphPatternPreprocessor::calculate()
{
    MultiGraphPatternFinder pattern_finder(m_graph_library, M_IS_DIRECTED, m_logger);
    std::vector<PatternPreprocessorResult> pattern_result(m_pattern_number);
    for (uint32_t pattern_index = 0U; pattern_index < m_pattern_number; ++pattern_index)
    {
        pattern_result[pattern_index] = pattern_finder.find_pattern(m_alive_precent, true);
    }
    return pattern_result;
}

}  // namespace sgf
