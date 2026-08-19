#include "EnumerationPreprocessManager.h"

#include "ColoredGraph.h"
#include "IGraphPreprocessor.h"
#include "LogLevel.h"
#include "LoggerHandler.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace sgf
{

EnumerationPreprocessManager::EnumerationPreprocessManager(std::vector<ColoredGraph> library,
                                                           LoggerHandler logger)
    : m_library(std::move(library))
    , m_logger(std::move(logger))
{
}

EnumerationResultVector EnumerationPreprocessManager::preprocess(const PreprocessorFactory& factory,
                                                                 const bool use_gpu) const
{
    EnumerationResultVector results;
    results.reserve(m_library.size());
    for (size_t graph_index = 0U; graph_index < m_library.size(); ++graph_index)
    {
        results.push_back(preprocess_graph(graph_index, factory, use_gpu));
    }
    return results;
}

EnumerationResult EnumerationPreprocessManager::preprocess_graph(const size_t graph_index,
                                                                 const PreprocessorFactory& factory,
                                                                 const bool use_gpu) const
{
    const ColoredGraph& graph = m_library[graph_index];
    m_logger.log(LogLevel::INFO, "Processing graph " + std::to_string(graph_index));
    std::unique_ptr<IGraphPreprocessor> preprocessor = factory(graph, m_logger);
    return preprocessor->calculate(use_gpu);
}

}  // namespace sgf
