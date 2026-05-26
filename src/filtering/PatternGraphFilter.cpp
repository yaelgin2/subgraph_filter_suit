#include "PatternGraphFilter.h"

namespace sgf
{

PatternGraphFilter::PatternGraphFilter(std::vector<PatternPreprocessorResult> library_cache,
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
    for (size_t library_index = 0; library_index < m_library_cache.size(); ++library_index)
    {
        
        const PatternPreprocessorResult& library_result = m_library_cache[library_index];
        SubgraphSearcher pattern_searcher(prior_policy, graph.is_directed(), is_induced);
        for (const auto& [motif_descriptor, query_count] : query_result.m_motif_counts)
        {
            if (is_check_relevant(result, m_library_cache[library_index].second))
            {
                if(pattern_searcher.find_all(graph, m_library_cache[library_index].first) < query_count)
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
    }
    return result;

}  // namespace sgf
