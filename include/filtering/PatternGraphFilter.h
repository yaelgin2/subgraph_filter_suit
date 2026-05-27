#pragma once

#include "FilteringUtils.h"
#include "IPatternPreprocessor.h"
#include "LoggerHandler.h"
#include "PatternPreprocessManager.h"
#include "PriorPolicy.h"

#include <vector>

namespace sgf
{

using ColoredGraphPatternResult = std::pair<ColoredGraph, std::unordered_set<uint32_t>>;

/**
 * @class PatternGraphFilter
 * @brief Prunes library candidates using motif/path frequency signatures.
 *
 * Holds a precomputed EnumerationResultVector cache (one EnumerationResult per library
 * graph). filter() exploits the necessary condition: if the query graph has
 * fewer occurrences of any motif than a library graph, the query cannot be
 * isomorphic to that library graph, so it is pruned.
 */
class PatternGraphFilter
{
public:
    /**
     * @brief Constructs the filter with a precomputed library cache.
     * @param library_cache One EnumerationResult per library graph, indexed by
     *                      library position.
     * @param logger        Optional logger for filter diagnostics.
     */
    explicit PatternGraphFilter(std::vector<ColoredGraphPatternResult> library_cache,
                                LoggerHandler logger = LoggerHandler::null());

    ~PatternGraphFilter() = default;

    PatternGraphFilter(const PatternGraphFilter&) = delete;
    PatternGraphFilter& operator=(const PatternGraphFilter&) = delete;
    PatternGraphFilter(PatternGraphFilter&&) = delete;
    PatternGraphFilter& operator=(PatternGraphFilter&&) = delete;

    /**
     * @brief Prunes library candidates against a query enumeration result.
     *
     * For each library graph, iterates its motif counts. If the query has
     * fewer occurrences of any motif than the library graph requires, that
     * library graph is marked as pruned (true) and the inner loop breaks early.
     *
     * @param graph_features Motif frequency map of the query graph.
     * @return FilterResult of size equal to the library: true = pruned,
     *         false = survives.
     */
    FilterResult filter(const ColoredGraph& graph, bool is_induced, PriorPolicy prior_policy) const;

private:
    std::vector<ColoredGraphPatternResult> m_library_cache;
    LoggerHandler m_logger;
};

}  // namespace sgf
