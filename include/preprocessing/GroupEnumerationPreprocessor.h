#pragma once

#include <memory>
#include <unordered_map>
#include <vector>
#include <ColoredGraph.h>
#include "ILogger.h"
#include "LogLevel.h"
#include "LoggerHandler.h"

/**
 * @brief Alias representing the collection of discovered groups.
 *
 * Each entry contains:
 * - first: a numeric motif/color/group descriptor.
 * - second: the list of vertex identifiers belonging to the group.
 *
 * The exact meaning of the first field depends on the derived implementation.
 */
using Groups = std::vector<std::pair<uint32_t, std::vector<int>>>;

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
     * @param m_logger Logger handler used for status/debug output.
     *
     * @throws std::invalid_argument Recommended if graph is null.
     */
    GroupEnmerationPreprocessor(
        std::shared_ptr<ColoredGraph> graph,
        LoggerHandler& m_logger
    );

    /**
     * @brief Virtual destructor.
     *
     * Ensures proper destruction through base pointers.
     */
    ~GroupEnmerationPreprocessor() = default;

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
     * @brief Discover groups of vertices to evaluate.
     *
     * Derived classes define how candidate groups are generated.
     *
     * @return Collection of discovered groups.
     */
    virtual Groups find_groups() = 0;

    /**
     * @brief Convert a group into a unique motif identifier.
     *
     * Implementations must deterministically encode:
     * - node colors or labels,
     * - internal edge structure.
     *
     * @param colors Ordered color/group descriptor.
     * @param edges Adjacency matrix of the group.
     *
     * @return Unique numeric motif identifier.
     */
    virtual __uint128_t calculate_motif_number(
        std::vector<uint32_t> colors,
        std::vector<std::vector<bool>> edges
    ) = 0;

private:

    /**
     * @brief Graph instance being processed.
     */
    std::shared_ptr<ColoredGraph> m_graph;

    /**
     * @brief Node ordering used during enumeration.
     *
     * Typically populated by sort_nodes().
     */
    std::vector<uint32_t> m_node_order;

    /**
     * @brief Logger used for runtime messages.
     */
    LoggerHandler& m_logger;

    /**
     * @brief Convert the full graph into an adjacency matrix.
     *
     * Creates a dense boolean matrix where:
     * adjacency_matrix[u][v] == true if edge (u,v) exists.
     *
     * @param adjacency_matrix Output matrix to populate.
     */
    void graph_to_adjacency_matrix(
        std::vector<std::vector<bool>>& adjacency_matrix
    );

    /**
     * @brief Build an adjacency matrix for a specific group of vertices.
     *
     * Creates an induced subgraph adjacency matrix preserving the order of
     * vertices in the supplied group vector.
     *
     * @param group Vertex identifiers belonging to the group.
     *
     * @return Adjacency matrix of the induced subgraph.
     */
    std::vector<std::vector<bool>> group_to_adjacency_matrix(
        const std::vector<uint32_t>& group
    );
};

} // namespace sgf