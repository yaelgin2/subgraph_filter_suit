#pragma once

#include "ColoredGraph.h"
#include "Constants.h"
#include "IGraphPreprocessor.h"
#include "Int128.h"
#include "LogLevel.h"
#include "LoggerHandler.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace sgf
{

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
class GroupEnumerationPreprocessor : public IGraphPreprocessor
{

public:
    /**
     * @brief Construct a new GroupEnumerationPreprocessor.
     *
     * @param graph The graph to process.
     * @param logger Logger handler used for status/debug output.
     * @param thread_number Maximum number of threads to use during enumeration.
     */
    GroupEnumerationPreprocessor(const ColoredGraph& graph, LoggerHandler logger,
                                 uint32_t thread_number = SgfConstants::DEFAULT_THREAD_NUMBER);

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
    ~GroupEnumerationPreprocessor() override = default;

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
     * @param use_gpu If true, dispatch to the GPU kernel implementation.
     *                Throws InvalidArgumentException if compiled without SGF_CUDA_ENABLED.
     * @return Map of motif identifier to occurrence count.
     */
    EnumerationResult calculate(bool use_gpu = false) override;

#ifdef SGF_CUDA_ENABLED
    /**
     * @brief GPU kernel dispatch entry point.
     *
     * Derived classes that support GPU acceleration override this to launch
     * CUDA kernels and return the enumeration result. The default implementation
     * throws InvalidArgumentException so that preprocessors without GPU support
     * (e.g. PathProcessor) remain fully instantiatable.
     *
     * @return Map of motif identifier to occurrence count.
     */
    virtual EnumerationResult calculate_gpu();
#endif

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
     * @brief Maximum number of threads to use during enumeration.
     */
    uint32_t m_thread_number;

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
     * @brief Enumerate groups and return their counts keyed by canonical motif identifier.
     *
     * Derived classes discover every group, compute its canonical identifier, and
     * accumulate counts into a local map. The base class merges the returned map
     * into its own accumulator with overflow detection.
     *
     * @return Map from canonical motif identifier to occurrence count.
     */
    virtual EnumerationResult stream_groups_to_counter() const = 0;

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
    virtual UInt128 calculate_motif_number(uint32_t motif_descriptor,
                                           const std::vector<uint32_t>& node_colors) const = 0;

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

private:
    /**
     * @brief Returns the human-readable name of the entity type being enumerated.
     *
     * Used by calculate() to produce the "Finished enumerating N <entity_name>" log line.
     * Derived classes return "motifs", "paths", etc.
     *
     * @return Entity name string, e.g. "motifs" or "paths".
     */
    [[nodiscard]] virtual std::string entity_name() const = 0;

};

}  // namespace sgf
