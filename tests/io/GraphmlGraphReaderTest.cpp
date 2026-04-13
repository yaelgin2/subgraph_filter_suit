#include "GraphmlGraphReader.h"

#include "ColoredGraph.h"
#include "ColoredGraphTestHelpers.h"
#include "GraphConstructionException.h"
#include "ILogger.h"
#include "InvalidArgumentException.h"
#include "SgfPathDoesntExistException.h"

#include <gtest/gtest.h>
#include <memory>
#include <string>

using namespace sgf;
using namespace test_helpers;

// ── Fixture ───────────────────────────────────────────────────────────────────

/**
 * @brief Test fixture for GraphmlGraphReader.
 *
 * Provides the reader instance and a helper for constructing absolute paths to
 * the graphml test-data directory (injected at compile time via CMake).
 */
class GraphmlGraphReaderTest : public ::testing::Test
{
protected:
    /**
     * @brief Returns the absolute path to a file in the graphml test-data directory.
     * @param filename Filename relative to the graphml test-data directory.
     * @return Absolute path string.
     */
    static std::string data(const std::string& filename)
    {
        return std::string(GRAPHML_TEST_DATA_DIR) + "/" + filename;
    }

    GraphmlGraphReader m_reader;
};

// ── Invalid path ──────────────────────────────────────────────────────────────

/**
 * @brief Reading a path that does not exist must throw SgfPathDoesntExistException.
 */
TEST_F(GraphmlGraphReaderTest, nonexistent_path_throws_path_doesnt_exist)
{
    EXPECT_THROW(m_reader.read(data("does_not_exist.graphml"), false, std::weak_ptr<ILogger>{}),
                 SgfPathDoesntExistException);
}

// ── Parse errors ──────────────────────────────────────────────────────────────

/**
 * @brief Malformed XML must throw GraphConstructionException.
 */
TEST_F(GraphmlGraphReaderTest, malformed_xml_throws_graph_construction)
{
    EXPECT_THROW(m_reader.read(data("malformed_xml.graphml"), false, std::weak_ptr<ILogger>{}),
                 GraphConstructionException);
}

/**
 * @brief Valid XML but invalid graphml (edge to undefined node) must throw
 * GraphConstructionException.
 */
TEST_F(GraphmlGraphReaderTest, invalid_graphml_structure_throws_graph_construction)
{
    EXPECT_THROW(
        m_reader.read(data("invalid_graphml_structure.graphml"), false, std::weak_ptr<ILogger>{}),
        GraphConstructionException);
}

// ── Empty graph ───────────────────────────────────────────────────────────────

/**
 * @brief An empty graphml file produces a zero-vertex undirected, zero-edge ColoredGraph.
 */
TEST_F(GraphmlGraphReaderTest, empty_graph_undirected)
{
    const ColoredGraph graph =
        m_reader.read(data("empty_undirected.graphml"), false, std::weak_ptr<ILogger>{});
    EXPECT_EQ(graph.vertex_count(), 0U);
    EXPECT_EQ(graph.edge_count(), 0U);
    EXPECT_FALSE(graph.is_directed());
}

/**
 * @brief An empty graphml file produces a zero-vertex directed, zero-edge ColoredGraph.
 */
TEST_F(GraphmlGraphReaderTest, empty_graph_directed)
{
    const ColoredGraph graph =
        m_reader.read(data("empty_directed.graphml"), true, std::weak_ptr<ILogger>{});
    EXPECT_EQ(graph.vertex_count(), 0U);
    EXPECT_EQ(graph.edge_count(), 0U);
    EXPECT_TRUE(graph.is_directed());
}

// ── Single node ───────────────────────────────────────────────────────────────

/**
 * @brief Single-node undirected graphml: one vertex, no edges, not directed.
 */
TEST_F(GraphmlGraphReaderTest, single_node_undirected)
{
    const ColoredGraph graph =
        m_reader.read(data("single_node_undirected.graphml"), false, std::weak_ptr<ILogger>{});
    EXPECT_EQ(graph.vertex_count(), 1U);
    EXPECT_EQ(graph.edge_count(), 0U);
    EXPECT_FALSE(graph.is_directed());
    EXPECT_EQ(graph.get_vertex_color(0), 0U);
    assert_neighbours(graph, 0, {});
}

/**
 * @brief Single-node directed graphml: one vertex, no edges, directed.
 */
TEST_F(GraphmlGraphReaderTest, single_node_directed)
{
    const ColoredGraph graph =
        m_reader.read(data("single_node_directed.graphml"), true, std::weak_ptr<ILogger>{});
    EXPECT_EQ(graph.vertex_count(), 1U);
    EXPECT_EQ(graph.edge_count(), 0U);
    EXPECT_TRUE(graph.is_directed());
    assert_neighbours(graph, 0, {});
    assert_neighbours(graph, 0, {}, true);
}

// ── Two nodes, no edges ───────────────────────────────────────────────────────

/**
 * @brief Two-node undirected graphml with no edges.
 */
TEST_F(GraphmlGraphReaderTest, two_nodes_no_edges_undirected)
{
    const ColoredGraph graph = m_reader.read(data("two_nodes_no_edges_undirected.graphml"), false,
                                             std::weak_ptr<ILogger>{});
    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_EQ(graph.edge_count(), 0U);
    EXPECT_FALSE(graph.is_directed());
    assert_neighbours(graph, 0, {});
    assert_neighbours(graph, 1, {});
}

/**
 * @brief Two-node directed graphml with no edges.
 */
TEST_F(GraphmlGraphReaderTest, two_nodes_no_edges_directed)
{
    const ColoredGraph graph =
        m_reader.read(data("two_nodes_no_edges_directed.graphml"), true, std::weak_ptr<ILogger>{});
    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_EQ(graph.edge_count(), 0U);
    EXPECT_TRUE(graph.is_directed());
    assert_neighbours(graph, 0, {});
    assert_neighbours(graph, 0, {}, true);
    assert_neighbours(graph, 1, {});
    assert_neighbours(graph, 1, {}, true);
}

// ── Two nodes, one edge ───────────────────────────────────────────────────────

/**
 * @brief Two nodes, one edge, undirected: each vertex sees the other as neighbour.
 */
TEST_F(GraphmlGraphReaderTest, two_nodes_one_edge_undirected)
{
    const ColoredGraph graph = m_reader.read(data("two_nodes_one_edge_undirected.graphml"), false,
                                             std::weak_ptr<ILogger>{});
    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_EQ(graph.edge_count(), 1U);
    EXPECT_FALSE(graph.is_directed());
    EXPECT_FALSE(graph.is_edges_colored());
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {0});
}

/**
 * @brief Two nodes, one directed edge 0→1.
 */
TEST_F(GraphmlGraphReaderTest, two_nodes_one_edge_directed)
{
    const ColoredGraph graph =
        m_reader.read(data("two_nodes_one_edge_directed.graphml"), true, std::weak_ptr<ILogger>{});
    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_EQ(graph.edge_count(), 1U);
    EXPECT_TRUE(graph.is_directed());
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {});
    assert_neighbours(graph, 0, {}, true);
    assert_neighbours(graph, 1, {0}, true);
}

// ── Triangle: vertex colors, undirected ───────────────────────────────────────

/**
 * @brief Triangle with all vertices the same color, undirected.
 */
TEST_F(GraphmlGraphReaderTest, triangle_same_vertex_color_undirected)
{
    const ColoredGraph graph = m_reader.read(data("triangle_same_vertex_color_undirected.graphml"),
                                             false, std::weak_ptr<ILogger>{});
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
TEST_F(GraphmlGraphReaderTest, triangle_diff_vertex_colors_undirected)
{
    const ColoredGraph graph = m_reader.read(data("triangle_diff_vertex_colors_undirected.graphml"),
                                             false, std::weak_ptr<ILogger>{});
    EXPECT_EQ(graph.vertex_count(), 3U);
    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_FALSE(graph.is_directed());
    assert_neighbours(graph, 0, {1, 2});
    assert_neighbours(graph, 1, {0, 2});
    assert_neighbours(graph, 2, {0, 1});
    EXPECT_NE(graph.get_vertex_color(0), graph.get_vertex_color(1));
    EXPECT_NE(graph.get_vertex_color(0), graph.get_vertex_color(2));
    EXPECT_NE(graph.get_vertex_color(1), graph.get_vertex_color(2));
}

/**
 * @brief Triangle with two vertices sharing a color, one distinct, undirected.
 */
TEST_F(GraphmlGraphReaderTest, triangle_two_same_vertex_color_undirected)
{
    const ColoredGraph graph = m_reader.read(
        data("triangle_two_same_vertex_color_undirected.graphml"), false, std::weak_ptr<ILogger>{});
    EXPECT_EQ(graph.vertex_count(), 3U);
    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_FALSE(graph.is_directed());
    assert_neighbours(graph, 0, {1, 2});
    assert_neighbours(graph, 1, {0, 2});
    assert_neighbours(graph, 2, {0, 1});
    EXPECT_EQ(graph.get_vertex_color(0), graph.get_vertex_color(1));
    EXPECT_NE(graph.get_vertex_color(0), graph.get_vertex_color(2));
}

// ── Triangle: vertex colors, directed ─────────────────────────────────────────

/**
 * @brief Triangle with all vertices the same color, directed.
 */
TEST_F(GraphmlGraphReaderTest, triangle_same_vertex_color_directed)
{
    const ColoredGraph graph = m_reader.read(data("triangle_same_vertex_color_directed.graphml"),
                                             true, std::weak_ptr<ILogger>{});
    EXPECT_EQ(graph.vertex_count(), 3U);
    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_TRUE(graph.is_directed());
    assert_neighbours(graph, 0, {1, 2});
    assert_neighbours(graph, 1, {2});
    assert_neighbours(graph, 2, {});
    assert_neighbours(graph, 2, {0, 1}, true);
    EXPECT_EQ(graph.get_vertex_color(0), graph.get_vertex_color(1));
    EXPECT_EQ(graph.get_vertex_color(0), graph.get_vertex_color(2));
}

/**
 * @brief Triangle with all vertices different colors, directed.
 */
TEST_F(GraphmlGraphReaderTest, triangle_diff_vertex_colors_directed)
{
    const ColoredGraph graph = m_reader.read(data("triangle_diff_vertex_colors_directed.graphml"),
                                             true, std::weak_ptr<ILogger>{});
    EXPECT_EQ(graph.vertex_count(), 3U);
    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_TRUE(graph.is_directed());
    assert_neighbours(graph, 0, {1, 2});
    assert_neighbours(graph, 1, {2});
    assert_neighbours(graph, 2, {});
    assert_neighbours(graph, 2, {0, 1}, true);
    EXPECT_NE(graph.get_vertex_color(0), graph.get_vertex_color(1));
    EXPECT_NE(graph.get_vertex_color(0), graph.get_vertex_color(2));
    EXPECT_NE(graph.get_vertex_color(1), graph.get_vertex_color(2));
}

/**
 * @brief Triangle with two vertices same color, one distinct, directed.
 */
TEST_F(GraphmlGraphReaderTest, triangle_two_same_vertex_color_directed)
{
    const ColoredGraph graph = m_reader.read(
        data("triangle_two_same_vertex_color_directed.graphml"), true, std::weak_ptr<ILogger>{});
    EXPECT_EQ(graph.vertex_count(), 3U);
    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_TRUE(graph.is_directed());
    assert_neighbours(graph, 0, {1, 2});
    assert_neighbours(graph, 1, {2});
    assert_neighbours(graph, 2, {});
    assert_neighbours(graph, 2, {0, 1}, true);
    EXPECT_EQ(graph.get_vertex_color(0), graph.get_vertex_color(1));
    EXPECT_NE(graph.get_vertex_color(0), graph.get_vertex_color(2));
}

// ── Two nodes: bidirectional edges ────────────────────────────────────────────

/**
 * @brief Bidirectional uncolored edges, undirected: deduplicated to one edge.
 */
TEST_F(GraphmlGraphReaderTest, two_nodes_bidirectional_uncolored_undirected)
{
    const ColoredGraph graph =
        m_reader.read(data("two_nodes_bidirectional_uncolored_undirected.graphml"), false,
                      std::weak_ptr<ILogger>{});
    EXPECT_EQ(graph.edge_count(), 1U);
    EXPECT_FALSE(graph.is_edges_colored());
    EXPECT_FALSE(graph.is_directed());
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {0});
}

/**
 * @brief Bidirectional uncolored edges, directed: two distinct directed edges.
 */
TEST_F(GraphmlGraphReaderTest, two_nodes_bidirectional_uncolored_directed)
{
    const ColoredGraph graph = m_reader.read(
        data("two_nodes_bidirectional_uncolored_directed.graphml"), true, std::weak_ptr<ILogger>{});
    EXPECT_EQ(graph.edge_count(), 2U);
    EXPECT_FALSE(graph.is_edges_colored());
    EXPECT_TRUE(graph.is_directed());
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {0});
    assert_neighbours(graph, 0, {1}, true);
    assert_neighbours(graph, 1, {0}, true);
}

// ── Two nodes: parallel edges (same direction) ────────────────────────────────

/**
 * @brief Parallel uncolored edges, undirected: deduplicated to one edge.
 */
TEST_F(GraphmlGraphReaderTest, two_nodes_parallel_uncolored_undirected)
{
    const ColoredGraph graph = m_reader.read(
        data("two_nodes_parallel_uncolored_undirected.graphml"), false, std::weak_ptr<ILogger>{});
    EXPECT_EQ(graph.edge_count(), 1U);
    EXPECT_FALSE(graph.is_edges_colored());
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {0});
}

/**
 * @brief Parallel uncolored edges, directed: deduplicated to one edge.
 */
TEST_F(GraphmlGraphReaderTest, two_nodes_parallel_uncolored_directed)
{
    const ColoredGraph graph = m_reader.read(data("two_nodes_parallel_uncolored_directed.graphml"),
                                             true, std::weak_ptr<ILogger>{});
    EXPECT_EQ(graph.edge_count(), 1U);
    EXPECT_FALSE(graph.is_edges_colored());
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {});
    assert_neighbours(graph, 0, {}, true);
    assert_neighbours(graph, 1, {0}, true);
}

// ── Two nodes: bidirectional same edge color ──────────────────────────────────

/**
 * @brief Bidirectional edges with the same color, undirected: one colored edge.
 */
TEST_F(GraphmlGraphReaderTest, two_nodes_bidirectional_same_edge_color_undirected)
{
    const ColoredGraph graph =
        m_reader.read(data("two_nodes_bidirectional_same_edge_color_undirected.graphml"), false,
                      std::weak_ptr<ILogger>{});
    EXPECT_EQ(graph.edge_count(), 1U);
    EXPECT_TRUE(graph.is_edges_colored());
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {0});
}

/**
 * @brief Bidirectional edges with the same color, directed: two colored edges.
 */
TEST_F(GraphmlGraphReaderTest, two_nodes_bidirectional_same_edge_color_directed)
{
    const ColoredGraph graph =
        m_reader.read(data("two_nodes_bidirectional_same_edge_color_directed.graphml"), true,
                      std::weak_ptr<ILogger>{});
    EXPECT_EQ(graph.edge_count(), 2U);
    EXPECT_TRUE(graph.is_edges_colored());
    EXPECT_EQ(graph.get_edge_color(0, 1), graph.get_edge_color(1, 0));
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {0});
}

// ── Two nodes: bidirectional different edge colors ────────────────────────────

/**
 * @brief Bidirectional edges with conflicting colors, undirected: must throw
 * InvalidArgumentException.
 *
 * The undirected conversion creates a color conflict for the same endpoint pair.
 */
TEST_F(GraphmlGraphReaderTest, two_nodes_bidirectional_diff_edge_colors_undirected_throws)
{
    EXPECT_THROW(m_reader.read(data("two_nodes_bidirectional_diff_edge_colors_undirected.graphml"),
                               false, std::weak_ptr<ILogger>{}),
                 InvalidArgumentException);
}

/**
 * @brief Bidirectional edges with different colors, directed: two separately colored edges.
 */
TEST_F(GraphmlGraphReaderTest, two_nodes_bidirectional_diff_edge_colors_directed)
{
    const ColoredGraph graph =
        m_reader.read(data("two_nodes_bidirectional_diff_edge_colors_directed.graphml"), true,
                      std::weak_ptr<ILogger>{});
    EXPECT_EQ(graph.edge_count(), 2U);
    EXPECT_TRUE(graph.is_edges_colored());
    EXPECT_NE(graph.get_edge_color(0, 1), graph.get_edge_color(1, 0));
}

// ── Two nodes: parallel same edge color ───────────────────────────────────────

/**
 * @brief Parallel edges with the same color, undirected: deduplicated to one colored edge.
 */
TEST_F(GraphmlGraphReaderTest, two_nodes_parallel_same_edge_color_undirected)
{
    const ColoredGraph graph =
        m_reader.read(data("two_nodes_parallel_same_edge_color_undirected.graphml"), false,
                      std::weak_ptr<ILogger>{});
    EXPECT_EQ(graph.edge_count(), 1U);
    EXPECT_TRUE(graph.is_edges_colored());
}

/**
 * @brief Parallel edges with the same color, directed: deduplicated to one colored edge.
 */
TEST_F(GraphmlGraphReaderTest, two_nodes_parallel_same_edge_color_directed)
{
    const ColoredGraph graph =
        m_reader.read(data("two_nodes_parallel_same_edge_color_directed.graphml"), true,
                      std::weak_ptr<ILogger>{});
    EXPECT_EQ(graph.edge_count(), 1U);
    EXPECT_TRUE(graph.is_edges_colored());
}

// ── Two nodes: parallel different edge colors ─────────────────────────────────

/**
 * @brief Parallel edges with conflicting colors, undirected: must throw InvalidArgumentException.
 */
TEST_F(GraphmlGraphReaderTest, two_nodes_parallel_diff_edge_colors_undirected_throws)
{
    EXPECT_THROW(m_reader.read(data("two_nodes_parallel_diff_edge_colors_undirected.graphml"),
                               false, std::weak_ptr<ILogger>{}),
                 InvalidArgumentException);
}

/**
 * @brief Parallel edges with conflicting colors, directed: must throw InvalidArgumentException.
 */
TEST_F(GraphmlGraphReaderTest, two_nodes_parallel_diff_edge_colors_directed_throws)
{
    EXPECT_THROW(m_reader.read(data("two_nodes_parallel_diff_edge_colors_directed.graphml"), true,
                               std::weak_ptr<ILogger>{}),
                 InvalidArgumentException);
}

// ── Triangle edge colors, undirected ──────────────────────────────────────────

/**
 * @brief Triangle with all edges the same color, undirected.
 */
TEST_F(GraphmlGraphReaderTest, triangle_all_edges_same_color_undirected)
{
    const ColoredGraph graph = m_reader.read(
        data("triangle_all_edges_same_color_undirected.graphml"), false, std::weak_ptr<ILogger>{});
    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_TRUE(graph.is_edges_colored());
    EXPECT_EQ(graph.get_edge_color(0, 1), graph.get_edge_color(1, 2));
    EXPECT_EQ(graph.get_edge_color(1, 2), graph.get_edge_color(0, 2));
}

/**
 * @brief Triangle with all edges different colors, undirected.
 *
 * Edges: (0,1)=1, (1,2)=2, (0,2)=3. Neighbours sorted ascending, colors aligned.
 */
TEST_F(GraphmlGraphReaderTest, triangle_all_edges_diff_colors_undirected)
{
    const ColoredGraph graph = m_reader.read(
        data("triangle_all_edges_diff_colors_undirected.graphml"), false, std::weak_ptr<ILogger>{});
    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_TRUE(graph.is_edges_colored());
    EXPECT_EQ(graph.get_edge_color(0, 1), graph.get_edge_color(1, 0));
    EXPECT_EQ(graph.get_edge_color(0, 2), graph.get_edge_color(2, 0));
    EXPECT_EQ(graph.get_edge_color(1, 2), graph.get_edge_color(2, 1));
    EXPECT_NE(graph.get_edge_color(0, 1), graph.get_edge_color(0, 2));
    EXPECT_NE(graph.get_edge_color(0, 1), graph.get_edge_color(1, 2));
    EXPECT_NE(graph.get_edge_color(0, 2), graph.get_edge_color(1, 2));
}

/**
 * @brief Triangle with two edges the same color, one distinct, undirected.
 */
TEST_F(GraphmlGraphReaderTest, triangle_two_edges_same_color_undirected)
{
    const ColoredGraph graph = m_reader.read(
        data("triangle_two_edges_same_color_undirected.graphml"), false, std::weak_ptr<ILogger>{});
    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_TRUE(graph.is_edges_colored());
    EXPECT_EQ(graph.get_edge_color(0, 1), graph.get_edge_color(1, 2));
    EXPECT_NE(graph.get_edge_color(0, 2), graph.get_edge_color(1, 2));
}

// ── Triangle edge colors, directed ────────────────────────────────────────────

/**
 * @brief Triangle with all edges the same color, directed.
 */
TEST_F(GraphmlGraphReaderTest, triangle_all_edges_same_color_directed)
{
    const ColoredGraph graph = m_reader.read(data("triangle_all_edges_same_color_directed.graphml"),
                                             true, std::weak_ptr<ILogger>{});
    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_TRUE(graph.is_directed());
    EXPECT_TRUE(graph.is_edges_colored());
    EXPECT_EQ(graph.get_edge_color(0, 1), graph.get_edge_color(0, 2));
}

/**
 * @brief Triangle with all edges different colors, directed.
 */
TEST_F(GraphmlGraphReaderTest, triangle_all_edges_diff_colors_directed)
{
    const ColoredGraph graph = m_reader.read(
        data("triangle_all_edges_diff_colors_directed.graphml"), true, std::weak_ptr<ILogger>{});
    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_TRUE(graph.is_edges_colored());
    EXPECT_NE(graph.get_edge_color(0, 1), graph.get_edge_color(0, 2));
    EXPECT_NE(graph.get_edge_color(1, 2), graph.get_edge_color(0, 2));
}

/**
 * @brief Triangle with two edges same color, one distinct, directed.
 */
TEST_F(GraphmlGraphReaderTest, triangle_two_edges_same_color_directed)
{
    const ColoredGraph graph = m_reader.read(data("triangle_two_edges_same_color_directed.graphml"),
                                             true, std::weak_ptr<ILogger>{});
    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_TRUE(graph.is_edges_colored());
    EXPECT_EQ(graph.get_edge_color(0, 1), graph.get_edge_color(1, 2));
    EXPECT_NE(graph.get_edge_color(0, 1), graph.get_edge_color(0, 2));
}

// ── Special cases ─────────────────────────────────────────────────────────────

/**
 * @brief Graphml with no edgedefault attribute: passing is_directed=true yields a directed graph.
 */
TEST_F(GraphmlGraphReaderTest, no_edgedefault_with_directed_param)
{
    EXPECT_THROW(m_reader.read(data("no_edgedefault.graphml"), true, std::weak_ptr<ILogger>{}),
                 GraphConstructionException);
}

/**
 * @brief Graphml with no color keys: vertices and edges default to color 0.
 */
TEST_F(GraphmlGraphReaderTest, no_color_keys_defaults_to_zero)
{
    const ColoredGraph graph =
        m_reader.read(data("no_color_keys.graphml"), false, std::weak_ptr<ILogger>{});
    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_EQ(graph.edge_count(), 1U);
    EXPECT_EQ(graph.get_vertex_color(0), 0U);
    EXPECT_EQ(graph.get_vertex_color(1), 0U);
    EXPECT_FALSE(graph.is_edges_colored());
}

// ── Param / file mismatch (null logger) ───────────────────────────────────────

/**
 * @brief is_directed=true on an undirected file: caller param wins, graph is directed.
 *
 * No logger is provided so the mismatch warning is silently suppressed.
 * The edge 0→1 from the file is preserved as a directed edge.
 */
TEST_F(GraphmlGraphReaderTest, directed_param_overrides_undirected_file)
{
    EXPECT_THROW(m_reader.read(data("two_nodes_one_edge_undirected.graphml"), true,
                               std::weak_ptr<ILogger>{}),
                 GraphConstructionException);
}

/**
 * @brief is_directed=false on a directed file: caller param wins, graph is undirected.
 *
 * No logger is provided so the mismatch warning is silently suppressed.
 * The single directed edge 0→1 becomes an undirected edge accessible from both vertices.
 */
TEST_F(GraphmlGraphReaderTest, undirected_param_overrides_directed_file)
{
    const ColoredGraph graph =
        m_reader.read(data("two_nodes_one_edge_directed.graphml"), false, std::weak_ptr<ILogger>{});
    EXPECT_FALSE(graph.is_directed());
    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_EQ(graph.edge_count(), 1U);
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {0});
}
