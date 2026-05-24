#pragma once

#include "ColoredGraph.h"
#include "IPatternPreprocessor.h"
#include "LoggerHandler.h"

#include <functional>
#include <memory>
#include <vector>

namespace sgf
{

/**
 * @brief Factory that creates an IPatternPreprocessor backed by the full graph library.
 *
 * The caller provides a lambda or function object matching this signature.
 * The manager passes the owned library reference and its logger so the factory
 * can construct the concrete preprocessor with all strategy-specific parameters
 * already captured.
 */
using PatternPreprocessorFactory =
    std::function<std::unique_ptr<IPatternPreprocessor>(std::vector<ColoredGraph>&, LoggerHandler)>;

/**
 * @class PatternPreprocessManager
 * @brief Drives pattern preprocessing across an entire graph library.
 *
 * Owns the library of colored graphs and exposes a single preprocess()
 * entry point. Callers supply a PatternPreprocessorFactory; the manager
 * creates the preprocessor, runs calculate() once, and returns all mined
 * patterns.
 */
class PatternPreprocessManager
{
public:
    /**
     * @brief Construct a manager over the given graph library.
     *
     * @param library Collection of colored graphs to preprocess.
     * @param logger  Logger forwarded to the created preprocessor instance.
     */
    PatternPreprocessManager(std::vector<ColoredGraph> library, LoggerHandler logger);

    PatternPreprocessManager() = delete;
    PatternPreprocessManager(const PatternPreprocessManager&) = delete;
    PatternPreprocessManager& operator=(const PatternPreprocessManager&) = delete;
    PatternPreprocessManager(PatternPreprocessManager&&) = delete;
    PatternPreprocessManager& operator=(PatternPreprocessManager&&) = delete;

    /**
     * @brief Default destructor.
     */
    ~PatternPreprocessManager() = default;

    /**
     * @brief Mine patterns from the library using the given preprocessor strategy.
     *
     * Calls @p factory with the owned library and logger to obtain a concrete
     * IPatternPreprocessor, then invokes calculate() and returns all results.
     *
     * @param factory Callable that constructs an IPatternPreprocessor.
     * @return All mined patterns, each paired with the set of library graph indices it covers.
     */
    std::vector<PatternPreprocessorResult> preprocess(const PatternPreprocessorFactory& factory);

private:
    std::vector<ColoredGraph> m_library;
    LoggerHandler m_logger;
};

}  // namespace sgf
