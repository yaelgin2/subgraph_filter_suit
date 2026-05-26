#include "PatternGraphFilter.h"

namespace sgf
{

PatternGraphFilter::PatternGraphFilter(std::vector<PatternPreprocessorResult> library_cache,
                                       LoggerHandler logger)
    : m_library_cache(std::move(library_cache))
    , m_logger(std::move(logger))
{
}

FilterResult PatternGraphFilter::filter(const ColoredGraph& graph) const
{
    // TODO
}

}  // namespace sgf
