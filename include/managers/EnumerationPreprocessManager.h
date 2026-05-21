#pragma once

#include "ColoredGraph.h"
#include "IGraphPreprocessor.h"
#include "Int128.h"
#include "LoggerHandler.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace sgf
{

/**
 * @brief Factory that creates an IGraphPreprocessor for a single graph.
 *
 * The caller provides a lambda or function object matching this signature.
 * The manager invokes it once per library graph, passing the graph and its
 * stored logger so the preprocessor can emit diagnostic messages.
 */
using PreprocessorFactory =
    std::function<std::unique_ptr<IGraphPreprocessor>(const ColoredGraph&, LoggerHandler)>;

/**
 * @brief Collection of EnumerationResult values, one per library graph.
 */
using EnumerationData = std::vector<EnumerationResult>;

/**
 * @class EnumerationPreprocessManager
 * @brief Drives enumeration preprocessing across an entire graph library.
 *
 * Owns the library of colored graphs and exposes a single preprocess()
 * entry point. Callers supply a PreprocessorFactory; the manager creates
 * one preprocessor per library graph, runs calculate(), and collects all
 * frequency-signature maps into a vector indexed by library position.
 */
class EnumerationPreprocessManager
{
public:
    /**
     * @brief Construct a manager over the given graph library.
     *
     * @param library Collection of colored graphs to preprocess.
     * @param logger  Logger forwarded to every created preprocessor instance.
     */
    EnumerationPreprocessManager(std::vector<ColoredGraph> library, LoggerHandler logger);

    EnumerationPreprocessManager() = delete;
    EnumerationPreprocessManager(const EnumerationPreprocessManager&) = delete;
    EnumerationPreprocessManager& operator=(const EnumerationPreprocessManager&) = delete;
    EnumerationPreprocessManager(EnumerationPreprocessManager&&) = delete;
    EnumerationPreprocessManager& operator=(EnumerationPreprocessManager&&) = delete;

    /**
     * @brief Default destructor.
     */
    ~EnumerationPreprocessManager() = default;

    /**
     * @brief Preprocess every library graph and return their frequency signatures.
     *
     * For each graph in m_library the manager calls @p factory to obtain a
     * concrete IGraphPreprocessor, invokes calculate() on it, and appends the
     * result.  The returned vector is index-aligned with m_library.
     *
     * @param factory Callable that constructs an IGraphPreprocessor for one graph.
     * @return One frequency-signature map per library graph.
     */
    EnumerationData preprocess(const PreprocessorFactory& factory) const;

private:
    std::vector<ColoredGraph> m_library;
    LoggerHandler m_logger;
};

}  // namespace sgf
