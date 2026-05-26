#include "SingleGraphPatternPreprocessor.h"

#include "BoostGraph.h"
#include "ColoredGraph.h"
#include "IPatternPreprocessor.h"
#include "LoggerHandler.h"
#include "SingleGraphPatternFinder.h"

#include <cstdint>
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

std::vector<PatternPreprocessorResult> SingleGraphPatternPreprocessor::calculate()
{
    SingleGraphPatternFinder pattern_finder(m_background_graph, M_IS_DIRECTED,
                                            M_CONFIG.m_max_active_patterns, M_CONFIG.m_alpha_0,
                                            M_CONFIG.m_alpha_decay, m_logger);
    std::vector<PatternPreprocessorResult> result;
    result.reserve(m_graph_library.size());
    for (uint32_t pattern_index = 0U; pattern_index < m_graph_library.size(); ++pattern_index)
    {
        if (!m_to_process[pattern_index])
        {
            continue;
        }
        ColoredGraph search_graph = m_graph_library[pattern_index];
        const std::vector<BoostGraph> patterns =
            pattern_finder.find_pattern(search_graph, M_SCORE_THRESHOLD);
        for (const BoostGraph& pattern : patterns)
        {
            result.emplace_back(pattern, std::unordered_set<uint32_t>{pattern_index});
        }
    }
    return result;
}

}  // namespace sgf
