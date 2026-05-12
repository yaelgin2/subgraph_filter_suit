#include "EnumerationPreprocessManager.h"

#include "LogLevel.h"

#include <string>

namespace sgf
{

EnumerationPreprocessManager::EnumerationPreprocessManager(std::vector<ColoredGraph> library,
                                                           LoggerHandler logger)
    : m_library(std::move(library))
    , m_logger(std::move(logger))
{
}

std::vector<std::unordered_map<UInt128, uint32_t, UInt128Hash>>
EnumerationPreprocessManager::preprocess(const PreprocessorFactory& factory) const
{
    std::vector<std::unordered_map<UInt128, uint32_t, UInt128Hash>> results;
    results.reserve(m_library.size());
    for (size_t graph_index = 0U; graph_index < m_library.size(); ++graph_index)
    {
        const ColoredGraph& graph = m_library[graph_index];
        m_logger.log(LogLevel::INFO, "Processing graph " + std::to_string(graph_index));
        std::unique_ptr<IGraphPreprocessor> preprocessor = factory(graph, m_logger);
        results.push_back(preprocessor->calculate());
    }
    return results;
}

}  // namespace sgf
