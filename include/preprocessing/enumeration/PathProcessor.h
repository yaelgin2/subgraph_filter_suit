#pragma once

#include "ColoredGraph.h"
#include "GroupEnumerationPreprocessor.h"
#include "LoggerHandler.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace sgf
{

using NeighbourIteratorPair =
    std::pair<std::vector<uint32_t>::const_iterator, std::vector<uint32_t>::const_iterator>;

/**
 * @brief Ordered sequence of (vertex, traversal-direction) hops forming one half-path.
 *
 * Each pair encodes a vertex index and the edge direction used to reach it
 * (false = outgoing, true = incoming; only meaningful in directed graphs).
 */
using PathInformation = std::vector<std::pair<uint32_t, bool>>;

/**
 * @brief A half-open iterator range over a vertex's sorted neighbour list.
 */
struct NeighbourRange
{
    std::vector<uint32_t>::const_iterator m_begin;  ///< First outgoing neighbour.
    std::vector<uint32_t>::const_iterator m_end;    ///< One past last outgoing neighbour.
};

/**
 * @class PathProcessor
 * @brief Enumerates all simple 4-edge paths (5 distinct vertices) in a colored graph.
 *
 * Extends GroupEnumerationPreprocessor to enumerate all simple paths consisting
 * of exactly 4 edges (5 distinct vertices). Each path is treated as rooted at
 * its middle vertex; two pairs of depth-1 and depth-2 neighbours are combined
 * to form the full path. Pairs sharing any vertex are discarded to ensure simplicity.
 *
 * The motif identifier encodes the vertex color sequence and, for directed graphs,
 * the bit-packed edge direction descriptor. The canonical form is chosen as the
 * lexicographically smaller of the forward and reversed representations so that
 * opposite-direction traversals of the same undirected path yield the same identifier.
 */
class PathProcessor : public GroupEnumerationPreprocessor
{
public:
    /**
     * @brief Construct a PathProcessor for the given graph.
     * @param graph The colored graph to preprocess.
     * @param logger Logger handler for status and debug output.
     * @param thread_number Maximum number of threads to use during enumeration.
     */
    PathProcessor(const ColoredGraph& graph, LoggerHandler logger, uint32_t thread_number = 1U);

    PathProcessor() = delete;
    PathProcessor(const PathProcessor&) = delete;
    PathProcessor& operator=(const PathProcessor&) = delete;
    PathProcessor(PathProcessor&&) = delete;
    PathProcessor& operator=(PathProcessor&&) = delete;

    /**
     * @brief Default destructor.
     */
    ~PathProcessor() override = default;

protected:
    /**
     * @brief Enumerate all simple 4-edge paths and report each via callback.
     *
     * Iterates over all vertices in m_node_order. For each root vertex, all pairs
     * of distinct depth-1 neighbours are considered; for each pair, all combinations
     * of depth-2 expansions are emitted if the resulting 5-vertex path is simple.
     *
     * @param graph_adjacency_matrix Dense boolean adjacency matrix (unused; neighbour
     *        lists from ColoredGraph are used directly).
     * @param count_group Callback invoked once per discovered path.
     */
    void stream_groups_to_counter(
        [[maybe_unused]] const std::vector<std::vector<bool>>& graph_adjacency_matrix,
        const GroupCounterCallback& count_group) const override;

    /**
     * @brief Encode a 4-edge path into a canonical 128-bit motif identifier.
     *
     * Packs vertex colors into the low 120 bits (5 slots × 24 bits each).
     * For undirected graphs, returns the minimum of the forward and reversed
     * color sequences. For directed graphs, also packs the edge-direction
     * descriptor into the high bits, selecting the descriptor that corresponds
     * to the canonical (smaller) color sequence.
     *
     * @param motif_descriptor Bit-packed edge directions (one bit per edge, 4 bits total).
     * @param node_colors Color labels of the 5 vertices in traversal order.
     * @return Canonical 128-bit path identifier.
     */
    UInt128 calculate_motif_number(uint32_t motif_descriptor,
                                   const std::vector<uint32_t>& node_colors) const override;

    /**
     * @brief Enumerate all 4-edge paths rooted at @p root.
     *
     * Considers all ordered pairs of distinct depth-1 neighbours of @p root,
     * including both outgoing and (for directed graphs) incoming edges at each
     * depth. Delegates each pair to
     * stream_groups_to_counter_for_two_depth_one_neighbours.
     *
     * @param count_group Callback invoked for each emitted path.
     * @param root The middle vertex of each enumerated path.
     */
    void stream_groups_to_counter_for_vertex(const GroupCounterCallback& count_group,
                                             uint32_t root) const;

    /**
     * @brief Enumerate paths for all pairs within the outgoing neighbour list of @p root.
     *
     * Iterates over all pairs of outgoing depth-1 neighbours and delegates each pair
     * to stream_groups_to_counter_for_two_depth_one_neighbours. For directed graphs
     * also pairs each outgoing neighbour with every incoming neighbour.
     *
     * @param count_group Callback invoked for each emitted path.
     * @param root The middle vertex of each enumerated path.
     * @param depth_one_out Half-open range over the root's outgoing neighbours.
     * @param depth_one_in Half-open range over the root's incoming neighbours.
     */
    void stream_groups_for_out_neighbours(const GroupCounterCallback& count_group, uint32_t root,
                                          NeighbourRange depth_one_out,
                                          NeighbourRange depth_one_in) const;

    /**
     * @brief Enumerate paths for all pairs within the incoming neighbour list of @p root.
     *
     * For directed graphs only. Iterates over all pairs of incoming depth-1 neighbours
     * and delegates each to stream_groups_to_counter_for_two_depth_one_neighbours.
     *
     * @param count_group Callback invoked for each emitted path.
     * @param root The middle vertex of each enumerated path.
     * @param depth_one_in_start Iterator to the start of the incoming neighbour list.
     * @param depth_one_in_end Iterator past the end of the incoming neighbour list.
     */
    void
    stream_groups_for_in_neighbours(const GroupCounterCallback& count_group, uint32_t root,
                                    std::vector<uint32_t>::const_iterator depth_one_in_start,
                                    std::vector<uint32_t>::const_iterator depth_one_in_end) const;

private:
    /**
     * @brief Number of edges in each enumerated path.
     */
    /**
     * @brief Returns "paths" to label the finished-enumeration log line.
     * @return The string "paths".
     */
    [[nodiscard]] std::string entity_name() const override;

    static constexpr uint32_t PATH_EDGE_COUNT = 4U;

    /**
     * @brief Number of vertices in a path of length PATH_EDGE_COUNT.
     */
    static constexpr uint32_t PATH_VERTEX_COUNT = PATH_EDGE_COUNT + 1U;

    /**
     * @brief Bits allocated per vertex color slot in the 128-bit identifier.
     */
    static constexpr uint32_t COLOR_BITS_PER_SLOT = 24U;

    /**
     * @brief Iterator and traversal direction for a depth-1 neighbour of the path root.
     *
     * Bundles the neighbour-list iterator with the edge direction (false = outgoing,
     * true = incoming) used to reach depth-2 neighbours from that position.
     */
    struct DepthOneNeighbourInfo
    {
        /**
         * @brief Iterator pointing at the depth-1 neighbour vertex index.
         */
        std::vector<uint32_t>::const_iterator m_iterator;

        /**
         * @brief Edge traversal direction for depth-2 expansion (false = outgoing,
         * true = incoming).
         */
        bool m_direction{false};
    };

    /**
     * @brief Enumerate paths for a depth-1 neighbour pair across all direction combinations.
     *
     * Calls the two-DepthOneNeighbourInfo overload with every combination of direction_one and
     * direction_two ({false,false}, {false,true}, {true,false}, {true,true}) to cover
     * all edge traversal directions at depth 2.
     *
     * @param count_group Callback for each emitted path.
     * @param root Middle vertex of the path.
     * @param first_depth_one_neighbour Iterator to the first depth-1 neighbour of root.
     * @param second_depth_one_neighbour Iterator to the second depth-1 neighbour of root.
     */
    void stream_groups_to_counter_for_two_depth_one_neighbours(
        const GroupCounterCallback& count_group, uint32_t root,
        std::vector<uint32_t>::const_iterator first_depth_one_neighbour,
        std::vector<uint32_t>::const_iterator second_depth_one_neighbour) const;

    /**
     * @brief Enumerate paths for a depth-1 neighbour pair with fixed depth-2 directions.
     *
     * Expands one hop further from each depth-1 neighbour using the given traversal
     * directions, then emits every non-intersecting combination via count_group.
     *
     * @param count_group Callback for each emitted path.
     * @param root Middle vertex of the path.
     * @param first_neighbour_info Iterator and direction for first depth-1 neighbour.
     * @param second_neighbour_info Iterator and direction for second depth-1 neighbour.
     */
    void stream_groups_to_counter_for_two_depth_one_neighbours(
        const GroupCounterCallback& count_group, uint32_t root,
        const DepthOneNeighbourInfo& first_neighbour_info,
        const DepthOneNeighbourInfo& second_neighbour_info) const;

    /**
     * @brief Compute the canonical direction descriptor for the reverse traversal of a path.
     *
     * Bit-reverses and bit-inverts the 4-bit @p motif_descriptor to produce the
     * descriptor that corresponds to traversing the same path in the opposite direction.
     *
     * @param motif_descriptor Original 4-bit direction descriptor (one bit per edge).
     * @return Reversed and inverted 4-bit direction descriptor.
     */
    static uint32_t compute_reversed_descriptor(uint32_t motif_descriptor);

    /**
     * @brief Check whether two 2-hop half-paths share any vertex that would create a cycle.
     *
     * Returns true if joining the two half-paths at the root would produce a
     * non-simple walk (a vertex visited more than once). The root itself is
     * excluded from this check as it is always shared by design.
     *
     * @param first_path Left 2-hop half-path: {(depth-1 vertex, dir), (depth-2 vertex, dir)}.
     * @param second_path Right 2-hop half-path: {(depth-1 vertex, dir), (depth-2 vertex, dir)}.
     * @return True if the joined path would be non-simple.
     */
    static bool check_path_intersection(const PathInformation& first_path,
                                        const PathInformation& second_path);

    /**
     * @brief Concatenate two 2-hop half-paths and the shared root into a vertex sequence.
     *
     * Returns the 5-vertex list: [first[1], first[0], root, second[0], second[1]].
     *
     * @param root The middle vertex shared by both half-paths.
     * @param first_path Left 2-hop half-path.
     * @param second_path Right 2-hop half-path.
     * @return Vertex index sequence of length PATH_VERTEX_COUNT.
     */
    static std::vector<uint32_t> concatenate_path(uint32_t root, const PathInformation& first_path,
                                                  const PathInformation& second_path);

    /**
     * @brief Compute the bit-packed edge-direction descriptor for a path.
     *
     * Encodes one bit per hop in @p first_path then @p second_path (true = 1,
     * false = 0). Returns 0 for undirected graphs, where direction is meaningless.
     *
     * @param first_path Left 2-hop half-path.
     * @param second_path Right 2-hop half-path.
     * @return Bit-packed direction descriptor (4 bits), or 0 for undirected graphs.
     */
    uint32_t compute_motif_descriptor(const PathInformation& first_path,
                                      const PathInformation& second_path) const;
};

}  // namespace sgf
