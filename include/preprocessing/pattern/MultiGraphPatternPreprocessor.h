#pragma once

#include "ColoredGraph.h"
#include "IPatternPreprocessor.h"
#include "LoggerHandler.h"

#include <cstdint>
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
class MultiGraphPatternPreprocessor : public IPatternPreprocessor
{
public:
    /**
     * @brief Construct a preprocessor for a given graph library.
     * @param graph_library  Non-owning reference to the library of graphs to mine patterns from.
     * @param is_directed    True if the graphs are directed.
     * @param pattern_number Number of patterns to extract.
     * @param alive_precent  Minimum fraction of graphs (0–100) a pattern must appear in.
     * @param logger         Optional logger; defaults to a no-op handler.
     */
    MultiGraphPatternPreprocessor(std::vector<ColoredGraph>& graph_library, bool is_directed,
                                  uint32_t pattern_number, uint32_t alive_precent,
                                  LoggerHandler logger = LoggerHandler::null());

    /**
     * @brief Mine @p m_pattern_number patterns from the library.
     * @return Vector of mined patterns, each paired with the set of library graph indices it covers.
     */
    std::vector<PatternPreprocessorResult> calculate() override;

private:
    std::vector<ColoredGraph>& m_graph_library;  ///< Library of graphs to mine.
    const bool M_IS_DIRECTED;                    ///< Whether graphs are directed.
    const uint32_t m_pattern_number;             ///< Number of patterns to extract.
    const uint32_t m_alive_precent;              ///< Minimum coverage fraction (0–100).
    LoggerHandler m_logger;                      ///< Logger for runtime messages.
};

}  // namespace sgf
