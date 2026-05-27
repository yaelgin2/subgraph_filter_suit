#include "PatternGraphFilter.h"

#include "SubgraphSearcher.h"
#include "PriorPolicy.h"

namespace sgf
{

PatternGraphFilter::PatternGraphFilter(std::vector<ColoredGraphPatternResult> library_cache,
                                       LoggerHandler logger)
    : m_library_cache(std::move(library_cache))
    , m_logger(std::move(logger))
{
}

bool is_check_relevant(const FilterResult& result, const std::unordered_set<uint32_t>& indices_to_filter)
{
    return std::all_of(indices_to_filter.cbegin(), indices_to_filter.cend(),
                       [&result](const uint32_t index) { return result[index]; });
}

FilterResult PatternGraphFilter::filter(const ColoredGraph& graph, bool is_induced, PriorPolicy prior_policy) const
{
    FilterResult result(m_library_cache.size(), false);
    SubgraphSearcher pattern_searcher(prior_policy, graph.is_directed(), is_induced, nullptr, m_logger);
    for (size_t library_index = 0; library_index < m_library_cache.size(); ++library_index)
    {   
        const ColoredGraphPatternResult& library_result = m_library_cache[library_index];
        if (is_check_relevant(result, library_result.second))
            {
                if(pattern_searcher.find_all(graph, library_result.first, true) < 1)
                {
                    for (const uint32_t index : m_library_cache[library_index].second)
                    {
                        if (!result[index])
                        {
                            m_logger.log(LogLevel::INFO,
                                 "Pruned library graph " + std::to_string(index));
                        }
                        result[index] = true;
                    }
                }
            }
    }
    return result;
}

}  // namespace sgf
