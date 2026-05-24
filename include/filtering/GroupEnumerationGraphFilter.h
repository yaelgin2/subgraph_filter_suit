#pragma once

#include "EnumerationPreprocessManager.h"
#include "FilteringUtils.h"
#include "IGraphPreprocessor.h"
#include "LoggerHandler.h"

#include <vector>

namespace sgf
{


/**
 * @class GroupEnumerationGraphFilter
 * @brief Prunes library candidates using motif/path frequency signatures.
 *
 * Holds a precomputed EnumerationResultVector cache (one EnumerationResult per library
 * graph). filter() exploits the necessary condition: if the query graph has
 * fewer occurrences of any motif than a library graph, the query cannot be
 * isomorphic to that library graph, so it is pruned.
 */
class GroupEnumerationGraphFilter
{
public:
    /**
     * @brief Constructs the filter with a precomputed library cache.
     * @param library_cache One EnumerationResult per library graph, indexed by
     *                      library position.
     * @param logger        Optional logger for filter diagnostics.
     */
    GroupEnumerationGraphFilter(EnumerationResultVector library_cache,
                                LoggerHandler logger = LoggerHandler::null());

    ~GroupEnumerationGraphFilter() = default;

    GroupEnumerationGraphFilter(const GroupEnumerationGraphFilter&) = delete;
    GroupEnumerationGraphFilter& operator=(const GroupEnumerationGraphFilter&) = delete;
    GroupEnumerationGraphFilter(GroupEnumerationGraphFilter&&) = delete;
    GroupEnumerationGraphFilter& operator=(GroupEnumerationGraphFilter&&) = delete;

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
    FilterResult filter(const EnumerationResult& graph_features) const;

private:
    EnumerationResultVector m_library_cache;
    LoggerHandler m_logger;
};

}  // namespace sgf
