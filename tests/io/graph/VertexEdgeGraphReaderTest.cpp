#include "VertexEdgeGraphReader.h"

#include "ColoredGraph.h"
#include "ColoredGraphTestHelpers.h"
#include "GraphConstructionException.h"
#include "ILogger.h"
#include "InvalidArgumentException.h"
#include "LoggerHandler.h"
#include "SgfPathDoesntExistException.h"

#include <gtest/gtest.h>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

using namespace sgf;
using namespace test_helpers;

// ── Fixture ───────────────────────────────────────────────────────────────────

/**
 * @brief Test fixture for VertexEdgeGraphReader.
 *
 * Provides the reader instance and a helper for constructing base paths to
 * the vertex-edge test-data directory (injected at compile time via CMake).
 * The reader appends .vertex_indices and .edges to the returned path.
 */
class VertexEdgeGraphReaderTest : public ::testing::Test
{
protected:
    /**
     * @brief Returns the base path for a named test case in the test-data directory.
     * @param stem Stem name (without file extension) of the test case.
     * @return Absolute base path string.
     */
    static std::string data(const std::string& stem)
    {
        return std::string(VERTEX_EDGE_TEST_DATA_DIR) + "/" + stem;
    }

    VertexEdgeGraphReader m_reader;
};

// ── Path errors ───────────────────────────────────────────────────────────────

/**
 * @brief Reading a base path where both files do not exist must throw SgfPathDoesntExistException.
 */
TEST_F(VertexEdgeGraphReaderTest, nonexistent_path_throws_path_doesnt_exist)
{
    EXPECT_THROW(
        m_reader.read(data("does_not_exist"), false, LoggerHandler(std::weak_ptr<ILogger>{})),
        SgfPathDoesntExistException);
}

/**
 * @brief Missing .vertex_indices file (with .edges present) must throw SgfPathDoesntExistException.
 */
TEST_F(VertexEdgeGraphReaderTest, missing_vertex_file_throws_path_doesnt_exist)
{
    EXPECT_THROW(
        m_reader.read(data("missing_vertex_file"), false, LoggerHandler(std::weak_ptr<ILogger>{})),
        SgfPathDoesntExistException);
}

/**
 * @brief Missing .edges file (with .vertex_indices present) must throw SgfPathDoesntExistException.
 */
TEST_F(VertexEdgeGraphReaderTest, missing_edge_file_throws_path_doesnt_exist)
{
    EXPECT_THROW(
        m_reader.read(data("missing_edge_file"), false, LoggerHandler(std::weak_ptr<ILogger>{})),
        SgfPathDoesntExistException);
}

// ── Malformed vertex file ─────────────────────────────────────────────────────

/**
 * @brief Vertex line with fewer than 2 tokens must throw GraphConstructionException.
 */
TEST_F(VertexEdgeGraphReaderTest, malformed_vertex_line_too_short_throws_graph_construction)
{
    EXPECT_THROW(m_reader.read(data("malformed_vertex_line_too_short"), false,
                               LoggerHandler(std::weak_ptr<ILogger>{})),
                 GraphConstructionException);
}

/**
 * @brief Vertex line with more than 2 tokens must throw GraphConstructionException.
 */
TEST_F(VertexEdgeGraphReaderTest, malformed_vertex_line_too_long_throws_graph_construction)
{
    EXPECT_THROW(m_reader.read(data("malformed_vertex_line_too_long"), false,
                               LoggerHandler(std::weak_ptr<ILogger>{})),
                 GraphConstructionException);
}

/**
 * @brief Duplicate vertex ID in .vertex_indices must throw GraphConstructionException.
 */
TEST_F(VertexEdgeGraphReaderTest, duplicate_vertex_id_throws_graph_construction)
{
    EXPECT_THROW(m_reader.read(data("duplicate_vertex_id"), false,
                               LoggerHandler(std::weak_ptr<ILogger>{})),
                 GraphConstructionException);
}

// ── Malformed edge file ───────────────────────────────────────────────────────

/**
 * @brief Edge line with fewer than 2 tokens must throw GraphConstructionException.
 */
TEST_F(VertexEdgeGraphReaderTest, malformed_edge_line_too_short_throws_graph_construction)
{
    EXPECT_THROW(m_reader.read(data("malformed_edge_line_too_short"), false,
                               LoggerHandler(std::weak_ptr<ILogger>{})),
                 GraphConstructionException);
}

/**
 * @brief Colored edge line with more than 3 tokens must throw GraphConstructionException.
 */
TEST_F(VertexEdgeGraphReaderTest, malformed_edge_line_too_long_throws_graph_construction)
{
    EXPECT_THROW(m_reader.read(data("malformed_edge_line_too_long"), false,
                               LoggerHandler(std::weak_ptr<ILogger>{})),
                 GraphConstructionException);
}

/**
 * @brief Edge referencing an unknown source vertex ID must throw GraphConstructionException.
 */
TEST_F(VertexEdgeGraphReaderTest, unknown_source_vertex_throws_graph_construction)
{
    EXPECT_THROW(m_reader.read(data("unknown_source_vertex"), false,
                               LoggerHandler(std::weak_ptr<ILogger>{})),
                 GraphConstructionException);
}

/**
 * @brief Edge referencing an unknown destination vertex ID must throw GraphConstructionException.
 */
TEST_F(VertexEdgeGraphReaderTest, unknown_dest_vertex_throws_graph_construction)
{
    EXPECT_THROW(m_reader.read(data("unknown_dest_vertex"), false,
                               LoggerHandler(std::weak_ptr<ILogger>{})),
                 GraphConstructionException);
}

/**
 * @brief Mixed edge colors (some lines with color, some without) must throw
 * GraphConstructionException.
 */
TEST_F(VertexEdgeGraphReaderTest, mixed_edge_colors_throws_graph_construction)
{
    EXPECT_THROW(
        m_reader.read(data("mixed_edge_colors"), false, LoggerHandler(std::weak_ptr<ILogger>{})),
        GraphConstructionException);
}

// ── Empty graph ───────────────────────────────────────────────────────────────

/**
 * @brief Empty files produce a zero-vertex, zero-edge undirected ColoredGraph.
 */
TEST_F(VertexEdgeGraphReaderTest, empty_graph_undirected)
{
    const ColoredGraph graph =
        m_reader.read(data("empty_graph"), false, LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 0U);
    EXPECT_EQ(graph.edge_count(), 0U);
    EXPECT_FALSE(graph.is_directed());
}

/**
 * @brief Empty files produce a zero-vertex, zero-edge directed ColoredGraph.
 */
TEST_F(VertexEdgeGraphReaderTest, empty_graph_directed)
{
    const ColoredGraph graph =
        m_reader.read(data("empty_graph"), true, LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 0U);
    EXPECT_EQ(graph.edge_count(), 0U);
    EXPECT_TRUE(graph.is_directed());
}

// ── Single vertex ─────────────────────────────────────────────────────────────

/**
 * @brief Single vertex with no edges, undirected.
 */
TEST_F(VertexEdgeGraphReaderTest, single_vertex_undirected)
{
    const ColoredGraph graph =
        m_reader.read(data("single_vertex"), false, LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 1U);
    EXPECT_EQ(graph.edge_count(), 0U);
    EXPECT_FALSE(graph.is_directed());
    EXPECT_EQ(graph.get_vertex_color(0), 0U);
    assert_neighbours(graph, 0, {});
}

/**
 * @brief Single vertex with no edges, directed.
 */
TEST_F(VertexEdgeGraphReaderTest, single_vertex_directed)
{
    const ColoredGraph graph =
        m_reader.read(data("single_vertex"), true, LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 1U);
    EXPECT_EQ(graph.edge_count(), 0U);
    EXPECT_TRUE(graph.is_directed());
    assert_neighbours(graph, 0, {});
    assert_neighbours(graph, 0, {}, true);
}

// ── Two vertices, no edges ────────────────────────────────────────────────────

/**
 * @brief Two vertices with no edges, undirected.
 */
TEST_F(VertexEdgeGraphReaderTest, two_vertices_no_edges_undirected)
{
    const ColoredGraph graph =
        m_reader.read(data("two_vertices_no_edges"), false, LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_EQ(graph.edge_count(), 0U);
    EXPECT_FALSE(graph.is_directed());
    assert_neighbours(graph, 0, {});
    assert_neighbours(graph, 1, {});
}

/**
 * @brief Two vertices with no edges, directed.
 */
TEST_F(VertexEdgeGraphReaderTest, two_vertices_no_edges_directed)
{
    const ColoredGraph graph =
        m_reader.read(data("two_vertices_no_edges"), true, LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_EQ(graph.edge_count(), 0U);
    EXPECT_TRUE(graph.is_directed());
    assert_neighbours(graph, 0, {});
    assert_neighbours(graph, 0, {}, true);
    assert_neighbours(graph, 1, {});
    assert_neighbours(graph, 1, {}, true);
}

// ── Two vertices, one uncolored edge ──────────────────────────────────────────

/**
 * @brief Two vertices with one uncolored edge, undirected: each vertex sees the other.
 */
TEST_F(VertexEdgeGraphReaderTest, two_vertices_one_edge_undirected)
{
    const ColoredGraph graph =
        m_reader.read(data("two_vertices_one_edge"), false, LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_EQ(graph.edge_count(), 1U);
    EXPECT_FALSE(graph.is_directed());
    EXPECT_FALSE(graph.is_edges_colored());
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {0});
}

/**
 * @brief Two vertices with one uncolored edge, directed: only source sees destination.
 */
TEST_F(VertexEdgeGraphReaderTest, two_vertices_one_edge_directed)
{
    const ColoredGraph graph =
        m_reader.read(data("two_vertices_one_edge"), true, LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_EQ(graph.edge_count(), 1U);
    EXPECT_TRUE(graph.is_directed());
    EXPECT_FALSE(graph.is_edges_colored());
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {});
    assert_neighbours(graph, 0, {}, true);
    assert_neighbours(graph, 1, {0}, true);
}

// ── Triangle: vertex colors, undirected ───────────────────────────────────────

/**
 * @brief Triangle with all vertices the same color, undirected.
 */
TEST_F(VertexEdgeGraphReaderTest, triangle_same_vertex_color_undirected)
{
    const ColoredGraph graph = m_reader.read(data("triangle_same_vertex_color"), false,
                                             LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 3U);
    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_FALSE(graph.is_directed());
    EXPECT_EQ(graph.get_vertex_color(0), graph.get_vertex_color(1));
    EXPECT_EQ(graph.get_vertex_color(0), graph.get_vertex_color(2));
    assert_neighbours(graph, 0, {1, 2});
    assert_neighbours(graph, 1, {0, 2});
    assert_neighbours(graph, 2, {0, 1});
}

/**
 * @brief Triangle with all vertices different colors, undirected.
 */
TEST_F(VertexEdgeGraphReaderTest, triangle_diff_vertex_colors_undirected)
{
    const ColoredGraph graph = m_reader.read(data("triangle_diff_vertex_colors"), false,
                                             LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 3U);
    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_FALSE(graph.is_directed());
    EXPECT_NE(graph.get_vertex_color(0), graph.get_vertex_color(1));
    EXPECT_NE(graph.get_vertex_color(0), graph.get_vertex_color(2));
    EXPECT_NE(graph.get_vertex_color(1), graph.get_vertex_color(2));
    assert_neighbours(graph, 0, {1, 2});
    assert_neighbours(graph, 1, {0, 2});
    assert_neighbours(graph, 2, {0, 1});
}

/**
 * @brief Triangle with two vertices sharing a color, one distinct, undirected.
 * Vertices 0 and 1 share color 7; vertex 2 has color 3.
 */
TEST_F(VertexEdgeGraphReaderTest, triangle_two_same_vertex_color_undirected)
{
    const ColoredGraph graph = m_reader.read(data("triangle_two_same_vertex_color"), false,
                                             LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 3U);
    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_FALSE(graph.is_directed());
    EXPECT_EQ(graph.get_vertex_color(0), graph.get_vertex_color(1));
    EXPECT_NE(graph.get_vertex_color(0), graph.get_vertex_color(2));
    assert_neighbours(graph, 0, {1, 2});
    assert_neighbours(graph, 1, {0, 2});
    assert_neighbours(graph, 2, {0, 1});
}

// ── Triangle: vertex colors, directed ─────────────────────────────────────────

/**
 * @brief Triangle with all vertices the same color, directed (0→1, 1→2, 0→2).
 */
TEST_F(VertexEdgeGraphReaderTest, triangle_same_vertex_color_directed)
{
    const ColoredGraph graph = m_reader.read(data("triangle_same_vertex_color"), true,
                                             LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 3U);
    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_TRUE(graph.is_directed());
    EXPECT_EQ(graph.get_vertex_color(0), graph.get_vertex_color(1));
    EXPECT_EQ(graph.get_vertex_color(0), graph.get_vertex_color(2));
    assert_neighbours(graph, 0, {1, 2});
    assert_neighbours(graph, 1, {2});
    assert_neighbours(graph, 2, {});
    assert_neighbours(graph, 2, {0, 1}, true);
}

/**
 * @brief Triangle with all vertices different colors, directed.
 */
TEST_F(VertexEdgeGraphReaderTest, triangle_diff_vertex_colors_directed)
{
    const ColoredGraph graph = m_reader.read(data("triangle_diff_vertex_colors"), true,
                                             LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 3U);
    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_TRUE(graph.is_directed());
    EXPECT_NE(graph.get_vertex_color(0), graph.get_vertex_color(1));
    EXPECT_NE(graph.get_vertex_color(0), graph.get_vertex_color(2));
    EXPECT_NE(graph.get_vertex_color(1), graph.get_vertex_color(2));
    assert_neighbours(graph, 0, {1, 2});
    assert_neighbours(graph, 1, {2});
    assert_neighbours(graph, 2, {});
    assert_neighbours(graph, 0, {}, true);
    assert_neighbours(graph, 1, {0}, true);
    assert_neighbours(graph, 2, {0,1}, true);
}

/**
 * @brief Triangle with two vertices sharing a color, directed.
 * Vertices 0 and 1 share color 7; vertex 2 has color 3.
 */
TEST_F(VertexEdgeGraphReaderTest, triangle_two_same_vertex_color_directed)
{
    const ColoredGraph graph = m_reader.read(data("triangle_two_same_vertex_color"), true,
                                             LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 3U);
    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_TRUE(graph.is_directed());
    EXPECT_EQ(graph.get_vertex_color(0), graph.get_vertex_color(1));
    EXPECT_NE(graph.get_vertex_color(0), graph.get_vertex_color(2));
    assert_neighbours(graph, 0, {1, 2});
    assert_neighbours(graph, 1, {2});
    assert_neighbours(graph, 2, {});
    assert_neighbours(graph, 0, {}, true);
    assert_neighbours(graph, 1, {0}, true);
    assert_neighbours(graph, 2, {0,1}, true);
}

// ── Two nodes: parallel edges ─────────────────────────────────────────────────

/**
 * @brief Two parallel uncolored edges in same direction, undirected: deduplicates to 1 edge.
 */
TEST_F(VertexEdgeGraphReaderTest, two_nodes_parallel_uncolored_undirected)
{
    const ColoredGraph graph = m_reader.read(data("two_nodes_parallel_uncolored"), false,
                                             LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_EQ(graph.edge_count(), 1U);
    EXPECT_FALSE(graph.is_directed());
    EXPECT_FALSE(graph.is_edges_colored());
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {0});
}

/**
 * @brief Two parallel uncolored edges in same direction, directed: deduplicates to 1 edge.
 */
TEST_F(VertexEdgeGraphReaderTest, two_nodes_parallel_uncolored_directed)
{
    const ColoredGraph graph = m_reader.read(data("two_nodes_parallel_uncolored"), true,
                                             LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_EQ(graph.edge_count(), 1U);
    EXPECT_TRUE(graph.is_directed());
    EXPECT_FALSE(graph.is_edges_colored());
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {});
    assert_neighbours(graph, 1, {0}, true);
    assert_neighbours(graph, 0, {}, true);
}

/**
 * @brief Two parallel same-colored edges, undirected: deduplicates to 1 colored edge.
 */
TEST_F(VertexEdgeGraphReaderTest, two_nodes_parallel_same_edge_color_undirected)
{
    const ColoredGraph graph = m_reader.read(data("two_nodes_parallel_same_edge_color"), false,
                                             LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_EQ(graph.edge_count(), 1U);
    EXPECT_FALSE(graph.is_directed());
    EXPECT_TRUE(graph.is_edges_colored());
    assert_neighbours(graph, 0, {1});
    assert_edge_colors(graph, 0, {3});
}

/**
 * @brief Two parallel same-colored edges, directed: deduplicates to 1 colored edge.
 */
TEST_F(VertexEdgeGraphReaderTest, two_nodes_parallel_same_edge_color_directed)
{
    const ColoredGraph graph = m_reader.read(data("two_nodes_parallel_same_edge_color"), true,
                                             LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_EQ(graph.edge_count(), 1U);
    EXPECT_TRUE(graph.is_directed());
    EXPECT_TRUE(graph.is_edges_colored());
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {0}, true);
    assert_neighbours(graph, 0, {}, true);
    assert_neighbours(graph, 1, {});
    assert_edge_colors(graph, 0, {3});
}

/**
 * @brief Two parallel edges with different colors, undirected: conflict throws
 * InvalidArgumentException.
 */
TEST_F(VertexEdgeGraphReaderTest, two_nodes_parallel_diff_edge_colors_undirected_throws)
{
    EXPECT_THROW(m_reader.read(data("two_nodes_parallel_diff_edge_colors"), false,
                               LoggerHandler(std::weak_ptr<ILogger>{})),
                 InvalidArgumentException);
}

/**
 * @brief Two parallel edges with different colors, directed: conflict throws
 * InvalidArgumentException.
 */
TEST_F(VertexEdgeGraphReaderTest, two_nodes_parallel_diff_edge_colors_directed_throws)
{
    EXPECT_THROW(m_reader.read(data("two_nodes_parallel_diff_edge_colors"), true,
                               LoggerHandler(std::weak_ptr<ILogger>{})),
                 InvalidArgumentException);
}

// ── Two nodes: bidirectional edges ────────────────────────────────────────────

/**
 * @brief Bidirectional uncolored edges, undirected: deduplicates to 1 edge.
 */
TEST_F(VertexEdgeGraphReaderTest, two_nodes_bidirectional_uncolored_undirected)
{
    const ColoredGraph graph = m_reader.read(data("two_nodes_bidirectional_uncolored"), false,
                                             LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_EQ(graph.edge_count(), 1U);
    EXPECT_FALSE(graph.is_directed());
    EXPECT_FALSE(graph.is_edges_colored());
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {0});
}

/**
 * @brief Bidirectional uncolored edges, directed: 2 distinct directed edges.
 */
TEST_F(VertexEdgeGraphReaderTest, two_nodes_bidirectional_uncolored_directed)
{
    const ColoredGraph graph = m_reader.read(data("two_nodes_bidirectional_uncolored"), true,
                                             LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_EQ(graph.edge_count(), 2U);
    EXPECT_TRUE(graph.is_directed());
    EXPECT_FALSE(graph.is_edges_colored());
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {0});
    assert_neighbours(graph, 0, {1}, true);
    assert_neighbours(graph, 1, {0}, true);
}

/**
 * @brief Bidirectional same-colored edges, undirected: deduplicates to 1 colored edge.
 */
TEST_F(VertexEdgeGraphReaderTest, two_nodes_bidirectional_same_edge_color_undirected)
{
    const ColoredGraph graph = m_reader.read(data("two_nodes_bidirectional_same_edge_color"), false,
                                             LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_EQ(graph.edge_count(), 1U);
    EXPECT_FALSE(graph.is_directed());
    EXPECT_TRUE(graph.is_edges_colored());
    assert_neighbours(graph, 0, {1});
    assert_edge_colors(graph, 0, {4});
}

/**
 * @brief Bidirectional same-colored edges, directed: 2 colored edges with the same color.
 */
TEST_F(VertexEdgeGraphReaderTest, two_nodes_bidirectional_same_edge_color_directed)
{
    const ColoredGraph graph = m_reader.read(data("two_nodes_bidirectional_same_edge_color"), true,
                                             LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_EQ(graph.edge_count(), 2U);
    EXPECT_TRUE(graph.is_directed());
    EXPECT_TRUE(graph.is_edges_colored());
    assert_edge_colors(graph, 0, {4});
    assert_edge_colors(graph, 1, {4});
}

/**
 * @brief Bidirectional edges with different colors, undirected: conflict throws
 * InvalidArgumentException.
 */
TEST_F(VertexEdgeGraphReaderTest, two_nodes_bidirectional_diff_edge_colors_undirected_throws)
{
    EXPECT_THROW(m_reader.read(data("two_nodes_bidirectional_diff_edge_colors"), false,
                               LoggerHandler(std::weak_ptr<ILogger>{})),
                 InvalidArgumentException);
}

/**
 * @brief Bidirectional edges with different colors, directed: 2 edges with different colors.
 * Edge 0→1 has color 2; edge 1→0 has color 9.
 */
TEST_F(VertexEdgeGraphReaderTest, two_nodes_bidirectional_diff_edge_colors_directed)
{
    const ColoredGraph graph = m_reader.read(data("two_nodes_bidirectional_diff_edge_colors"), true,
                                             LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_EQ(graph.edge_count(), 2U);
    EXPECT_TRUE(graph.is_directed());
    EXPECT_TRUE(graph.is_edges_colored());
    EXPECT_NE(graph.get_edge_color(0, 1), graph.get_edge_color(1, 0));
}

// ── Triangle: edge colors, undirected ─────────────────────────────────────────

/**
 * @brief Triangle with all edges the same color, undirected.
 */
TEST_F(VertexEdgeGraphReaderTest, triangle_all_edges_same_color_undirected)
{
    const ColoredGraph graph = m_reader.read(data("triangle_all_edges_same_color"), false,
                                             LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 3U);
    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_FALSE(graph.is_directed());
    EXPECT_TRUE(graph.is_edges_colored());
    EXPECT_EQ(graph.get_edge_color(0, 1), 5U);
    EXPECT_EQ(graph.get_edge_color(1, 2), 5U);
    EXPECT_EQ(graph.get_edge_color(0, 2), 5U);
}

/**
 * @brief Triangle with all edges different colors, undirected.
 * Edge 0-1 has color 1, edge 1-2 has color 2, edge 0-2 has color 3.
 */
TEST_F(VertexEdgeGraphReaderTest, triangle_all_edges_diff_colors_undirected)
{
    const ColoredGraph graph = m_reader.read(data("triangle_all_edges_diff_colors"), false,
                                             LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 3U);
    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_FALSE(graph.is_directed());
    EXPECT_TRUE(graph.is_edges_colored());
    EXPECT_EQ(graph.get_edge_color(0, 1), graph.get_edge_color(1, 0));
    EXPECT_NE(graph.get_edge_color(1, 0),graph.get_edge_color(2, 1));
    EXPECT_NE(graph.get_edge_color(1, 0),graph.get_edge_color(2, 0));
    EXPECT_EQ(graph.get_edge_color(1, 2), graph.get_edge_color(2, 1));
    EXPECT_NE(graph.get_edge_color(1, 2), graph.get_edge_color(2, 0));
}

/**
 * @brief Triangle with two edges the same color and one different, undirected.
 * Edges 0-1 and 1-2 have color 5; edge 0-2 has color 9.
 */
TEST_F(VertexEdgeGraphReaderTest, triangle_two_edges_same_color_undirected)
{
    const ColoredGraph graph = m_reader.read(data("triangle_two_edges_same_color"), false,
                                             LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 3U);
    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_FALSE(graph.is_directed());
    EXPECT_TRUE(graph.is_edges_colored());
    EXPECT_EQ(graph.get_edge_color(0, 1), graph.get_edge_color(1, 2));
    EXPECT_NE(graph.get_edge_color(0, 2), graph.get_edge_color(1, 2));
}

// ── Triangle: edge colors, directed ───────────────────────────────────────────

/**
 * @brief Triangle with all edges the same color, directed (0→1, 1→2, 0→2).
 */
TEST_F(VertexEdgeGraphReaderTest, triangle_all_edges_same_color_directed)
{
    const ColoredGraph graph = m_reader.read(data("triangle_all_edges_same_color"), true,
                                             LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 3U);
    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_TRUE(graph.is_directed());
    EXPECT_TRUE(graph.is_edges_colored());
    assert_neighbours(graph, 0, {1, 2});
    assert_neighbours(graph, 1, {2});
    EXPECT_EQ(graph.get_edge_color(0, 1), graph.get_edge_color(0, 2));
    EXPECT_EQ(graph.get_edge_color(0, 1), graph.get_edge_color(1, 2));
}

/**
 * @brief Triangle with all edges different colors, directed.
 * Edge 0→1 has color 1, edge 1→2 has color 2, edge 0→2 has color 3.
 */
TEST_F(VertexEdgeGraphReaderTest, triangle_all_edges_diff_colors_directed)
{
    const ColoredGraph graph = m_reader.read(data("triangle_all_edges_diff_colors"), true,
                                             LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 3U);
    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_TRUE(graph.is_directed());
    EXPECT_TRUE(graph.is_edges_colored());
    EXPECT_NE(graph.get_edge_color(0, 1), graph.get_edge_color(1, 2));
    EXPECT_NE(graph.get_edge_color(1, 2), graph.get_edge_color(0, 2));
    EXPECT_NE(graph.get_edge_color(0, 2), graph.get_edge_color(0, 1));
}

/**
 * @brief Triangle with two edges the same color and one different, directed.
 * Edges 0→1 and 1→2 have color 5; edge 0→2 has color 9.
 */
TEST_F(VertexEdgeGraphReaderTest, triangle_two_edges_same_color_directed)
{
    const ColoredGraph graph = m_reader.read(data("triangle_two_edges_same_color"), true,
                                             LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 3U);
    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_TRUE(graph.is_directed());
    EXPECT_TRUE(graph.is_edges_colored());
    EXPECT_EQ(graph.get_edge_color(0, 1), graph.get_edge_color(1, 2));
    EXPECT_NE(graph.get_edge_color(0, 2), graph.get_edge_color(1, 2));
}

// ── Format-specific edge cases ────────────────────────────────────────────────

/**
 * @brief Non-consecutive vertex IDs are remapped to consecutive indices.
 *
 * Vertex IDs 5, 20, 100 form a path 5-20-100 with edges 5-20 and 20-100.
 * Regardless of the specific index assignments, the resulting graph must have
 * 3 vertices, 2 edges, and one vertex with degree 2 (the middle of the path).
 */
TEST_F(VertexEdgeGraphReaderTest, non_consecutive_vertex_ids_remapped)
{
    const ColoredGraph graph = m_reader.read(data("non_consecutive_vertex_ids"), false,
                                             LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 3U);
    EXPECT_EQ(graph.edge_count(), 2U);
    EXPECT_FALSE(graph.is_directed());
    uint32_t degree_two_count = 0;
    uint32_t degree_one_count = 0;
    for (uint32_t vertex = 0; vertex < graph.vertex_count(); ++vertex)
    {
        const std::pair<std::vector<uint32_t>::const_iterator,
                        std::vector<uint32_t>::const_iterator>
            range = graph.get_neighbours(vertex);
        const std::ptrdiff_t raw_distance = std::distance(range.first, range.second);
        ASSERT_GE(raw_distance, 0);
        const uint32_t degree = static_cast<uint32_t>(raw_distance);
        if (degree == 2U)
        {
            ++degree_two_count;
        }
        else
        {
            EXPECT_EQ(degree, 1U);
            ++degree_one_count;
        }
    }
    EXPECT_EQ(degree_two_count, 1U);
    EXPECT_EQ(degree_one_count, 2U);
}

/**
 * @brief Blank lines in the .vertex_indices file are skipped without error.
 */
TEST_F(VertexEdgeGraphReaderTest, empty_lines_in_vertex_file_skipped)
{
    const ColoredGraph graph = m_reader.read(data("empty_lines_in_vertex_file"), false,
                                             LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 3U);
    EXPECT_EQ(graph.edge_count(), 2U);
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {0, 2});
    assert_neighbours(graph, 2, {1});
}

/**
 * @brief Blank lines in the .edges file are skipped without error.
 */
TEST_F(VertexEdgeGraphReaderTest, empty_lines_in_edge_file_skipped)
{
    const ColoredGraph graph = m_reader.read(data("empty_lines_in_edge_file"), false,
                                             LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 3U);
    EXPECT_EQ(graph.edge_count(), 2U);
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {0, 2});
    assert_neighbours(graph, 2, {1});
}

// ── Whitespace-only lines ─────────────────────────────────────────────────────

/**
 * @brief A whitespace-only line in the .vertex_indices file must throw
 * GraphConstructionException (not silently skipped).
 */
TEST_F(VertexEdgeGraphReaderTest, whitespace_only_vertex_line_throws_graph_construction)
{
    EXPECT_THROW(m_reader.read(data("whitespace_only_vertex_line"), false,
                               LoggerHandler(std::weak_ptr<ILogger>{})),
                 GraphConstructionException);
}

/**
 * @brief A whitespace-only line in the .edges file must throw
 * GraphConstructionException (not silently skipped).
 */
TEST_F(VertexEdgeGraphReaderTest, whitespace_only_edge_line_throws_graph_construction)
{
    EXPECT_THROW(m_reader.read(data("whitespace_only_edge_line"), false,
                               LoggerHandler(std::weak_ptr<ILogger>{})),
                 GraphConstructionException);
}

// ── Non-numeric tokens ────────────────────────────────────────────────────────

/**
 * @brief A non-numeric token in the .vertex_indices file must throw
 * GraphConstructionException.
 */
TEST_F(VertexEdgeGraphReaderTest, non_numeric_token_vertex_file_throws_graph_construction)
{
    EXPECT_THROW(m_reader.read(data("non_numeric_token_vertex_file"), false,
                               LoggerHandler(std::weak_ptr<ILogger>{})),
                 GraphConstructionException);
}

/**
 * @brief A non-numeric token in the .edges file must throw GraphConstructionException.
 */
TEST_F(VertexEdgeGraphReaderTest, non_numeric_token_edge_file_throws_graph_construction)
{
    EXPECT_THROW(m_reader.read(data("non_numeric_token_edge_file"), false,
                               LoggerHandler(std::weak_ptr<ILogger>{})),
                 GraphConstructionException);
}

// ── Self-loop ─────────────────────────────────────────────────────────────────

/**
 * @brief A self-loop edge in the .edges file must throw InvalidArgumentException
 * (propagated from the ColoredGraph constructor).
 */
TEST_F(VertexEdgeGraphReaderTest, self_loop_throws_invalid_argument)
{
    EXPECT_THROW(
        m_reader.read(data("self_loop"), false, LoggerHandler(std::weak_ptr<ILogger>{})),
        InvalidArgumentException);
}

// ── Non-consecutive IDs: color preservation ───────────────────────────────────

/**
 * @brief Non-consecutive vertex IDs with non-zero colors must have their colors
 * preserved correctly after remapping to consecutive indices.
 *
 * File uses IDs 5 (color 3), 20 (color 7), 100 (color 1).
 * The exact index assignment is an implementation detail; only the multiset of
 * colors {1, 3, 7} is verified.
 */
TEST_F(VertexEdgeGraphReaderTest, non_consecutive_vertex_ids_colors_preserved)
{
    const ColoredGraph graph = m_reader.read(data("non_consecutive_with_colors"), false,
                                             LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 3U);
    std::vector<uint32_t> colors;
    for (uint32_t vertex = 0; vertex < graph.vertex_count(); ++vertex)
    {
        colors.push_back(graph.get_vertex_color(vertex));
    }
    std::sort(colors.begin(), colors.end());
    const std::vector<uint32_t> expected_colors = {1U, 3U, 7U};
    EXPECT_EQ(colors, expected_colors);
}
