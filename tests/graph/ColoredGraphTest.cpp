#include "graph/ColoredGraph.h"

#include "exceptions/InvalidArgumentException.h"

#include <cstdint>
#include <gtest/gtest.h>
#include <tuple>
#include <utility>
#include <vector>

using namespace sgf;

// ── Shared assertion helpers ──────────────────────────────────────────────────

namespace
{

/**
 * @brief Asserts that the neighbour list of @p vertex equals @p expected.
 *
 * Neighbours are returned in sorted order by the CSR structure, so @p expected
 * must also be sorted ascending.
 *
 * @param graph The graph under test.
 * @param vertex Vertex whose neighbours are checked.
 * @param expected Sorted list of expected neighbour IDs.
 * @param reversed If true, checks in-neighbours (directed graphs only).
 */
void assert_neighbours(const ColoredGraph& graph, const uint32_t vertex,
                       const std::vector<uint32_t>& expected, const bool reversed = false)
{
    const std::pair<std::vector<uint32_t>::const_iterator, std::vector<uint32_t>::const_iterator>
        range = graph.get_neighbours(vertex, reversed);
    const std::vector<uint32_t> actual(range.first, range.second);
    EXPECT_EQ(actual, expected) << "vertex " << vertex << (reversed ? " (reversed)" : "");
}

/**
 * @brief Asserts that the edge-color list parallel to the neighbour list of @p vertex
 *        equals @p expected.
 *
 * The order of colors matches the order returned by get_neighbours(), which is sorted
 * ascending by neighbour ID.
 *
 * @param graph The graph under test.
 * @param vertex Vertex whose edge colors are checked.
 * @param expected Edge colors in the same order as get_neighbours() output.
 * @param reversed If true, checks in-edge colors (directed graphs only).
 */
void assert_edge_colors(const ColoredGraph& graph, const uint32_t vertex,
                        const std::vector<uint32_t>& expected, const bool reversed = false)
{
    const std::pair<std::vector<uint32_t>::const_iterator, std::vector<uint32_t>::const_iterator>
        range = graph.get_neighbour_edge_colors(vertex, reversed);
    const std::vector<uint32_t> actual(range.first, range.second);
    EXPECT_EQ(actual, expected) << "edge colors for vertex " << vertex
                                << (reversed ? " (reversed)" : "");
}

}  // namespace

// ── Fixture ───────────────────────────────────────────────────────────────────

/**
 * @brief Test fixture for ColoredGraph unit tests.
 *
 * Provides factory helpers that build common edge and color vectors so
 * individual tests stay concise and focused on assertions.
 */
class ColoredGraphTest : public ::testing::Test
{
protected:
    /**
     * @brief Builds a uniform color vector of length @p count, all set to @p color.
     * @param count Number of vertices.
     * @param color Color value to assign to every vertex.
     * @return Color vector suitable for passing to ColoredGraph.
     */
    static std::vector<int32_t> uniform_colors(const uint32_t count, const int32_t color = 0)
    {
        return std::vector<int32_t>(count, color);
    }

    /**
     * @brief Builds a color vector where vertex @p idx has color equal to its index.
     *
     * Useful for tests that verify each vertex carries a distinct, identifiable color.
     *
     * @param count Number of vertices.
     * @return Color vector {0, 1, 2, ..., count-1}.
     */
    static std::vector<int32_t> indexed_colors(const uint32_t count)
    {
        std::vector<int32_t> colors;
        colors.reserve(count);
        for (uint32_t idx = 0; idx < count; ++idx)
        {
            colors.push_back(static_cast<int32_t>(idx));
        }
        return colors;
    }
};

// ── Exception tests ───────────────────────────────────────────────────────────

/**
 * @brief Passing an empty color vector when num_vertices > 0 must throw.
 */
TEST_F(ColoredGraphTest, empty_colors_throws_invalid_argument)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    const std::vector<int32_t> colors;
    EXPECT_THROW(ColoredGraph(1, edges, colors), InvalidArgumentException);
}

/**
 * @brief Passing a non-empty color vector when num_vertices is zero must throw.
 */
TEST_F(ColoredGraphTest, nonzero_colors_with_zero_vertices_throws_invalid_argument)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    const std::vector<int32_t> colors = {0};  // 1 color for 0 vertices
    EXPECT_THROW(ColoredGraph(0, edges, colors), InvalidArgumentException);
}

/**
 * @brief Passing more colors than vertices must throw.
 */
TEST_F(ColoredGraphTest, colors_longer_than_vertex_count_throws_invalid_argument)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    const std::vector<int32_t> colors = {0, 0, 0};  // 3 colors for 2 vertices
    EXPECT_THROW(ColoredGraph(2, edges, colors), InvalidArgumentException);
}

/**
 * @brief Passing fewer colors than vertices must throw.
 */
TEST_F(ColoredGraphTest, colors_shorter_than_vertex_count_throws_invalid_argument)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    const std::vector<int32_t> colors = {0, 0};  // only 2 colors for 3 vertices
    EXPECT_THROW(ColoredGraph(3, edges, colors), InvalidArgumentException);
}

/**
 * @brief An edge whose source vertex ID equals num_vertices must throw.
 */
TEST_F(ColoredGraphTest, edge_with_out_of_range_source_throws_invalid_argument)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{2, 0}};  // vertex 2 invalid for size 2
    const std::vector<int32_t> colors = uniform_colors(2);
    EXPECT_THROW(ColoredGraph(2, edges, colors), InvalidArgumentException);
}

/**
 * @brief An edge whose destination vertex ID equals num_vertices must throw.
 */
TEST_F(ColoredGraphTest, edge_with_out_of_range_dest_throws_invalid_argument)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0, 2}};  // vertex 2 invalid for size 2
    const std::vector<int32_t> colors = uniform_colors(2);
    EXPECT_THROW(ColoredGraph(2, edges, colors), InvalidArgumentException);
}

// ── Color tests ───────────────────────────────────────────────────────────────

/**
 * @brief Colors passed at construction are stored and retrievable per vertex.
 */
TEST_F(ColoredGraphTest, colors_stored_correctly_per_vertex)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    const ColoredGraph graph(3, edges, indexed_colors(3));

    EXPECT_EQ(graph.get_vertex_color(0), 0U);
    EXPECT_EQ(graph.get_vertex_color(1), 1U);
    EXPECT_EQ(graph.get_vertex_color(2), 2U);
}

/**
 * @brief set_vertex_color updates the stored color and is reflected by get_vertex_color.
 */
TEST_F(ColoredGraphTest, set_vertex_color_updates_color)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    const std::vector<int32_t> colors = uniform_colors(1, 0);
    ColoredGraph graph(1, edges, colors);

    EXPECT_EQ(graph.get_vertex_color(0), 0U);
    graph.set_vertex_color(0, 42);
    EXPECT_EQ(graph.get_vertex_color(0), 42U);
    EXPECT_EQ(graph.vertex_count(), 1U);
    EXPECT_EQ(graph.edge_count(), 0U);
    assert_neighbours(graph, 0, {});
}

// ── is_edge tests ─────────────────────────────────────────────────────────────

/**
 * @brief Undirected path 0-1-2: existing edges are found, non-existent are not.
 */
TEST_F(ColoredGraphTest, is_edge_undirected_path_graph)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0, 1}, {1, 2}};
    const std::vector<int32_t> colors = uniform_colors(3);
    const ColoredGraph graph(3, edges, colors, false);

    EXPECT_TRUE(graph.is_edge(0, 1));
    EXPECT_TRUE(graph.is_edge(1, 0));  // undirected: both directions
    EXPECT_TRUE(graph.is_edge(1, 2));
    EXPECT_TRUE(graph.is_edge(2, 1));
    EXPECT_FALSE(graph.is_edge(0, 2));
    EXPECT_FALSE(graph.is_edge(2, 0));
}

/**
 * @brief Directed graph 0->1<-2: only declared directed edges exist.
 */
TEST_F(ColoredGraphTest, is_edge_directed_converging_graph)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0, 1}, {2, 1}};
    const std::vector<int32_t> colors = uniform_colors(3);
    const ColoredGraph graph(3, edges, colors, true);

    EXPECT_TRUE(graph.is_edge(0, 1));
    EXPECT_TRUE(graph.is_edge(2, 1));
    EXPECT_FALSE(graph.is_edge(1, 0));  // reverse of declared edge
    EXPECT_FALSE(graph.is_edge(1, 2));  // reverse of declared edge
    EXPECT_FALSE(graph.is_edge(0, 2));  // non-existent
    EXPECT_FALSE(graph.is_edge(2, 0));  // non-existent
}

// ── Undirected graph tests ────────────────────────────────────────────────────

/**
 * @brief An undirected graph with zero vertices and zero edges is valid.
 */
TEST_F(ColoredGraphTest, undirected_empty_graph)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    const std::vector<int32_t> colors;
    const ColoredGraph graph(0, edges, colors, false);

    EXPECT_EQ(graph.vertex_count(), 0U);
    EXPECT_EQ(graph.edge_count(), 0U);
}

/**
 * @brief An undirected graph with one vertex and no edges has an empty neighbour list.
 */
TEST_F(ColoredGraphTest, undirected_single_vertex_no_edges)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    const ColoredGraph graph(1, edges, indexed_colors(1), false);

    EXPECT_EQ(graph.vertex_count(), 1U);
    EXPECT_EQ(graph.edge_count(), 0U);
    EXPECT_EQ(graph.get_vertex_color(0), 0U);
    assert_neighbours(graph, 0, {});
}

/**
 * @brief An undirected graph with two vertices and no edges has two isolated vertices.
 */
TEST_F(ColoredGraphTest, undirected_two_vertices_no_edges)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    const ColoredGraph graph(2, edges, indexed_colors(2), false);

    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_EQ(graph.edge_count(), 0U);
    assert_neighbours(graph, 0, {});
    assert_neighbours(graph, 1, {});
}

/**
 * @brief An undirected graph with two vertices and one edge has symmetric neighbours.
 */
TEST_F(ColoredGraphTest, undirected_two_vertices_one_edge)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0, 1}};
    const ColoredGraph graph(2, edges, indexed_colors(2), false);

    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_EQ(graph.edge_count(), 1U);
    EXPECT_EQ(graph.get_vertex_color(0), 0U);
    EXPECT_EQ(graph.get_vertex_color(1), 1U);
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {0});
}

/**
 * @brief Three vertices, one edge: the disconnected vertex has an empty neighbour list.
 */
TEST_F(ColoredGraphTest, undirected_three_vertices_one_edge_disconnected_vertex)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0, 1}};
    const ColoredGraph graph(3, edges, indexed_colors(3), false);

    EXPECT_EQ(graph.vertex_count(), 3U);
    EXPECT_EQ(graph.edge_count(), 1U);
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {0});
    assert_neighbours(graph, 2, {});  // vertex 2 is isolated
}

/**
 * @brief An undirected triangle has every vertex connected to both others.
 */
TEST_F(ColoredGraphTest, undirected_triangle_graph)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0, 1}, {1, 2}, {0, 2}};
    const ColoredGraph graph(3, edges, indexed_colors(3), false);

    EXPECT_EQ(graph.vertex_count(), 3U);
    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_EQ(graph.get_vertex_color(0), 0U);
    EXPECT_EQ(graph.get_vertex_color(1), 1U);
    EXPECT_EQ(graph.get_vertex_color(2), 2U);
    assert_neighbours(graph, 0, {1, 2});
    assert_neighbours(graph, 1, {0, 2});
    assert_neighbours(graph, 2, {0, 1});
}

/**
 * @brief A duplicate edge in the same direction is silently de-duplicated.
 *
 * After de-duplication the graph must behave identically to one built with a
 * single copy of the edge.  edge_count() must reflect the unique edge count.
 */
TEST_F(ColoredGraphTest, undirected_duplicate_edge_same_direction_is_removed)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0, 1}, {0, 1}};
    const ColoredGraph graph(2, edges, indexed_colors(2), false);

    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_EQ(graph.edge_count(), 1U);  // duplicate removed
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {0});
}

/**
 * @brief Providing both (0,1) and (1,0) for an undirected graph is treated as one edge.
 *
 * Undirected graphs store both directions internally; the caller supplying both
 * directions explicitly must result in the same graph as supplying one.
 */
TEST_F(ColoredGraphTest, undirected_duplicate_edge_opposite_directions_is_removed)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0, 1}, {1, 0}};
    const ColoredGraph graph(2, edges, indexed_colors(2), false);

    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_EQ(graph.edge_count(), 1U);  // (0,1) and (1,0) are the same undirected edge
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {0});
}

/**
 * @brief Undirected star: vertex 0 connected to every other vertex.
 *
 * Checks that the hub has all spokes as neighbours and each spoke has only the hub.
 */
TEST_F(ColoredGraphTest, undirected_star_graph)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0, 1}, {0, 2}, {0, 3}, {0, 4}};
    const ColoredGraph graph(5, edges, indexed_colors(5), false);

    EXPECT_EQ(graph.vertex_count(), 5U);
    EXPECT_EQ(graph.edge_count(), 4U);
    assert_neighbours(graph, 0, {1, 2, 3, 4});  // hub
    assert_neighbours(graph, 1, {0});
    assert_neighbours(graph, 2, {0});
    assert_neighbours(graph, 3, {0});
    assert_neighbours(graph, 4, {0});
}

// ── Directed graph tests ──────────────────────────────────────────────────────

/**
 * @brief A directed graph with zero vertices and zero edges is valid.
 */
TEST_F(ColoredGraphTest, directed_empty_graph)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    const std::vector<int32_t> colors;
    const ColoredGraph graph(0, edges, colors, true);

    EXPECT_EQ(graph.vertex_count(), 0U);
    EXPECT_EQ(graph.edge_count(), 0U);
}

/**
 * @brief A directed graph with one vertex has empty forward and reverse neighbour lists.
 */
TEST_F(ColoredGraphTest, directed_single_vertex_no_edges)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    const ColoredGraph graph(1, edges, indexed_colors(1), true);

    EXPECT_EQ(graph.vertex_count(), 1U);
    EXPECT_EQ(graph.edge_count(), 0U);
    assert_neighbours(graph, 0, {}, false);  // out-neighbours
    assert_neighbours(graph, 0, {}, true);   // in-neighbours
}

/**
 * @brief A directed graph with two isolated vertices has no neighbours in either direction.
 */
TEST_F(ColoredGraphTest, directed_two_vertices_no_edges)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    const ColoredGraph graph(2, edges, indexed_colors(2), true);

    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_EQ(graph.edge_count(), 0U);
    assert_neighbours(graph, 0, {}, false);
    assert_neighbours(graph, 1, {}, false);
    assert_neighbours(graph, 0, {}, true);
    assert_neighbours(graph, 1, {}, true);
}

/**
 * @brief Directed edge 0->1: out-neighbours and in-neighbours are correct.
 */
TEST_F(ColoredGraphTest, directed_two_vertices_forward_edge)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0, 1}};
    const ColoredGraph graph(2, edges, indexed_colors(2), true);

    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_EQ(graph.edge_count(), 1U);
    assert_neighbours(graph, 0, {1}, false);  // 0 has out-neighbour 1
    assert_neighbours(graph, 1, {}, false);   // 1 has no out-neighbours
    assert_neighbours(graph, 0, {}, true);    // 0 has no in-neighbours
    assert_neighbours(graph, 1, {0}, true);   // 1 has in-neighbour 0
}

/**
 * @brief Two identical directed edges 0->1 are de-duplicated to one.
 */
TEST_F(ColoredGraphTest, directed_duplicate_forward_edge_is_removed)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0, 1}, {0, 1}};
    const ColoredGraph graph(2, edges, indexed_colors(2), true);

    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_EQ(graph.edge_count(), 1U);  // duplicate removed
    assert_neighbours(graph, 0, {1}, false);
    assert_neighbours(graph, 1, {}, false);  // 1 has no out-neighbours
    assert_neighbours(graph, 0, {}, true);   // 0 has no in-neighbours
    assert_neighbours(graph, 1, {0}, true);
}

/**
 * @brief Directed edge 1->0: out-neighbours and in-neighbours are correct.
 */
TEST_F(ColoredGraphTest, directed_two_vertices_backward_edge)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{1, 0}};
    const ColoredGraph graph(2, edges, indexed_colors(2), true);

    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_EQ(graph.edge_count(), 1U);
    assert_neighbours(graph, 1, {0}, false);  // 1 has out-neighbour 0
    assert_neighbours(graph, 0, {}, false);   // 0 has no out-neighbours
    assert_neighbours(graph, 1, {}, true);    // 1 has no in-neighbours
    assert_neighbours(graph, 0, {1}, true);   // 0 has in-neighbour 1
}

/**
 * @brief Two identical directed edges 1->0 are de-duplicated to one.
 */
TEST_F(ColoredGraphTest, directed_duplicate_backward_edge_is_removed)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{1, 0}, {1, 0}};
    const ColoredGraph graph(2, edges, indexed_colors(2), true);

    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_EQ(graph.edge_count(), 1U);  // duplicate removed
    assert_neighbours(graph, 1, {0}, false);
    assert_neighbours(graph, 0, {1}, true);
}

/**
 * @brief Complete directed graph on 3 vertices: every vertex has 2 out- and 2 in-neighbours.
 *
 * All 6 directed edges are present (both directions for every pair), so
 * edge_count() must equal 6.
 */
TEST_F(ColoredGraphTest, directed_triangle_graph)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0, 1}, {1, 0}, {1, 2},
                                                        {2, 1}, {0, 2}, {2, 0}};
    const ColoredGraph graph(3, edges, indexed_colors(3), true);

    EXPECT_EQ(graph.vertex_count(), 3U);
    EXPECT_EQ(graph.edge_count(), 6U);
    EXPECT_EQ(graph.get_vertex_color(0), 0U);
    EXPECT_EQ(graph.get_vertex_color(1), 1U);
    EXPECT_EQ(graph.get_vertex_color(2), 2U);
    assert_neighbours(graph, 0, {1, 2}, false);
    assert_neighbours(graph, 1, {0, 2}, false);
    assert_neighbours(graph, 2, {0, 1}, false);
    assert_neighbours(graph, 0, {1, 2}, true);
    assert_neighbours(graph, 1, {0, 2}, true);
    assert_neighbours(graph, 2, {0, 1}, true);
}

/**
 * @brief Both 0->1 and 1->0 are present: two distinct directed edges are kept.
 */
TEST_F(ColoredGraphTest, directed_two_vertices_bidirectional_edges)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0, 1}, {1, 0}};
    const ColoredGraph graph(2, edges, indexed_colors(2), true);

    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_EQ(graph.edge_count(), 2U);
    assert_neighbours(graph, 0, {1}, false);
    assert_neighbours(graph, 1, {0}, false);
    assert_neighbours(graph, 0, {1}, true);
    assert_neighbours(graph, 1, {0}, true);
}

/**
 * @brief Directed out-star: hub 0 points to all spokes; spokes have no out-neighbours.
 *
 * Checks forward (out) and reverse (in) neighbours for every vertex.
 */
TEST_F(ColoredGraphTest, directed_out_star_graph)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0, 1}, {0, 2}, {0, 3}, {0, 4}};
    const ColoredGraph graph(5, edges, indexed_colors(5), true);

    EXPECT_EQ(graph.vertex_count(), 5U);
    EXPECT_EQ(graph.edge_count(), 4U);
    assert_neighbours(graph, 0, {1, 2, 3, 4}, false);  // hub: all out-neighbours
    assert_neighbours(graph, 1, {}, false);
    assert_neighbours(graph, 2, {}, false);
    assert_neighbours(graph, 3, {}, false);
    assert_neighbours(graph, 4, {}, false);
    assert_neighbours(graph, 0, {}, true);  // hub: no in-neighbours
    assert_neighbours(graph, 1, {0}, true);
    assert_neighbours(graph, 2, {0}, true);
    assert_neighbours(graph, 3, {0}, true);
    assert_neighbours(graph, 4, {0}, true);
}

/**
 * @brief Directed in-star: all spokes point to hub 0; hub has no out-neighbours.
 *
 * Checks forward (out) and reverse (in) neighbours for every vertex.
 */
TEST_F(ColoredGraphTest, directed_in_star_graph)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{1, 0}, {2, 0}, {3, 0}, {4, 0}};
    const ColoredGraph graph(5, edges, indexed_colors(5), true);

    EXPECT_EQ(graph.vertex_count(), 5U);
    EXPECT_EQ(graph.edge_count(), 4U);
    assert_neighbours(graph, 0, {}, false);  // hub: no out-neighbours
    assert_neighbours(graph, 1, {0}, false);
    assert_neighbours(graph, 2, {0}, false);
    assert_neighbours(graph, 3, {0}, false);
    assert_neighbours(graph, 4, {0}, false);
    assert_neighbours(graph, 0, {1, 2, 3, 4}, true);  // hub: all in-neighbours
    assert_neighbours(graph, 1, {}, true);
    assert_neighbours(graph, 2, {}, true);
    assert_neighbours(graph, 3, {}, true);
    assert_neighbours(graph, 4, {}, true);
}

// ── Self-loop tests ───────────────────────────────────────────────────────────

/**
 * @brief A self-loop on an undirected graph must throw InvalidArgumentException.
 */
TEST_F(ColoredGraphTest, self_loop_on_undirected_graph_throws)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0, 0}};
    const std::vector<int32_t> colors = uniform_colors(1);
    EXPECT_THROW(ColoredGraph(1, edges, colors, false), InvalidArgumentException);
}

/**
 * @brief A self-loop on a directed graph must throw InvalidArgumentException.
 */
TEST_F(ColoredGraphTest, self_loop_on_directed_graph_throws)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0, 0}};
    const std::vector<int32_t> colors = uniform_colors(1);
    EXPECT_THROW(ColoredGraph(1, edges, colors, true), InvalidArgumentException);
}

// ── get_neighbours with reversed=true on undirected graphs ────────────────────

/**
 * @brief On an undirected graph, reversed=true returns the same neighbours as reversed=false.
 *
 * The reversed flag only applies to directed graphs. For undirected graphs the
 * directed flag is false, so the reversed branch is skipped and the forward
 * adjacency list is returned regardless of the reversed argument.
 */
TEST_F(ColoredGraphTest, undirected_reversed_neighbours_equal_forward_neighbours)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0, 1}, {1, 2}, {0, 2}};
    const ColoredGraph graph(3, edges, uniform_colors(3), false);

    for (const uint32_t vertex : {0U, 1U, 2U})
    {
        const std::pair<std::vector<uint32_t>::const_iterator,
                        std::vector<uint32_t>::const_iterator>
            forward = graph.get_neighbours(vertex, false);
        const std::pair<std::vector<uint32_t>::const_iterator,
                        std::vector<uint32_t>::const_iterator>
            reversed = graph.get_neighbours(vertex, true);

        const std::vector<uint32_t> forward_list(forward.first, forward.second);
        const std::vector<uint32_t> reversed_list(reversed.first, reversed.second);
        EXPECT_EQ(forward_list, reversed_list) << "vertex " << vertex;
    }
}

// ── Colored edge — is_edges_colored flag ─────────────────────────────────────

/**
 * @brief is_edges_colored() returns false for a graph built from pair edges.
 */
TEST_F(ColoredGraphTest, uncolored_graph_is_not_edges_colored)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0, 1}};
    const ColoredGraph graph(2, edges, uniform_colors(2));
    EXPECT_FALSE(graph.is_edges_colored());
}

/**
 * @brief is_edges_colored() returns true for a graph built from tuple edges.
 */
TEST_F(ColoredGraphTest, colored_graph_is_edges_colored)
{
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> edges = {{0, 1, 7}};
    const ColoredGraph graph(2, edges, uniform_colors(2));
    EXPECT_TRUE(graph.is_edges_colored());
}

/**
 * @brief get_edge_color() throws on a graph built from pair edges.
 */
TEST_F(ColoredGraphTest, get_edge_color_on_uncolored_graph_throws)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0, 1}};
    const ColoredGraph graph(2, edges, uniform_colors(2));
    EXPECT_THROW(graph.get_edge_color(0, 1), InvalidArgumentException);
}

/**
 * @brief get_neighbour_edge_colors() throws on a graph built from pair edges.
 */
TEST_F(ColoredGraphTest, get_neighbour_edge_colors_on_uncolored_graph_throws)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0, 1}};
    const ColoredGraph graph(2, edges, uniform_colors(2));
    EXPECT_THROW(graph.get_neighbour_edge_colors(0), InvalidArgumentException);
}

// ── Colored edge — undirected ─────────────────────────────────────────────────

/**
 * @brief An edge-colored undirected graph with zero vertices and zero edges is valid.
 */
TEST_F(ColoredGraphTest, edge_colored_undirected_empty_graph)
{
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> edges;
    const ColoredGraph graph(0, edges, {});

    EXPECT_TRUE(graph.is_edges_colored());
    EXPECT_EQ(graph.vertex_count(), 0U);
    EXPECT_EQ(graph.edge_count(), 0U);
}

/**
 * @brief Edge-colored undirected graph: one vertex, no edges.
 */
TEST_F(ColoredGraphTest, edge_colored_undirected_one_vertex_no_edges)
{
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> edges;
    const ColoredGraph graph(1, edges, uniform_colors(1));

    EXPECT_TRUE(graph.is_edges_colored());
    EXPECT_EQ(graph.vertex_count(), 1U);
    EXPECT_EQ(graph.edge_count(), 0U);
    assert_neighbours(graph, 0, {});
}

/**
 * @brief Edge-colored undirected graph: two vertices, no edges.
 */
TEST_F(ColoredGraphTest, edge_colored_undirected_two_vertices_no_edges)
{
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> edges;
    const ColoredGraph graph(2, edges, uniform_colors(2));

    EXPECT_TRUE(graph.is_edges_colored());
    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_EQ(graph.edge_count(), 0U);
    assert_neighbours(graph, 0, {});
    assert_neighbours(graph, 1, {});
}

/**
 * @brief Edge-colored undirected graph: two vertices, one edge with color 5.
 *
 * Both endpoints must report the edge color; the reverse direction inherits the
 * same color.
 */
TEST_F(ColoredGraphTest, edge_colored_undirected_two_vertices_one_edge)
{
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> edges = {{0, 1, 5}};
    const ColoredGraph graph(2, edges, uniform_colors(2));

    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_EQ(graph.edge_count(), 1U);
    EXPECT_EQ(graph.get_edge_color(0, 1), 5U);
    EXPECT_EQ(graph.get_edge_color(1, 0), 5U);
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {0});
    assert_edge_colors(graph, 0, {5});
    assert_edge_colors(graph, 1, {5});
}

/**
 * @brief Edge-colored undirected triangle: all three edges share the same color.
 */
TEST_F(ColoredGraphTest, edge_colored_undirected_triangle_same_color)
{
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> edges = {{0, 1, 7}, {1, 2, 7}, {0, 2, 7}};
    const ColoredGraph graph(3, edges, uniform_colors(3));

    EXPECT_EQ(graph.edge_count(), 3U);
    assert_neighbours(graph, 0, {1, 2});
    assert_neighbours(graph, 1, {0, 2});
    assert_neighbours(graph, 2, {0, 1});
    assert_edge_colors(graph, 0, {7, 7});
    assert_edge_colors(graph, 1, {7, 7});
    assert_edge_colors(graph, 2, {7, 7});
}

/**
 * @brief Edge-colored undirected triangle: each edge has a distinct color.
 *
 * Edge (0,1) has color 1, (0,2) has color 2, (1,2) has color 3.
 * Neighbors are sorted ascending, so edge-color order follows neighbor order.
 */
TEST_F(ColoredGraphTest, edge_colored_undirected_triangle_different_colors)
{
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> edges = {{0, 1, 1}, {1, 2, 3}, {0, 2, 2}};
    const ColoredGraph graph(3, edges, uniform_colors(3));

    EXPECT_EQ(graph.edge_count(), 3U);
    // vertex 0: neighbors [1, 2], colors [1, 2]
    assert_neighbours(graph, 0, {1, 2});
    assert_edge_colors(graph, 0, {1, 2});
    // vertex 1: neighbors [0, 2], colors [1, 3]
    assert_neighbours(graph, 1, {0, 2});
    assert_edge_colors(graph, 1, {1, 3});
    // vertex 2: neighbors [0, 1], colors [2, 3]
    assert_neighbours(graph, 2, {0, 1});
    assert_edge_colors(graph, 2, {2, 3});
}

/**
 * @brief Exact duplicate edge-colored undirected edge is silently de-duplicated.
 */
TEST_F(ColoredGraphTest, edge_colored_undirected_duplicate_edge_same_color)
{
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> edges = {{0, 1, 5}, {0, 1, 5}};
    const ColoredGraph graph(2, edges, uniform_colors(2));

    EXPECT_EQ(graph.edge_count(), 1U);
    assert_neighbours(graph, 0, {1});
    assert_edge_colors(graph, 0, {5});
}

/**
 * @brief Duplicate undirected edge with conflicting colors must throw.
 */
TEST_F(ColoredGraphTest, edge_colored_undirected_duplicate_edge_different_color_throws)
{
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> edges = {{0, 1, 5}, {0, 1, 6}};
    EXPECT_THROW(ColoredGraph(2, edges, uniform_colors(2)), InvalidArgumentException);
}

// ── Colored edge — directed ───────────────────────────────────────────────────

/**
 * @brief Edge-colored directed graph: one vertex, no edges.
 */
TEST_F(ColoredGraphTest, edge_colored_directed_one_vertex_no_edges)
{
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> edges;
    const ColoredGraph graph(1, edges, uniform_colors(1), true);

    EXPECT_TRUE(graph.is_edges_colored());
    EXPECT_EQ(graph.vertex_count(), 1U);
    EXPECT_EQ(graph.edge_count(), 0U);
    assert_neighbours(graph, 0, {}, false);
    assert_neighbours(graph, 0, {}, true);
}

/**
 * @brief Edge-colored directed graph: two vertices, no edges.
 */
TEST_F(ColoredGraphTest, edge_colored_directed_two_vertices_no_edges)
{
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> edges;
    const ColoredGraph graph(2, edges, uniform_colors(2), true);

    EXPECT_EQ(graph.edge_count(), 0U);
    assert_neighbours(graph, 0, {}, false);
    assert_neighbours(graph, 1, {}, false);
    assert_neighbours(graph, 0, {}, true);
    assert_neighbours(graph, 1, {}, true);
}

/**
 * @brief Edge-colored directed graph: one edge 0->1 with color 3.
 *
 * The in-edge color at vertex 1 must also be 3 (same physical edge).
 */
TEST_F(ColoredGraphTest, edge_colored_directed_two_vertices_one_edge)
{
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> edges = {{0, 1, 3}};
    const ColoredGraph graph(2, edges, uniform_colors(2), true);

    EXPECT_EQ(graph.edge_count(), 1U);
    EXPECT_EQ(graph.get_edge_color(0, 1), 3U);
    assert_neighbours(graph, 0, {1}, false);
    assert_neighbours(graph, 1, {}, false);
    assert_neighbours(graph, 0, {}, true);
    assert_neighbours(graph, 1, {0}, true);
    assert_edge_colors(graph, 0, {3}, false);
    assert_edge_colors(graph, 1, {3}, true);
}

/**
 * @brief Edge-colored directed triangle: all edges share the same color.
 */
TEST_F(ColoredGraphTest, edge_colored_directed_triangle_same_color)
{
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> edges = {{0, 1, 4}, {1, 2, 4}, {2, 0, 4}};
    const ColoredGraph graph(3, edges, uniform_colors(3), true);

    EXPECT_EQ(graph.edge_count(), 3U);
    assert_neighbours(graph, 0, {1}, false);
    assert_neighbours(graph, 1, {2}, false);
    assert_neighbours(graph, 2, {0}, false);
    assert_edge_colors(graph, 0, {4}, false);
    assert_edge_colors(graph, 1, {4}, false);
    assert_edge_colors(graph, 2, {4}, false);
    assert_neighbours(graph, 0, {2}, true);
    assert_neighbours(graph, 1, {0}, true);
    assert_neighbours(graph, 2, {1}, true);
    assert_edge_colors(graph, 0, {4}, true);
    assert_edge_colors(graph, 1, {4}, true);
    assert_edge_colors(graph, 2, {4}, true);
}

/**
 * @brief Edge-colored directed triangle: each edge has a distinct color.
 *
 * Edges: 0->1 color 1, 1->2 color 2, 2->0 color 3.
 */
TEST_F(ColoredGraphTest, edge_colored_directed_triangle_different_colors)
{
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> edges = {{0, 1, 1}, {1, 2, 2}, {2, 0, 3}};
    const ColoredGraph graph(3, edges, uniform_colors(3), true);

    EXPECT_EQ(graph.edge_count(), 3U);
    assert_edge_colors(graph, 0, {1}, false);
    assert_edge_colors(graph, 1, {2}, false);
    assert_edge_colors(graph, 2, {3}, false);
    assert_edge_colors(graph, 0, {3}, true);
    assert_edge_colors(graph, 1, {1}, true);
    assert_edge_colors(graph, 2, {2}, true);
}

/**
 * @brief Duplicate directed edge in same direction with same color is de-duplicated.
 */
TEST_F(ColoredGraphTest, edge_colored_directed_duplicate_same_direction_same_color)
{
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> edges = {{0, 1, 9}, {0, 1, 9}};
    const ColoredGraph graph(2, edges, uniform_colors(2), true);

    EXPECT_EQ(graph.edge_count(), 1U);
    assert_neighbours(graph, 0, {1}, false);
    assert_edge_colors(graph, 0, {9}, false);
}

/**
 * @brief Duplicate directed edge in same direction with different colors must throw.
 */
TEST_F(ColoredGraphTest, edge_colored_directed_duplicate_same_direction_different_color_throws)
{
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> edges = {{0, 1, 9}, {0, 1, 8}};
    EXPECT_THROW(ColoredGraph(2, edges, uniform_colors(2), true), InvalidArgumentException);
}

/**
 * @brief 0->1 and 1->0 with the same color are two distinct directed edges, both kept.
 */
TEST_F(ColoredGraphTest, edge_colored_directed_duplicate_different_directions_same_color)
{
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> edges = {{0, 1, 5}, {1, 0, 5}};
    const ColoredGraph graph(2, edges, uniform_colors(2), true);

    EXPECT_EQ(graph.edge_count(), 2U);
    assert_edge_colors(graph, 0, {5}, false);
    assert_edge_colors(graph, 1, {5}, false);
    assert_edge_colors(graph, 0, {5}, true);
    assert_edge_colors(graph, 1, {5}, true);
}

/**
 * @brief 0->1 and 1->0 with different colors are two distinct directed edges, both kept.
 */
TEST_F(ColoredGraphTest, edge_colored_directed_duplicate_different_directions_different_color)
{
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> edges = {{0, 1, 5}, {1, 0, 6}};
    const ColoredGraph graph(2, edges, uniform_colors(2), true);

    EXPECT_EQ(graph.edge_count(), 2U);
    assert_edge_colors(graph, 0, {5}, false);  // 0->1 color 5
    assert_edge_colors(graph, 1, {6}, false);  // 1->0 color 6
    assert_edge_colors(graph, 0, {6}, true);   // in-edge 1->0 at vertex 0
    assert_edge_colors(graph, 1, {5}, true);   // in-edge 0->1 at vertex 1
}
