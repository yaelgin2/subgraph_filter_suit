#pragma once

#include "ColoredGraph.h"
#include "LogLevel.h"
#include "LoggerHandler.h"

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace std
{

/**
 * @brief Hash specialization for __uint128_t (GCC extension, no std::hash by default).
 */
template <>
struct hash<__uint128_t>
{
    /**
     * @brief Hashes a 128-bit unsigned integer.
     * @param value The value to hash.
     * @return Combined hash of the high and low 64-bit halves.
     */
    size_t operator()(const __uint128_t value) const noexcept
    {
        constexpr uint32_t high_bits_shift = 64U;
        constexpr size_t hash_mix_shift = 1U;
        const uint64_t high = static_cast<uint64_t>(value >> high_bits_shift);
        const uint64_t low = static_cast<uint64_t>(value);
        return std::hash<uint64_t>{}(high) ^ (std::hash<uint64_t>{}(low) << hash_mix_shift);
    }
};

}  // namespace std

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

namespace sgf
{

/**
 * @class GroupEnmerationPreprocessor
 * @brief Abstract base class for graph group enumeration and motif preprocessing.
 *
 * This class provides a reusable framework for enumerating groups of vertices
 * from a ColoredGraph and transforming each discovered group into a compact
 * motif identifier.
 *
 * The class performs the common pipeline:
 * 1. Build a graph adjacency matrix.
 * 2. Sort nodes according to a derived strategy.
 * 3. Discover candidate groups.
 * 4. Build per-group adjacency matrices.
 * 5. Convert each group into a motif identifier.
 * 6. Count motif frequencies.
 *
 * Derived classes must implement:
 * - node ordering logic,
 * - group discovery logic,
 * - motif encoding logic.
 *
 * @note This class is abstract and intended for inheritance only.
 * @note The graph object is shared using std::shared_ptr.
 */
class GroupEnmerationPreprocessor
{

public:
    /**
     * @brief Construct a new GroupEnmerationPreprocessor.
     *
     * @param graph Shared pointer to the graph to process.
     * @param logger Logger handler used for status/debug output.
     *
     * @throws std::invalid_argument Recommended if graph is null.
     */
    GroupEnmerationPreprocessor(const ColoredGraph& graph, LoggerHandler logger);

    GroupEnmerationPreprocessor() = default;
    GroupEnmerationPreprocessor(const GroupEnmerationPreprocessor&) = delete;
    GroupEnmerationPreprocessor& operator=(const GroupEnmerationPreprocessor&) = delete;
    GroupEnmerationPreprocessor(GroupEnmerationPreprocessor&&) = delete;
    GroupEnmerationPreprocessor& operator=(GroupEnmerationPreprocessor&&) = delete;

    /**
     * @brief Virtual destructor.
     *
     * Ensures proper destruction through base pointers.
     */
    virtual ~GroupEnmerationPreprocessor() = default;

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
    std::unordered_map<__uint128_t, uint32_t> calculate();

protected:
    /**
     * @brief Logger used for runtime messages.
     */
    LoggerHandler m_logger;

    /**
     * @brief Graph instance being processed.
     */
    const ColoredGraph& m_graph;

    /**
     * @brief Sort graph nodes before enumeration.
     *
     * Derived classes implement the ordering strategy and typically populate
     * m_node_order.
     *
     * Ordering may improve:
     * - deterministic traversal,
     * - pruning,
     * - performance,
     * - symmetry reduction.
     */
    virtual void sort_nodes() = 0;

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
                             const GroupCounterCallback& count_group) = 0;

    /**
     * @brief Convert a group into a unique motif identifier.
     *
     * Implementations must deterministically encode:
     * - node colors or labels,
     * - internal edge structure.
     *
     * @param motif_descriptor Numeric motif/color/group descriptor for the group.
     * @param edges Adjacency matrix of the group.
     *
     * @return Unique numeric motif identifier.
     */
    virtual __uint128_t calculate_motif_number(uint32_t motif_descriptor,
                                               const std::vector<uint32_t>& node_colors) = 0;

private:
    /**
     * @brief Node ordering used during enumeration.
     *
     * Typically populated by sort_nodes().
     */
    std::vector<uint32_t> m_node_order;

    /**
     * @brief Convert the full graph into an adjacency matrix.
     *
     * Creates a dense boolean matrix where:
     * adjacency_matrix[u][v] == true if edge (u,v) exists.
     *
     * @param adjacency_matrix Output matrix to populate.
     */
    void graph_to_adjacency_matrix(std::vector<std::vector<bool>>& adjacency_matrix);

    /**
     * @brief Extract node colors for a specific group of vertices.
     *
     * Returns the color label of each vertex in the group, preserving
     * the order of vertices as supplied.
     *
     * @param group Vertex identifiers belonging to the group.
     * @return Color labels corresponding to each vertex in @p group.
     */
    std::vector<uint32_t> group_to_node_colors(const std::vector<uint32_t>& group);
};

}  // namespace sgf
