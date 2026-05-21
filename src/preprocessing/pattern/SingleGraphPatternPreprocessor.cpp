#include "SingleGraphPatternPreprocessor.h"

#include "BoostGraph.h"
#include "ColoredGraph.h"
#include "LoggerHandler.h"
#include "SingleGraphPatternFinder.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace sgf
{

SingleGraphPatternPreprocessor::SingleGraphPatternPreprocessor(
    std::vector<ColoredGraph>& graph_library, const bool is_directed, LoggerHandler logger)
    : m_graph_library(graph_library)
    , M_IS_DIRECTED(is_directed)
    , m_logger(std::move(logger))
{
}

std::vector<SingleGraphPatternResult> SingleGraphPatternPreprocessor::calculate(
    ColoredGraph background_graph, const uint32_t pattern_number, const double score_threshold,
    const SingleGraphFinderConfig config)
{
    SingleGraphPatternFinder pattern_finder(std::move(background_graph), M_IS_DIRECTED,
                                            config.m_max_active_patterns, config.m_alpha_0,
                                            config.m_alpha_decay, m_logger);
    std::vector<SingleGraphPatternResult> result;
    result.reserve(pattern_number);
    for (uint32_t pattern_index = 0U; pattern_index < pattern_number; ++pattern_index)
    {
        ColoredGraph search_graph = m_graph_library[pattern_index];
        const std::vector<BoostGraph> patterns =
            pattern_finder.find_pattern(search_graph, score_threshold);
        for (const BoostGraph& pattern : patterns)
        {
            result.emplace_back(pattern, std::vector<uint32_t>{pattern_index});
        }
    }
    return result;
}

}  // namespace sgf
