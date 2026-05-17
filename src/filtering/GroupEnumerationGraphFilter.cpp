#include "GroupEnumerationGraphFilter.h"

#include "Int128.h"

namespace sgf
{

GroupEnumerationGraphFilter::GroupEnumerationGraphFilter(EnumerationData library_cache)
    : m_library_cache(std::move(library_cache))
{
}

FilterResult GroupEnumerationGraphFilter::filter(const EnumerationResult& graph_features) const
{
    FilterResult can_graph_be_filtered(m_library_cache.size(), false);
    for (uint32_t library_graph_index = 0; library_graph_index < m_library_cache.size();
         ++library_graph_index)
    {
        for (auto [motif_key, motif_appearences] : m_library_cache[library_graph_index])
        {
            auto graph_feature_appearences_iter = graph_features.find(motif_key);
            uint32_t graph_feature_appearences =
                (graph_feature_appearences_iter != graph_features.end())
                    ? graph_feature_appearences_iter->second
                    : 0;
            if (graph_feature_appearences < motif_appearences)
            {
                can_graph_be_filtered[library_graph_index] = true;
                break;
            }
        }
    }
    return can_graph_be_filtered;
}

}  // namespace sgf
