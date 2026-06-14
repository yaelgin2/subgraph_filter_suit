#pragma once

#include "BoostGraph.h"
#include "ColoredGraph.h"
#include "IPatternPreprocessor.h"
#include "LoggerHandler.h"

#include <cstdint>
#include <unordered_set>
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
    uint32_t m_thread_number = 1U;  ///< Maximum threads to use during pattern search.
};

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
class SingleGraphPatternPreprocessor : public IPatternPreprocessor
{
public:
    /**
     * @brief Construct a preprocessor over a graph library.
     *
     * @param graph_library    Non-owning reference to the library of graphs to search.
     * @param is_directed      True if the graphs are directed.
     * @param background_graph Graph that defines the null model for pattern scoring.
     *                         Must not exceed @p graph_library.size().
     * @param score_threshold  Beam search stops when any state's score falls below this.
     * @param config           Finder tuning parameters; uses sensible defaults if omitted.
     * @param logger           Optional logger; defaults to a no-op handler.
     */
    SingleGraphPatternPreprocessor(std::vector<ColoredGraph>& graph_library, bool is_directed,
                                   ColoredGraph background_graph,
                                   const std::vector<bool>& to_process, double score_threshold,
                                   SingleGraphFinderConfig config = {},
                                   LoggerHandler logger = LoggerHandler::null());

    /**
     * @brief Mine patterns from the first @p m_pattern_number graphs in the library.
     *
     * @return All mined patterns paired with their source library graph index.
     * @throws InvalidArgumentException if any search graph or the background graph has no vertices.
     */
    std::vector<PatternPreprocessorResult> calculate() override;

private:
    std::vector<ColoredGraph>& m_graph_library;  ///< Library of graphs to search.
    const bool M_IS_DIRECTED;                    ///< Whether the graphs are directed.
    ColoredGraph m_background_graph;             ///< Null model for pattern scoring.
    const double M_SCORE_THRESHOLD;              ///< Beam search score cutoff.
    const std::vector<bool>& m_to_process;       ///< Indicator vector for which graphs to process.
    const SingleGraphFinderConfig M_CONFIG;      ///< Finder tuning parameters.
    LoggerHandler m_logger;                      ///< Logger for runtime messages.
};

}  // namespace sgf
