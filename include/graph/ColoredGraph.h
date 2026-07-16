#pragma once

#include "LogLevel.h"
#include "LoggerHandler.h"
#include "exceptions/GraphConstructionException.h"
#include "exceptions/InvalidArgumentException.h"

#include <cstdint>
#include <tuple>
#include <vector>

namespace sgf
{

/**
 * @brief Adjacency structure with per-vertex and optional per-edge color labels.
 *
 * Stores an (optionally directed) graph as a CSR-style neighbour list and
 * provides O(degree) edge queries and O(1) neighbour-range access.
 *
 * Construction rules enforced at runtime:
 * - The @p vertex_colors vector must have exactly @p num_vertices entries,
 *   otherwise InvalidArgumentException is thrown (logged at ERROR level first).
 * - Self-loops (edges where source == destination) are not allowed as real
 *   edges: they are silently discarded from the edge list, and a single
 *   WARNING is logged (via the supplied logger) if any were discarded.
 * - An edge referencing a vertex ID >= num_vertices throws
 *   InvalidArgumentException (logged at ERROR level first).
 * - For uncolored graphs, duplicate edges are silently removed.
 * - For edge-colored graphs, exact duplicate tuples are silently removed.
 *   Duplicate (source, destination) pairs with different colors throw
 *   InvalidArgumentException.
 *
 * Edge color support:
 * - Graphs constructed from pair edges carry no edge colors (is_edges_colored() == false).
 * - Graphs constructed from tuple edges carry a per-edge color stored in a vector
 *   parallel to the neighbour list (is_edges_colored() == true).
 * - Calling get_edge_color() or get_edge_color_at() on an uncolored graph throws
 *   InvalidArgumentException.
 */
class ColoredGraph
{
public:
    /**
     * @brief Constructs an uncolored ColoredGraph (edges carry no color labels).
     * @param num_vertices Number of vertices in the graph.
     * @param edges List of (source, destination) pairs. Modified in place:
     *              self-loops are discarded, reverse edges are appended for
     *              undirected graphs, then the list is sorted and de-duplicated
     *              internally.
     * @param vertex_colors Per-vertex color labels; must have exactly @p num_vertices entries.
     * @param is_directed If true, treat edges as directed.
     * @param logger Logger used to report discarded self-loops and thrown errors.
     */
    ColoredGraph(uint32_t num_vertices, std::vector<std::pair<uint32_t, uint32_t>>& edges,
                 const std::vector<uint32_t>& vertex_colors, bool is_directed = false,
                 LoggerHandler logger = LoggerHandler::null());

    /**
     * @brief Constructs an edge-colored ColoredGraph.
     *
     * Each edge is a (source, destination, edge_color) tuple. Exact duplicate
     * tuples are silently removed. Duplicate (source, destination) pairs with
     * different colors throw InvalidArgumentException. Reverse edges appended for
     * undirected graphs inherit the same edge color.
     *
     * @param num_vertices Number of vertices in the graph.
     * @param edges List of (source, destination, edge_color) tuples. Modified in place:
     *              self-loops are discarded, reverse edges are appended for
     *              undirected graphs, then the list is sorted and de-duplicated
     *              internally.
     * @param vertex_colors Per-vertex color labels; must have exactly @p num_vertices entries.
     * @param is_directed If true, treat edges as directed.
     * @param logger Logger used to report discarded self-loops and thrown errors.
     * @throws InvalidArgumentException if any two tuples share the same (source,
     *         destination) but carry different colors.
     */
    ColoredGraph(uint32_t num_vertices,
                 std::vector<std::tuple<uint32_t, uint32_t, uint32_t>>& edges,
                 const std::vector<uint32_t>& vertex_colors, bool is_directed = false,
                 LoggerHandler logger = LoggerHandler::null());

    ColoredGraph(const ColoredGraph&) = default;
    ColoredGraph(ColoredGraph&&) = default;
    ColoredGraph& operator=(const ColoredGraph&) = default;
    ColoredGraph& operator=(ColoredGraph&&) = default;

    /**
     * @brief Default destructor.
     */
    ~ColoredGraph() = default;

    /**
     * @brief Returns iterators over the neighbour IDs of @p vertex.
     * @param vertex The vertex whose neighbours are requested.
     * @param reversed If true and the graph is directed, return in-neighbours
     *                 instead of out-neighbours.
     * @return A pair of const iterators [begin, end) over the neighbour list.
     */
    std::pair<std::vector<uint32_t>::const_iterator, std::vector<uint32_t>::const_iterator>
    get_neighbours(uint32_t vertex, bool reversed = false) const;

    /**
     * @brief Returns iterators over the edge colors parallel to get_neighbours().
     *
     * The returned range is index-aligned with the range from get_neighbours():
     * element i in this range is the color of the edge to neighbour i.
     *
     * @param vertex The vertex whose neighbour edge colors are requested.
     * @param reversed If true and the graph is directed, return colors for
     *                 in-edges instead of out-edges.
     * @return A pair of const iterators [begin, end) over the edge color list.
     * @throws InvalidArgumentException if the graph has no edge colors (logged at
     *         ERROR level before throwing).
     */
    std::pair<std::vector<uint32_t>::const_iterator, std::vector<uint32_t>::const_iterator>
    get_neighbour_edge_colors(uint32_t vertex, bool reversed = false) const;

    /**
     * @brief Checks whether a directed or undirected edge exists.
     * @param source_vertex The source (or first endpoint) of the edge.
     * @param dest_vertex The destination (or second endpoint) of the edge.
     * @return True if the edge exists.
     */
    bool is_edge(uint32_t source_vertex, uint32_t dest_vertex) const;

    /**
     * @brief Returns the number of vertices.
     * @return Vertex count.
     */
    uint32_t vertex_count() const;

    /**
     * @brief Returns the number of edges (without reverse copies).
     * @return Edge count.
     */
    uint32_t edge_count() const;

    /**
     * @brief Returns the number of neighbours of @p vertex in O(1).
     *
     * Reads directly from the CSR index array. For undirected graphs and the
     * forward direction of directed graphs:
     * @code
     * m_index_of_neighbours[vertex + 1] - m_index_of_neighbours[vertex]
     * @endcode
     * For the last vertex, where @c vertex+1 would be out of bounds, the total
     * neighbour array size is used as the end position instead.
     *
     * @param vertex The vertex to query.
     * @param is_reversed If true and the graph is directed, return the in-degree
     *                    (neighbours in the reversed adjacency list) instead of
     *                    the out-degree. Has no effect on undirected graphs.
     * @return Number of neighbours (out-degree, or in-degree when @p is_reversed is true).
     */
    uint32_t get_neighbour_count(uint32_t vertex, bool is_reversed) const;

    /**
     * @brief Returns the color label of @p vertex.
     * @param vertex The vertex to query.
     * @return The vertex color label.
     */
    uint32_t get_vertex_color(uint32_t vertex) const;

    /**
     * @brief Sets the color label of @p vertex.
     * @param vertex The vertex to update.
     * @param new_color The new color label.
     */
    void set_vertex_color(uint32_t vertex, uint32_t new_color);

    /**
     * @brief Returns whether this graph carries per-edge color labels.
     * @return True if constructed from a tuple edge list, false otherwise.
     */
    bool is_edges_colored() const;

    /**
     * @brief Returns the color of the edge from @p source_vertex to @p dest_vertex.
     *
     * For undirected graphs, get_edge_color(u, v) == get_edge_color(v, u).
     *
     * @param source_vertex The source endpoint (or either endpoint for undirected).
     * @param dest_vertex The destination endpoint.
     * @return The edge color.
     * @throws InvalidArgumentException if the graph has no edge colors or the
     *         edge does not exist (logged at ERROR level before throwing).
     */
    uint32_t get_edge_color(uint32_t source_vertex, uint32_t dest_vertex) const;

    /**
     * @brief Returns the edge color for the neighbour pointed to by @p neighbour_it.
     *
     * @p neighbour_it must be a valid non-end iterator obtained from
     * get_neighbours(v, reversed) for the same value of @p reversed passed here.
     * Passing a mismatched iterator yields undefined behaviour.
     *
     * @param neighbour_it A valid non-end iterator from get_neighbours().
     * @param reversed Must match the @p reversed argument used when obtaining the iterator.
     * @return The edge color at that iterator position.
     * @throws InvalidArgumentException if the graph has no edge colors (logged at
     *         ERROR level before throwing).
     */
    uint32_t get_edge_color_at(std::vector<uint32_t>::const_iterator neighbour_it,
                               bool reversed = false) const;

    /**
     * @brief Returns whether the graph is directed.
     * @return True if edges are directed, false if undirected.
     */
    bool is_directed() const;

    /**
     * @brief Returns the out-degree (or total degree for undirected) of @p vertex.
     * @param vertex The vertex to measure.
     * @return Number of out-edges.
     */
    uint32_t out_degree(uint32_t vertex) const;

    /**
     * @brief Returns the in-degree of @p vertex.
     * @param vertex The vertex to measure.
     * @return Number of in-edges (equals out_degree for undirected graphs).
     */
    uint32_t in_degree(uint32_t vertex) const;

private:
    static constexpr uint32_t UNDIRECTED_EDGE_FACTOR = 2U;

    /**
     * @brief Validates that the vertex color vector length matches the vertex count.
     * @param vertex_colors The color vector to validate.
     * @param num_vertices Expected number of vertices.
     * @param logger Logger used to report the error before throwing.
     * @throws InvalidArgumentException if the sizes do not match.
     */
    static void validate_vertex_colors_size(const std::vector<uint32_t>& vertex_colors,
                                            uint32_t num_vertices, const LoggerHandler& logger);

    /**
     * @brief Validates a batch of edges, discarding self-loops and rejecting out-of-range ones.
     *
     * Self-loops (source == destination) are silently discarded from the
     * returned vector; a single WARNING is logged via @p logger if any were
     * discarded. An edge referencing a vertex ID >= num_vertices is illegal
     * and throws InvalidArgumentException (logged at ERROR level first).
     *
     * @tparam EdgeType Edge representation, e.g. std::pair or std::tuple.
     * @tparam SourceGetter Callable returning the source vertex ID for an EdgeType.
     * @tparam DestGetter Callable returning the destination vertex ID for an EdgeType.
     * @param edges The edge list to validate.
     * @param num_vertices The total number of vertices in the graph.
     * @param get_source Accessor returning the source endpoint of an edge.
     * @param get_dest Accessor returning the destination endpoint of an edge.
     * @param logger Logger used to report discarded self-loops and thrown errors.
     * @return A copy of @p edges with self-loops removed.
     * @throws InvalidArgumentException if any edge references a vertex ID >= num_vertices.
     */
    template <typename EdgeType, typename SourceGetter, typename DestGetter>
    static std::vector<EdgeType> validate_edges(const std::vector<EdgeType>& edges,
                                                uint32_t num_vertices, SourceGetter get_source,
                                                DestGetter get_dest, const LoggerHandler& logger);

    /**
     * @brief Dispatches to build_undirected_structures or build_directed_structures.
     *
     * @param num_vertices Number of vertices.
     * @param edges (source, destination, color) tuples; color is 0 for uncolored graphs.
     */
    void build_structures(uint32_t num_vertices,
                          std::vector<std::tuple<uint32_t, uint32_t, uint32_t>>& edges);

    /**
     * @brief Builds CSR structures for an undirected graph.
     *
     * Appends reverse edges (inheriting their colors), sorts and de-duplicates the
     * combined list, then fills m_neighbours, m_index_of_neighbours, m_edge_colors,
     * and m_edge_count.
     *
     * @param num_vertices Number of vertices.
     * @param colored_edges (source, destination, color) tuples; reverse edges are appended.
     */
    void build_undirected_structures(
        uint32_t num_vertices,
        std::vector<std::tuple<uint32_t, uint32_t, uint32_t>>& colored_edges);

    /**
     * @brief Builds CSR structures for a directed graph.
     *
     * Sorts and de-duplicates the forward edges, fills forward and reversed adjacency
     * lists and their parallel edge-color arrays (if colored), plus m_edge_count.
     *
     * @param num_vertices Number of vertices.
     * @param colored_edges (source, destination, color) tuples; color is 0 for uncolored.
     */
    void
    build_directed_structures(uint32_t num_vertices,
                              std::vector<std::tuple<uint32_t, uint32_t, uint32_t>>& colored_edges);

    /**
     * @brief Returns the [begin, end) iterator range for a vertex in any flat uint32_t
     *        CSR array.
     *
     * Used for both neighbour arrays and the parallel edge-color arrays since both
     * are stored as vector<uint32_t> and share the same index_of_neighbours.
     *
     * @param vertex The vertex to look up.
     * @param elements The flat CSR array (neighbours or edge colors).
     * @param index_of_neighbours Index array mapping each vertex to its start in @p elements.
     * @return A pair of const iterators [begin, end) over that vertex's slice.
     */
    static std::pair<std::vector<uint32_t>::const_iterator, std::vector<uint32_t>::const_iterator>
    compute_range(uint32_t vertex, const std::vector<uint32_t>& elements,
                  const std::vector<uint32_t>& index_of_neighbours);

    /**
     * @brief Fills a contiguous range of index_of_neighbours entries with the same value.
     *
     * Used during CSR construction to stamp the current neighbour count into every
     * vertex slot between @p from_vertex (exclusive) and @p to_vertex (inclusive)
     * when no edges originate from those vertices.
     *
     * @param from_vertex First vertex whose slot is NOT written (the last processed vertex).
     * @param to_vertex   Last vertex whose slot IS written.
     * @param neighbour_count The value to store in each slot.
     * @param index_of_neighbours The index array being built.
     */
    static void fill_index_range(uint32_t from_vertex, uint32_t to_vertex, uint32_t neighbour_count,
                                 std::vector<uint32_t>& index_of_neighbours);

    /**
     * @brief Builds CSR neighbour and index arrays from a sorted, de-duplicated edge list.
     *
     * Does not perform sorting or de-duplication — callers are responsible for
     * pre-processing edges before calling this function.
     *
     * @param num_vertices Number of vertices.
     * @param edges Sorted, de-duplicated edge pairs.
     * @param neighbours Output neighbour array filled with destination vertices.
     * @param index_of_neighbours Output index array mapping each vertex to its
     *                            first position in @p neighbours.
     */
    static void initiate_graph(uint32_t num_vertices,
                               const std::vector<std::pair<uint32_t, uint32_t>>& edges,
                               std::vector<uint32_t>& neighbours,
                               std::vector<uint32_t>& index_of_neighbours);

    /**
     * @brief Sorts a tuple edge vector and removes duplicates, throwing on color conflicts.
     *
     * Exact duplicate tuples are silently removed. Duplicate (source, destination) pairs
     * with different colors throw InvalidArgumentException. Uncolored graphs always use
     * color 0, so no conflict is possible and duplicates are silently removed.
     *
     * @param edges The tuple edge vector to sort and de-duplicate in place.
     * @param logger Logger used to report a color conflict before throwing.
     * @throws InvalidArgumentException if two tuples share the same (source, destination)
     *         but carry different colors.
     */
    static void sort_and_deduplicate(std::vector<std::tuple<uint32_t, uint32_t, uint32_t>>& edges,
                                     const LoggerHandler& logger);

    /**
     * @brief Extracts parallel pair and color vectors from a tuple edge vector.
     *
     * @param tuples Source (source, destination, color) tuples.
     * @param pairs Output (source, destination) pairs, resized to match @p tuples.
     * @param colors Output color values; filled only when @p fill_colors is true,
     *               cleared otherwise.
     * @param fill_colors When true, populate @p colors from the tuple color fields.
     *                    When false, @p colors is left empty.
     */
    static void extract_edges(const std::vector<std::tuple<uint32_t, uint32_t, uint32_t>>& tuples,
                              std::vector<std::pair<uint32_t, uint32_t>>& pairs,
                              std::vector<uint32_t>& colors, bool fill_colors);

    /**
     * @brief Converts a pair edge list into a tuple edge list.
     *
     * Each output tuple contains (source, destination, color). When @p reversed is true,
     * source and destination are swapped in every tuple, producing the reverse edge list.
     * Color values are taken from @p colors when non-empty, otherwise default to 0.
     *
     * @param pairs Source (source, destination) pairs.
     * @param colors Parallel color values; may be empty for uncolored graphs.
     * @param reversed If true, swap source and destination in every output tuple.
     * @return A vector of (source, destination, color) tuples.
     */
    static std::vector<std::tuple<uint32_t, uint32_t, uint32_t>>
    to_edge_tuples(const std::vector<std::pair<uint32_t, uint32_t>>& pairs,
                   const std::vector<uint32_t>& colors, bool reversed);

    /**
     * @brief Throws InvalidArgumentException if this graph has no edge colors.
     *
     * Centralizes the "graph has no edge colors" check shared by
     * get_neighbour_edge_colors(), get_edge_color(), and get_edge_color_at().
     * Uncolored graphs (built from the pair-edge constructor) are otherwise
     * fully supported; this check only guards the edge-color accessors.
     *
     * @throws InvalidArgumentException if is_edges_colored() is false (logged
     *         at ERROR level before throwing).
     */
    void ensure_edges_colored() const;

    std::vector<uint32_t> m_neighbours;
    std::vector<uint32_t> m_index_of_neighbours;
    std::vector<uint32_t> m_edge_colors;

    std::vector<uint32_t> m_reversed_neighbours;
    std::vector<uint32_t> m_reversed_index_of_neighbours;
    std::vector<uint32_t> m_reversed_edge_colors;

    std::vector<uint32_t> m_colors;

    uint32_t m_edge_count = 0;
    bool m_directed = false;
    bool m_edges_colored = false;

    LoggerHandler m_logger = LoggerHandler::null();

#ifdef SGF_CUDA_ENABLED
    friend class DeviceGraphBuilder;
#endif
};

template <typename EdgeType, typename SourceGetter, typename DestGetter>
std::vector<EdgeType> ColoredGraph::validate_edges(const std::vector<EdgeType>& edges,
                                                   uint32_t num_vertices, SourceGetter get_source,
                                                   DestGetter get_dest, const LoggerHandler& logger)
{
    std::vector<EdgeType> filtered_edges;
    filtered_edges.reserve(edges.size());
    bool self_loop_discarded = false;
    for (const EdgeType& edge : edges)
    {
        const uint32_t source_vertex = get_source(edge);
        const uint32_t dest_vertex = get_dest(edge);
        if (source_vertex >= num_vertices || dest_vertex >= num_vertices)
        {
            const std::string message =
                "edge (" + std::to_string(source_vertex) + ", " + std::to_string(dest_vertex) +
                ") references a vertex ID >= num_vertices (" + std::to_string(num_vertices) + ")";
            logger.log(LogLevel::ERROR, message);
            throw InvalidArgumentException(message);
        }
        if (source_vertex == dest_vertex)
        {
            self_loop_discarded = true;
            continue;
        }
        filtered_edges.push_back(edge);
    }
    if (self_loop_discarded)
    {
        logger.log(LogLevel::WARNING,
                   "self-loop edge(s) discarded during ColoredGraph construction");
    }
    return filtered_edges;
}

}  // namespace sgf
