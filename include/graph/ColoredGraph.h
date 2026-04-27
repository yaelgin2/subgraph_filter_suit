#pragma once

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
 * - The @p vertex_colors vector must have exactly @p num_vertices entries.
 * - Self-loops (edges where source == destination) are not allowed and throw
 *   InvalidArgumentException.
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
     *              reverse edges are appended for undirected graphs, then the
     *              list is sorted and de-duplicated internally.
     * @param vertex_colors Per-vertex color labels; must have exactly @p num_vertices entries.
     * @param is_directed If true, treat edges as directed.
     */
ColoredGraph(uint32_t num_vertices, std::vector<std::pair<uint32_t, uint32_t>>& edges,
                 const std::vector<uint32_t>& vertex_colors, bool is_directed = false);

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
     *              reverse edges are appended for undirected graphs, then the
     *              list is sorted and de-duplicated internally.
     * @param vertex_colors Per-vertex color labels; must have exactly @p num_vertices entries.
     * @param is_directed If true, treat edges as directed.
     * @throws InvalidArgumentException if any two tuples share the same (source,
     *         destination) but carry different colors.
     */
    ColoredGraph(uint32_t num_vertices,
                 std::vector<std::tuple<uint32_t, uint32_t, uint32_t>>& edges,
                 const std::vector<uint32_t>& vertex_colors, bool is_directed = false);

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
     * @throws InvalidArgumentException if the graph has no edge colors.
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
     *         edge does not exist.
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
     * @throws InvalidArgumentException if the graph has no edge colors.
     */
    uint32_t get_edge_color_at(std::vector<uint32_t>::const_iterator neighbour_it,
                               bool reversed = false) const;

    /**
     * @brief Returns whether the graph is directed.
     * @return True if edges are directed, false if undirected.
     */
    bool is_directed() const;

private:
    /**
     * @brief Validates that the vertex color vector length matches the vertex count.
     * @param vertex_colors The color vector to validate.
     * @param num_vertices Expected number of vertices.
     * @throws InvalidArgumentException if the sizes do not match.
     */
    static void validate_vertex_colors_size(const std::vector<uint32_t>& vertex_colors,
                                            uint32_t num_vertices);

    /**
     * @brief Validates a single edge, throwing if it is illegal.
     *
     * An edge is illegal if it is a self-loop (source == destination) or if
     * either endpoint is out of range (>= num_vertices).
     *
     * @param edge The (source, destination) pair to validate.
     * @param num_vertices The total number of vertices in the graph.
     * @throws InvalidArgumentException if the edge is a self-loop or out of range.
     */
    static void validate_edge(const std::pair<uint32_t, uint32_t>& edge, uint32_t num_vertices);

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
     * @throws InvalidArgumentException if two tuples share the same (source, destination)
     *         but carry different colors.
     */
    static void sort_and_deduplicate(std::vector<std::tuple<uint32_t, uint32_t, uint32_t>>& edges);

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
};

}  // namespace sgf
