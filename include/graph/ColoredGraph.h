#pragma once

#include "exceptions/GraphConstructionException.h"
#include "exceptions/InvalidArgumentException.h"

#include <cstdint>
#include <vector>

namespace sgf
{

/**
 * @brief Adjacency structure with per-vertex color labels.
 *
 * Stores an (optionally directed) graph as a CSR-style neighbour list and
 * provides O(degree) edge queries and O(1) neighbour-range access.
 *
 * Construction rules enforced at runtime:
 * - The @p colors vector must have exactly @p num_vertices entries.
 * - Self-loops (edges where source == destination) are not allowed and throw
 *   InvalidArgumentException.
 * - Duplicate edges are silently removed; both (u,v) and (v,u) supplied to an
 *   undirected graph count as the same edge and are de-duplicated to one.
 */
class ColoredGraph
{
public:
    /**
     * @brief Constructs a ColoredGraph.
     * @param num_vertices Number of vertices in the graph.
     * @param edges List of (source, destination) pairs. Modified in place:
     *              reverse edges are appended for undirected graphs, then the
     *              list is sorted and de-duplicated internally.
     * @param colors Per-vertex color labels; must have exactly @p num_vertices entries.
     * @param is_directed If true, treat edges as directed.
     */
    ColoredGraph(const uint32_t num_vertices, std::vector<std::pair<uint32_t, uint32_t>>& edges,
                 const std::vector<int32_t>& colors, const bool is_directed = false);

    /**
     * @brief Default destructor.
     */
    ~ColoredGraph() = default;

    /**
     * @brief Returns iterators over the neighbours of @p vertex.
     * @param vertex The vertex whose neighbours are requested.
     * @param reversed If true and the graph is directed, return in-neighbours
     *                 instead of out-neighbours.
     * @return A pair of const iterators [begin, end) over the neighbour list.
     */
    std::pair<std::vector<uint32_t>::const_iterator, std::vector<uint32_t>::const_iterator>
    get_neighbours(const uint32_t vertex, const bool reversed = false) const;

    /**
     * @brief Checks whether a directed or undirected edge exists.
     * @param source_vertex The source (or first endpoint) of the edge.
     * @param dest_vertex The destination (or second endpoint) of the edge.
     * @return True if the edge exists.
     */
    bool is_edge(const uint32_t source_vertex, const uint32_t dest_vertex) const;

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
     * @return The color label.
     */
    uint32_t get_vertex_color(const uint32_t vertex) const;

    /**
     * @brief Sets the color label of @p vertex.
     * @param vertex The vertex to update.
     * @param new_color The new color label.
     */
    void set_vertex_color(const uint32_t vertex, const int32_t new_color);

    /**
     * @brief Returns whether the graph is directed.
     * @return True if edges are directed, false if undirected.
     */
    bool is_directed() const;

private:
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
    static void validate_edge(const std::pair<uint32_t, uint32_t>& edge,
                              const uint32_t num_vertices);

    /**
     * @brief Returns the [begin, end) iterator range for a vertex in one adjacency list.
     * @param vertex The vertex to look up.
     * @param neighbours The flat neighbour array.
     * @param index_of_neighbours Index array mapping each vertex to its start in @p neighbours.
     * @return A pair of const iterators [begin, end) over that vertex's neighbours.
     */
    static std::pair<std::vector<uint32_t>::const_iterator, std::vector<uint32_t>::const_iterator>
    compute_neighbour_range(const uint32_t vertex, const std::vector<uint32_t>& neighbours,
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
    static void fill_index_range(const uint32_t from_vertex, const uint32_t to_vertex,
                                 const uint32_t neighbour_count,
                                 std::vector<uint32_t>& index_of_neighbours);

    /**
     * @brief Builds the CSR neighbour list from a sorted, de-duplicated edge list.
     * @param num_vertices Number of vertices.
     * @param edges Edge list; sorted and de-duplicated in place.
     * @param neighbours Output neighbour array filled with destination vertices.
     * @param index_of_neighbours Output index array mapping each vertex to its
     *                            first position in @p neighbours.
     */
    static void initiate_graph(const uint32_t num_vertices,
                               std::vector<std::pair<uint32_t, uint32_t>>& edges,
                               std::vector<uint32_t>& neighbours,
                               std::vector<uint32_t>& index_of_neighbours);

    std::vector<uint32_t> m_neighbours;
    std::vector<uint32_t> m_index_of_neighbours;

    std::vector<uint32_t> m_reversed_neighbours;
    std::vector<uint32_t> m_reversed_index_of_neighbours;

    std::vector<int32_t> m_colors;
    uint32_t m_edge_count = 0;
    bool m_directed = false;
};

}  // namespace sgf
