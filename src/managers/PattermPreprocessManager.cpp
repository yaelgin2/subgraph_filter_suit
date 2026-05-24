#include "PattermPreprocessManager.h"

#include "ColoredGraph.h"
#include "IPatternPreprocessor.h"
#include "LoggerHandler.h"

#include <memory>
#include <utility>
#include <vector>

namespace sgf
{

PatternPreprocessManager::PatternPreprocessManager(std::vector<ColoredGraph> library,
                                                   LoggerHandler logger)
    : m_library(std::move(library))
    , m_logger(std::move(logger))
{
}

std::vector<PatternPreprocessorResult>
PatternPreprocessManager::preprocess(const PatternPreprocessorFactory& factory)
{
    std::unique_ptr<IPatternPreprocessor> preprocessor = factory(m_library, m_logger);
    return preprocessor->calculate();
}

}  // namespace sgf
