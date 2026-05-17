#pragma once

#include "EnumerationPreprocessManager.h"
#include "IGraphPreprocessor.h"

#include <vector>

namespace sgf
{

/**
 * @brief Result of a filter pass: one bool per library graph.
 *
 * true  — candidate is pruned (query cannot be isomorphic to this library graph).
 * false — candidate survives and must be checked further.
 */
using FilterResult = std::vector<bool>;

/**
 * @class GroupEnumerationGraphFilter
 * @brief Prunes library candidates using motif/path frequency signatures.
 *
 * Holds a precomputed EnumerationData cache (one EnumerationResult per library
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
     */
    explicit GroupEnumerationGraphFilter(EnumerationData library_cache);

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
    EnumerationData m_library_cache;
};

}  // namespace sgf
