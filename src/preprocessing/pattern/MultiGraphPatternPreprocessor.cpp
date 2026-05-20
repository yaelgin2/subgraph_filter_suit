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
    std::vector<ColoredGraph>& graph_library, const bool is_directed, LoggerHandler logger)
    : m_graph_library(graph_library)
    , M_IS_DIRECTED(is_directed)
    , m_logger(std::move(logger))
{
}

std::vector<MultiGraphPatternResult>
MultiGraphPatternPreprocessor::calculate(const uint32_t pattern_number,
                                         const uint32_t alive_precent)
{
    MultiGraphPatternFinder pattern_finder(m_graph_library, M_IS_DIRECTED, m_logger);
    std::vector<MultiGraphPatternResult> pattern_result(pattern_number);
    for (uint32_t pattern_index = 0U; pattern_index < pattern_number; ++pattern_index)
    {
        pattern_result[pattern_index] = pattern_finder.find_pattern(alive_precent, true);
    }
    return pattern_result;
}

}  // namespace sgf
