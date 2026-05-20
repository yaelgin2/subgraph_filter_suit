#pragma once

#include "ColoredGraph.h"
#include "LoggerHandler.h"
#include "MultiGraphPatternFinder.h"

#include <vector>

namespace sgf
{

/**
 * @class MultiGraphPatternPreprocessor
 * @brief Computes a set of representative patterns from a multi-graph library.
 *
 * Repeatedly invokes MultiGraphPatternFinder to extract patterns that are
 * common across the graph library. The resulting patterns are used downstream
 * by the pattern filtering stage to eliminate unlikely subgraph candidates.
 */
class MultiGraphPatternPreprocessor
{
public:
    /**
     * @brief Construct a preprocessor for a given graph library.
     * @param graph_library Non-owning reference to the library of graphs to mine patterns from.
     * @param is_directed True if the graphs are directed.
     * @param logger Optional logger; defaults to a no-op handler.
     */
    MultiGraphPatternPreprocessor(std::vector<ColoredGraph>& graph_library, bool is_directed,
                                  LoggerHandler logger = LoggerHandler::null());

    /**
     * @brief Mine a fixed number of patterns from the library.
     * @param pattern_number Number of patterns to extract.
     * @param alive_precent Minimum fraction of graphs (0–100) a pattern must appear in.
     * @return Vector of mined patterns, each paired with the set of graph indices it covers.
     */
    std::vector<MultiGraphPatternResult> calculate(uint32_t pattern_number, uint32_t alive_precent);

private:
    std::vector<ColoredGraph>& m_graph_library;  ///< Library of graphs to mine.
    const bool M_IS_DIRECTED;                    ///< Whether graphs are directed.
    LoggerHandler m_logger;                      ///< Logger for runtime messages.
};

}  // namespace sgf
