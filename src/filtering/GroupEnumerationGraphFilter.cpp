#include "GroupEnumerationGraphFilter.h"

#include "EnumerationPreprocessManager.h"
#include "FilteringUtils.h"
#include "IGraphPreprocessor.h"
#include "LogLevel.h"
#include "LoggerHandler.h"

#include <cstdint>
#include <string>
#include <utility>

namespace sgf
{

GroupEnumerationGraphFilter::GroupEnumerationGraphFilter(EnumerationResultVector library_cache,
                                                         LoggerHandler logger)
    : m_library_cache(std::move(library_cache)), m_logger(std::move(logger))
{
}

FilterResult GroupEnumerationGraphFilter::filter(const EnumerationResult& graph_features) const
{
    FilterResult can_graph_be_filtered(m_library_cache.size(), false);
    for (uint32_t library_graph_index = 0; library_graph_index < m_library_cache.size();
         ++library_graph_index)
    {
        m_logger.log(LogLevel::INFO, "Filtering against library graph " + std::to_string(library_graph_index));
        for (auto [motif_key, motif_appearences] : m_library_cache[library_graph_index])
        {
            const EnumerationResult::const_iterator graph_feature_appearences_iter =
                graph_features.find(motif_key);
            const uint32_t graph_feature_appearences =
                (graph_feature_appearences_iter != graph_features.end())
                    ? graph_feature_appearences_iter->second
                    : 0U;
            if (graph_feature_appearences < motif_appearences)
            {
                can_graph_be_filtered[library_graph_index] = true;
                break;
            }
        }
        m_logger.log(LogLevel::INFO, "Library graph " + std::to_string(library_graph_index) +
                                     (can_graph_be_filtered[library_graph_index] ? " can be filtered." : " survives."));
    }
    return can_graph_be_filtered;
}

}  // namespace sgf
