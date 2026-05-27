#include "PatternGraphFilter.h"

#include "ColoredGraph.h"
#include "FilteringUtils.h"
#include "LogLevel.h"
#include "LoggerHandler.h"
#include "PriorPolicy.h"
#include "SubgraphSearcher.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace sgf
{

PatternGraphFilter::PatternGraphFilter(std::vector<ColoredGraphPatternResult> library_cache,
                                       LoggerHandler logger)
    : m_library_cache(std::move(library_cache))
    , m_logger(std::move(logger))
{
}

}  // namespace sgf

namespace
{

bool is_check_relevant(const sgf::FilterResult& result,
                       const std::unordered_set<uint32_t>& indices_to_filter)
{
    return std::all_of(indices_to_filter.cbegin(), indices_to_filter.cend(),
                       [&result](const uint32_t index)
                       {
                           return result[index];
                       });
}

void log_and_mark_pruned(sgf::FilterResult& result, const uint32_t index,
                         const sgf::LoggerHandler& logger)
{
    if (!result[index])
    {
        logger.log(sgf::LogLevel::INFO, "Pruned library graph " + std::to_string(index));
    }
    result[index] = true;
}

void apply_prune(sgf::FilterResult& result, const std::unordered_set<uint32_t>& indices,
                 const sgf::LoggerHandler& logger)
{
    for (const uint32_t index : indices)
    {
        log_and_mark_pruned(result, index, logger);
    }
}

}  // namespace

namespace sgf
{

FilterResult PatternGraphFilter::filter(const ColoredGraph& graph, const bool is_induced,
                                        const PriorPolicy prior_policy) const
{
    FilterResult result(m_library_cache.size(), false);
    const SubgraphSearcher pattern_searcher(prior_policy, graph.is_directed(), is_induced, nullptr,
                                            m_logger);
    for (const auto& library_result : m_library_cache)
    {
        if (is_check_relevant(result, library_result.second))
        {
            if (pattern_searcher.find_all(graph, library_result.first, true) < 1U)
            {
                apply_prune(result, library_result.second, m_logger);
            }
        }
    }
    return result;
}

}  // namespace sgf
