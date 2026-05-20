#include "MultiGraphPatternPreprocessor.h"

#include "MultiGraphPatternFinder.h"

#include <cstdint>
#include <vector>

namespace sgf
{

MultiGraphPatternPreprocessor::MultiGraphPatternPreprocessor(
    std::vector<ColoredGraph>& graph_library, const bool is_directed, LoggerHandler logger)
    : m_graph_library(graph_library)
    , m_is_directed(is_directed)
    , m_logger(std::move(logger))
{
}

std::vector<MultiGraphPatternResult> MultiGraphPatternPreprocessor::calculate(
    const uint32_t pattern_number, const uint32_t alive_precent)
{
    MultiGraphPatternFinder pattern_finder(m_graph_library, m_is_directed, m_logger);
    std::vector<MultiGraphPatternResult> pattern_result;
    for (uint32_t pattern_index = 0U; pattern_index < pattern_number; ++pattern_index)
    {
        pattern_result.push_back(pattern_finder.find_pattern(alive_precent, true));
    }
    return pattern_result;
}

}  // namespace sgf
