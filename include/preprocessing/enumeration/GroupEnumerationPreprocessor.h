#pragma once

#include "ColoredGraph.h"
#include "LogLevel.h"
#include "LoggerHandler.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace sgf
{

/**
 * @brief Hash functor for __int128_t (GCC extension; no std::hash specialization by default).
 */
struct Int128Hash
{
    /**
     * @brief Hashes a 128-bit integer.
     * @param value The value to hash.
     * @return Combined hash of the high and low 64-bit halves.
     */
    size_t operator()(const __int128_t value) const noexcept
    {
        constexpr uint32_t HIGH_BITS_SHIFT = 64U;
        constexpr size_t HASH_MIX_SHIFT = 1U;
        const uint64_t high =
            static_cast<uint64_t>(static_cast<__uint128_t>(value) >> HIGH_BITS_SHIFT);
        const uint64_t low = static_cast<uint64_t>(value);
        return std::hash<uint64_t>{}(high) ^ (std::hash<uint64_t>{}(low) << HASH_MIX_SHIFT);
    }
};

/**
 * @brief Callback invoked once per discovered group during enumeration.
 *
 * Parameters:
 * - group_structure_descriptor: numeric encoding of the group's edge structure.
 * - group_vertex_ids: vertex identifiers belonging to the group.
 *
 * The base class passes this callback to stream_groups_to_counter() so that
 * each group is counted immediately and never stored in a collection.
 */
using GroupCounterCallback = std::function<void(uint32_t group_structure_descriptor,
                                                const std::vector<uint32_t>& group_vertex_ids)>;

/**
 * @class GroupEnumerationPreprocessor
 * @brief Abstract base class for graph group enumeration and motif preprocessing.
 *
 * This class provides a reusable framework for enumerating groups of vertices
 * from a ColoredGraph and transforming each discovered group into a compact
 * motif identifier.
 *
 * The class performs the common pipeline:
 * 1. Build a graph adjacency matrix.
 * 2. Sort nodes according to combined degree.
 * 3. Discover candidate groups.
 * 4. Build per-group adjacency matrices.
 * 5. Convert each group into a motif identifier.
 * 6. Count motif frequencies.
 *
 * Derived classes must implement:
 * - group discovery logic,
 * - motif encoding logic.
 *
 * @note This class is abstract and intended for inheritance only.
 */
class GroupEnumerationPreprocessor
{

public:
    /**
     * @brief Construct a new GroupEnumerationPreprocessor.
     *
     * @param graph The graph to process.
     * @param logger Logger handler used for status/debug output.
     */
    GroupEnumerationPreprocessor(const ColoredGraph& graph, LoggerHandler logger);

    GroupEnumerationPreprocessor() = delete;
    GroupEnumerationPreprocessor(const GroupEnumerationPreprocessor&) = delete;
    GroupEnumerationPreprocessor& operator=(const GroupEnumerationPreprocessor&) = delete;
    GroupEnumerationPreprocessor(GroupEnumerationPreprocessor&&) = delete;
    GroupEnumerationPreprocessor& operator=(GroupEnumerationPreprocessor&&) = delete;

    /**
     * @brief Virtual destructor.
     *
     * Ensures proper destruction through base pointers.
     */
    virtual ~GroupEnumerationPreprocessor() = default;

    /**
     * @brief Run the full group enumeration pipeline.
     *
     * Executes preprocessing and motif counting:
     * - converts graph to adjacency matrix,
     * - sorts nodes,
     * - finds groups,
     * - computes motif identifiers,
     * - counts occurrences.
     *
     * @return Map of motif identifier to occurrence count.
     */
    std::unordered_map<__int128_t, uint32_t, Int128Hash> calculate();

protected:
    /**
     * @brief Graph instance being processed.
     */
    const ColoredGraph& m_graph;

    /**
     * @brief Logger used for runtime messages.
     */
    LoggerHandler m_logger;

    /**
     * @brief Node ordering used during enumeration.
     *
     * Populated by sort_nodes() before group discovery begins.
     */
    std::vector<uint32_t> m_node_order;

    /**
     * @brief Combined (out + in) degree of @p vertex; out-degree only for undirected.
     * @param vertex Vertex to query.
     * @return Count of distinct neighbours in either direction.
     */
    size_t combined_degree(uint32_t vertex) const;

    /**
     * @brief Sort graph nodes by combined degree (descending) before enumeration.
     *
     * Populates m_node_order with vertex indices ordered by decreasing combined degree.
     * Higher-degree nodes are processed first, which reduces the number of
     * candidate groups that need full evaluation.
     */
    virtual void sort_nodes();

    /**
     * @brief Enumerate groups of vertices and report each one via callback.
     *
     * Derived classes discover candidate groups and invoke @p count_group
     * exactly once per group as it is found. The base class supplies a
     * counting callback so no group collection is ever materialized in memory.
     *
     * @param graph_adjacency_matrix Dense boolean adjacency matrix of the graph.
     * @param count_group Callback to invoke for each discovered group.
     */
    virtual void
    stream_groups_to_counter(const std::vector<std::vector<bool>>& graph_adjacency_matrix,
                             const GroupCounterCallback& count_group) const = 0;

    /**
     * @brief Convert a group into a unique motif identifier.
     *
     * Implementations must deterministically encode:
     * - node colors or labels,
     * - internal edge structure.
     *
     * @param motif_descriptor Numeric motif/color/group descriptor for the group.
     * @param node_colors Color labels of the group's vertices in traversal order.
     *
     * @return Unique numeric motif identifier.
     */
    virtual __int128_t calculate_motif_number(uint32_t motif_descriptor,
                                              const std::vector<uint32_t>& node_colors) const = 0;

private:
    /**
     * @brief Convert the full graph into an adjacency matrix.
     *
     * Creates a dense boolean matrix where:
     * adjacency_matrix[u][v] == true if edge (u,v) exists.
     *
     * @param adjacency_matrix Output matrix to populate.
     */
    void graph_to_adjacency_matrix(std::vector<std::vector<bool>>& adjacency_matrix) const;

    /**
     * @brief Extract node colors for a specific group of vertices.
     *
     * Returns the color label of each vertex in the group, preserving
     * the order of vertices as supplied.
     *
     * @param group Vertex identifiers belonging to the group.
     * @return Color labels corresponding to each vertex in @p group.
     */
    std::vector<uint32_t> group_to_node_colors(const std::vector<uint32_t>& group) const;
};

}  // namespace sgf
