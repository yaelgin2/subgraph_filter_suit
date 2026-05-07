#include "GraphmlPatternWriter.h"

#include "BoostGraph.h"
#include "ColoredGraph.h"
#include "ColoredGraphTestHelpers.h"
#include "GraphmlGraphReader.h"
#include "ILogger.h"
#include "LoggerHandler.h"
#include "SgfPathDoesntExistException.h"

#include <boost/graph/adjacency_list.hpp>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <string>

using namespace sgf;
using namespace test_helpers;

// ── Fixture ───────────────────────────────────────────────────────────────────

/**
 * @brief Test fixture for GraphmlPatternWriter.
 *
 * Each test writes a BoostGraph to a temp file then reads it back via
 * GraphmlGraphReader to verify the round-trip. All written files land in
 * GTest's temp directory and are never cleaned up manually.
 */
class GraphmlPatternWriterTest : public ::testing::Test
{
protected:
    /**
     * @brief Writes @p graph to a temp file, reads it back, and returns the result.
     * @param graph BoostGraph to serialize.
     * @param filename Filename inside the GTest temp directory.
     * @return The parsed ColoredGraph (always directed — BoostGraph uses directedS).
     */
    ColoredGraph round_trip(const BoostGraph& graph, const std::string& filename)
    {
        const std::string path = std::string(testing::TempDir()) + "/" + filename;
        m_writer.write(graph, path);
        return m_reader.read(path, true, LoggerHandler(std::weak_ptr<ILogger>{}));
    }

    GraphmlPatternWriter m_writer;
    GraphmlGraphReader m_reader;
};

// ── Empty graph ───────────────────────────────────────────────────────────────

/**
 * @brief An empty BoostGraph serializes to zero vertices and zero edges.
 */
TEST_F(GraphmlPatternWriterTest, empty_graph)
{
    const BoostGraph graph;
    const ColoredGraph result = round_trip(graph, "writer_empty.graphml");
    EXPECT_EQ(result.vertex_count(), 0U);
    EXPECT_EQ(result.edge_count(), 0U);
    EXPECT_TRUE(result.is_directed());
}

// ── Single node ───────────────────────────────────────────────────────────────

/**
 * @brief Single vertex with default color (0) round-trips correctly.
 */
TEST_F(GraphmlPatternWriterTest, single_node_default_color)
{
    BoostGraph graph;
    boost::add_vertex(graph);
    const ColoredGraph result = round_trip(graph, "writer_single_node_default.graphml");
    EXPECT_EQ(result.vertex_count(), 1U);
    EXPECT_EQ(result.edge_count(), 0U);
    EXPECT_EQ(result.get_vertex_color(0), 0U);
    assert_neighbours(result, 0, {});
}

/**
 * @brief Single vertex with a non-default color survives serialization.
 *
 * The exact uint ID is remapped by the reader's first-appearance registry;
 * only the structural presence of the color is verified here.
 */
TEST_F(GraphmlPatternWriterTest, single_node_non_default_color)
{
    BoostGraph graph;
    const BoostGraph::vertex_descriptor vertex = boost::add_vertex(graph);
    graph[vertex].m_color = 3;
    const ColoredGraph result = round_trip(graph, "writer_single_node_colored.graphml");
    EXPECT_EQ(result.vertex_count(), 1U);
    EXPECT_EQ(result.edge_count(), 0U);
}

// ── Two nodes, no edges ───────────────────────────────────────────────────────

/**
 * @brief Two isolated vertices serialize without edges.
 */
TEST_F(GraphmlPatternWriterTest, two_nodes_no_edges)
{
    BoostGraph graph;
    boost::add_vertex(graph);
    boost::add_vertex(graph);
    const ColoredGraph result = round_trip(graph, "writer_two_nodes_no_edges.graphml");
    EXPECT_EQ(result.vertex_count(), 2U);
    EXPECT_EQ(result.edge_count(), 0U);
    assert_neighbours(result, 0, {});
    assert_neighbours(result, 1, {});
}

// ── Two nodes, one directed edge ──────────────────────────────────────────────

/**
 * @brief One directed edge 0→1 is preserved with correct out-/in-neighbour sets.
 */
TEST_F(GraphmlPatternWriterTest, two_nodes_one_directed_edge)
{
    BoostGraph graph;
    const BoostGraph::vertex_descriptor v0 = boost::add_vertex(graph);
    const BoostGraph::vertex_descriptor v1 = boost::add_vertex(graph);
    boost::add_edge(v0, v1, graph);
    const ColoredGraph result = round_trip(graph, "writer_two_nodes_one_edge.graphml");
    EXPECT_EQ(result.vertex_count(), 2U);
    EXPECT_EQ(result.edge_count(), 1U);
    assert_neighbours(result, 0, {1});
    assert_neighbours(result, 1, {});
    assert_neighbours(result, 0, {}, true);
    assert_neighbours(result, 1, {0}, true);
}

// ── Two nodes, two directed edges ─────────────────────────────────────────────

/**
 * @brief Bidirectional edges (0→1 and 1→0) produce two directed edges.
 */
TEST_F(GraphmlPatternWriterTest, two_nodes_two_directed_edges)
{
    BoostGraph graph;
    const BoostGraph::vertex_descriptor v0 = boost::add_vertex(graph);
    const BoostGraph::vertex_descriptor v1 = boost::add_vertex(graph);
    boost::add_edge(v0, v1, graph);
    boost::add_edge(v1, v0, graph);
    const ColoredGraph result = round_trip(graph, "writer_two_nodes_two_edges.graphml");
    EXPECT_EQ(result.vertex_count(), 2U);
    EXPECT_EQ(result.edge_count(), 2U);
    assert_neighbours(result, 0, {1});
    assert_neighbours(result, 1, {0});
    assert_neighbours(result, 0, {1}, true);
    assert_neighbours(result, 1, {0}, true);
}

// ── Two nodes, different vertex colors ───────────────────────────────────────

/**
 * @brief One directed edge, vertices with distinct colors: colors remain distinct after round-trip.
 */
TEST_F(GraphmlPatternWriterTest, two_nodes_one_edge_different_vertex_colors)
{
    BoostGraph graph;
    const BoostGraph::vertex_descriptor v0 = boost::add_vertex(graph);
    const BoostGraph::vertex_descriptor v1 = boost::add_vertex(graph);
    graph[v0].m_color = 1;
    graph[v1].m_color = 2;
    boost::add_edge(v0, v1, graph);
    const ColoredGraph result = round_trip(graph, "writer_two_nodes_diff_colors.graphml");
    EXPECT_EQ(result.vertex_count(), 2U);
    EXPECT_EQ(result.edge_count(), 1U);
    EXPECT_NE(result.get_vertex_color(0), result.get_vertex_color(1));
}

/**
 * @brief Bidirectional edges, vertices with distinct colors: colors remain distinct.
 */
TEST_F(GraphmlPatternWriterTest, two_nodes_two_edges_different_vertex_colors)
{
    BoostGraph graph;
    const BoostGraph::vertex_descriptor v0 = boost::add_vertex(graph);
    const BoostGraph::vertex_descriptor v1 = boost::add_vertex(graph);
    graph[v0].m_color = 1;
    graph[v1].m_color = 2;
    boost::add_edge(v0, v1, graph);
    boost::add_edge(v1, v0, graph);
    const ColoredGraph result = round_trip(graph, "writer_two_nodes_two_edges_diff_colors.graphml");
    EXPECT_EQ(result.vertex_count(), 2U);
    EXPECT_EQ(result.edge_count(), 2U);
    EXPECT_NE(result.get_vertex_color(0), result.get_vertex_color(1));
}

// ── Edge colors ───────────────────────────────────────────────────────────────

/**
 * @brief A colored edge round-trips with is_edges_colored true.
 */
TEST_F(GraphmlPatternWriterTest, edge_with_non_default_color)
{
    BoostGraph graph;
    const BoostGraph::vertex_descriptor v0 = boost::add_vertex(graph);
    const BoostGraph::vertex_descriptor v1 = boost::add_vertex(graph);
    const BoostGraph::edge_descriptor edge = boost::add_edge(v0, v1, graph).first;
    graph[edge].m_color = 1;
    const ColoredGraph result = round_trip(graph, "writer_edge_colored.graphml");
    EXPECT_EQ(result.edge_count(), 1U);
    EXPECT_TRUE(result.is_edges_colored());
}

/**
 * @brief Two directed edges with distinct colors remain distinct after round-trip.
 */
TEST_F(GraphmlPatternWriterTest, two_edges_different_colors)
{
    BoostGraph graph;
    const BoostGraph::vertex_descriptor v0 = boost::add_vertex(graph);
    const BoostGraph::vertex_descriptor v1 = boost::add_vertex(graph);
    const BoostGraph::edge_descriptor e01 = boost::add_edge(v0, v1, graph).first;
    const BoostGraph::edge_descriptor e10 = boost::add_edge(v1, v0, graph).first;
    graph[e01].m_color = 1;
    graph[e10].m_color = 2;
    const ColoredGraph result = round_trip(graph, "writer_two_edges_diff_colors.graphml");
    EXPECT_EQ(result.edge_count(), 2U);
    EXPECT_TRUE(result.is_edges_colored());
    EXPECT_NE(result.get_edge_color(0, 1), result.get_edge_color(1, 0));
}

// ── Triangle: same vertex color ───────────────────────────────────────────────

/**
 * @brief Triangle where all vertices share the same color, directed edges 0→1, 1→2, 0→2.
 */
TEST_F(GraphmlPatternWriterTest, triangle_same_vertex_color_directed)
{
    BoostGraph graph;
    const BoostGraph::vertex_descriptor v0 = boost::add_vertex(graph);
    const BoostGraph::vertex_descriptor v1 = boost::add_vertex(graph);
    const BoostGraph::vertex_descriptor v2 = boost::add_vertex(graph);
    boost::add_edge(v0, v1, graph);
    boost::add_edge(v1, v2, graph);
    boost::add_edge(v0, v2, graph);
    const ColoredGraph result = round_trip(graph, "writer_triangle_same_color_directed.graphml");
    EXPECT_EQ(result.vertex_count(), 3U);
    EXPECT_EQ(result.edge_count(), 3U);
    EXPECT_EQ(result.get_vertex_color(0), result.get_vertex_color(1));
    EXPECT_EQ(result.get_vertex_color(0), result.get_vertex_color(2));
    assert_neighbours(result, 0, {1, 2});
    assert_neighbours(result, 1, {2});
    assert_neighbours(result, 2, {});
}

/**
 * @brief Triangle same vertex color, all six directed edges (fully connected bidirectional).
 */
TEST_F(GraphmlPatternWriterTest, triangle_same_vertex_color_bidirectional)
{
    BoostGraph graph;
    const BoostGraph::vertex_descriptor v0 = boost::add_vertex(graph);
    const BoostGraph::vertex_descriptor v1 = boost::add_vertex(graph);
    const BoostGraph::vertex_descriptor v2 = boost::add_vertex(graph);
    boost::add_edge(v0, v1, graph);
    boost::add_edge(v1, v0, graph);
    boost::add_edge(v1, v2, graph);
    boost::add_edge(v2, v1, graph);
    boost::add_edge(v0, v2, graph);
    boost::add_edge(v2, v0, graph);
    const ColoredGraph result =
        round_trip(graph, "writer_triangle_same_color_bidirectional.graphml");
    EXPECT_EQ(result.vertex_count(), 3U);
    EXPECT_EQ(result.edge_count(), 6U);
    EXPECT_EQ(result.get_vertex_color(0), result.get_vertex_color(1));
    EXPECT_EQ(result.get_vertex_color(0), result.get_vertex_color(2));
}

// ── Triangle: all different vertex colors ─────────────────────────────────────

/**
 * @brief Triangle where each vertex has a distinct color, directed edges 0→1, 1→2, 0→2.
 */
TEST_F(GraphmlPatternWriterTest, triangle_all_different_vertex_colors_directed)
{
    BoostGraph graph;
    const BoostGraph::vertex_descriptor v0 = boost::add_vertex(graph);
    const BoostGraph::vertex_descriptor v1 = boost::add_vertex(graph);
    const BoostGraph::vertex_descriptor v2 = boost::add_vertex(graph);
    graph[v0].m_color = 1;
    graph[v1].m_color = 2;
    graph[v2].m_color = 3;
    boost::add_edge(v0, v1, graph);
    boost::add_edge(v1, v2, graph);
    boost::add_edge(v0, v2, graph);
    const ColoredGraph result = round_trip(graph, "writer_triangle_diff_colors_directed.graphml");
    EXPECT_EQ(result.vertex_count(), 3U);
    EXPECT_EQ(result.edge_count(), 3U);
    EXPECT_NE(result.get_vertex_color(0), result.get_vertex_color(1));
    EXPECT_NE(result.get_vertex_color(0), result.get_vertex_color(2));
    EXPECT_NE(result.get_vertex_color(1), result.get_vertex_color(2));
    assert_neighbours(result, 0, {1, 2});
    assert_neighbours(result, 1, {2});
    assert_neighbours(result, 2, {});
}

/**
 * @brief Triangle different vertex colors, fully bidirectional (six directed edges).
 */
TEST_F(GraphmlPatternWriterTest, triangle_all_different_vertex_colors_bidirectional)
{
    BoostGraph graph;
    const BoostGraph::vertex_descriptor v0 = boost::add_vertex(graph);
    const BoostGraph::vertex_descriptor v1 = boost::add_vertex(graph);
    const BoostGraph::vertex_descriptor v2 = boost::add_vertex(graph);
    graph[v0].m_color = 1;
    graph[v1].m_color = 2;
    graph[v2].m_color = 3;
    boost::add_edge(v0, v1, graph);
    boost::add_edge(v1, v0, graph);
    boost::add_edge(v1, v2, graph);
    boost::add_edge(v2, v1, graph);
    boost::add_edge(v0, v2, graph);
    boost::add_edge(v2, v0, graph);
    const ColoredGraph result =
        round_trip(graph, "writer_triangle_diff_colors_bidirectional.graphml");
    EXPECT_EQ(result.vertex_count(), 3U);
    EXPECT_EQ(result.edge_count(), 6U);
    EXPECT_NE(result.get_vertex_color(0), result.get_vertex_color(1));
    EXPECT_NE(result.get_vertex_color(0), result.get_vertex_color(2));
    EXPECT_NE(result.get_vertex_color(1), result.get_vertex_color(2));
}

// ── Error cases ───────────────────────────────────────────────────────────────

/**
 * @brief Writing to a path whose parent directory does not exist throws
 * SgfPathDoesntExistException.
 */
TEST_F(GraphmlPatternWriterTest, nonexistent_directory_throws_path_doesnt_exist)
{
    const BoostGraph graph;
    EXPECT_THROW(m_writer.write(graph, "/no_such_directory_sgf_test/file.graphml"),
                 SgfPathDoesntExistException);
}
