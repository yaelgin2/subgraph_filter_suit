#pragma once

#include "BoostGraph.h"
#include "ColoredGraph.h"
#include "LoggerHandler.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace sgf
{

/**
 * @brief Tuning parameters forwarded to SingleGraphPatternFinder on each calculate() call.
 *
 * Default values mirror the defaults in SingleGraphPatternFinder's constructor.
 */
struct SingleGraphFinderConfig
{
    /// Default maximum number of simultaneously active beam states.
    static constexpr uint32_t DEFAULT_MAX_ACTIVE_PATTERNS = 500U;

    /// Default initial weight for the outside-neighbour score term.
    static constexpr double DEFAULT_ALPHA_0 = 1.0;

    /// Default per-depth multiplicative decay applied to alpha_0.
    static constexpr double DEFAULT_ALPHA_DECAY = 0.9;

    uint32_t m_max_active_patterns = DEFAULT_MAX_ACTIVE_PATTERNS;  ///< Beam width cap.
    double m_alpha_0 = DEFAULT_ALPHA_0;                            ///< Initial alpha weight.
    double m_alpha_decay = DEFAULT_ALPHA_DECAY;                    ///< Per-depth alpha decay.
};

/**
 * @brief Result of one pattern search: the found pattern and the library graph index it came from.
 */
using SingleGraphPatternResult = std::pair<BoostGraph, std::vector<uint32_t>>;

/**
 * @class SingleGraphPatternPreprocessor
 * @brief Extracts representative patterns from a library of graphs using a shared background model.
 *
 * Iterates over the first @p pattern_number graphs in the library, running
 * SingleGraphPatternFinder on each with the caller-supplied background graph.
 * Every pattern returned by the finder is collected together with the index of
 * the library graph it was discovered in.
 *
 * The resulting patterns are used downstream by the pattern filtering stage to
 * eliminate unlikely subgraph candidates.
 */
class SingleGraphPatternPreprocessor
{
public:
    /**
     * @brief Construct a preprocessor over a graph library.
     *
     * @param graph_library  Non-owning reference to the library of graphs to search.
     * @param is_directed    True if the graphs are directed.
     * @param logger         Optional logger; defaults to a no-op handler.
     */
    SingleGraphPatternPreprocessor(std::vector<ColoredGraph>& graph_library, bool is_directed,
                                   LoggerHandler logger = LoggerHandler::null());

    /**
     * @brief Mine patterns from the first @p pattern_number graphs in the library.
     *
     * Creates a SingleGraphPatternFinder backed by @p background_graph and runs
     * find_pattern on @c m_graph_library[0] … @c m_graph_library[pattern_number-1].
     * Every pattern returned by each find_pattern call is appended to the result
     * paired with the corresponding library graph index.
     *
     * @param background_graph  Graph that defines the null model for pattern scoring.
     * @param pattern_number    Number of library graphs to process.
     *                          Must not exceed @c m_graph_library.size().
     * @param score_threshold   Beam search stops when any state's score falls below this.
     * @param config            Finder tuning parameters; uses sensible defaults if omitted.
     * @return All mined patterns paired with their source library graph index.
     * @throws InvalidArgumentException if any search graph or the background graph has no vertices.
     */
    std::vector<SingleGraphPatternResult> calculate(ColoredGraph background_graph,
                                                    uint32_t pattern_number, double score_threshold,
                                                    SingleGraphFinderConfig config = {});

private:
    std::vector<ColoredGraph>& m_graph_library;  ///< Library of graphs to search.
    const bool M_IS_DIRECTED;                    ///< Whether the graphs are directed.
    LoggerHandler m_logger;                      ///< Logger for runtime messages.
};

}  // namespace sgf
