#include "JsonGraphReader.h"

#include "ColoredGraph.h"
#include "ColoredGraphTestHelpers.h"
#include "GraphConstructionException.h"
#include "ILogger.h"
#include "InvalidArgumentException.h"
#include "LoggerHandler.h"
#include "SgfPathDoesntExistException.h"

#include <gtest/gtest.h>
#include <memory>
#include <string>

using namespace sgf;
using namespace test_helpers;

// ── Fixture ───────────────────────────────────────────────────────────────────

/**
 * @brief Test fixture for JsonGraphReader.
 *
 * Provides the reader instance and a helper for constructing absolute paths to
 * the json test-data directory (injected at compile time via CMake).
 */
class JsonGraphReaderTest : public ::testing::Test
{
protected:
    /**
     * @brief Returns the absolute path to a file in the json test-data directory.
     * @param filename Filename relative to the json test-data directory.
     * @return Absolute path string.
     */
    static std::string data(const std::string& filename)
    {
        return std::string(JSON_TEST_DATA_DIR) + "/" + filename;
    }

    JsonGraphReader m_reader;
};

// ── Invalid path ──────────────────────────────────────────────────────────────

/**
 * @brief Reading a path that does not exist must throw SgfPathDoesntExistException.
 */
TEST_F(JsonGraphReaderTest, nonexistent_path_throws_path_doesnt_exist)
{
    EXPECT_THROW(m_reader.read(data("does_not_exist.json"), false, LoggerHandler(std::weak_ptr<ILogger>{})),
                 SgfPathDoesntExistException);
}

// ── Parse errors ──────────────────────────────────────────────────────────────

/**
 * @brief Malformed JSON must throw GraphConstructionException.
 */
TEST_F(JsonGraphReaderTest, malformed_json_throws_graph_construction)
{
    EXPECT_THROW(m_reader.read(data("malformed_json.json"), false, LoggerHandler(std::weak_ptr<ILogger>{})),
                 GraphConstructionException);
}

/**
 * @brief Valid JSON but invalid graph structure (link to undefined node ID) must throw
 * GraphConstructionException.
 */
TEST_F(JsonGraphReaderTest, invalid_graph_structure_throws_graph_construction)
{
    EXPECT_THROW(
        m_reader.read(data("link_unknown_node.json"), false, LoggerHandler(std::weak_ptr<ILogger>{})),
        GraphConstructionException);
}

// ── JSON-specific structural errors ──────────────────────────────────────────

/**
 * @brief A JSON root that is an array (not an object) must throw GraphConstructionException.
 */
TEST_F(JsonGraphReaderTest, non_object_root_throws_graph_construction)
{
    EXPECT_THROW(m_reader.read(data("array_root.json"), false, LoggerHandler(std::weak_ptr<ILogger>{})),
                 GraphConstructionException);
}

/**
 * @brief A JSON object missing the "nodes" key must throw GraphConstructionException.
 */
TEST_F(JsonGraphReaderTest, missing_nodes_key_throws_graph_construction)
{
    EXPECT_THROW(m_reader.read(data("missing_nodes_key.json"), false, LoggerHandler(std::weak_ptr<ILogger>{})),
                 GraphConstructionException);
}

/**
 * @brief A JSON object missing the "links" key must throw GraphConstructionException.
 */
TEST_F(JsonGraphReaderTest, missing_links_key_throws_graph_construction)
{
    EXPECT_THROW(m_reader.read(data("missing_links_key.json"), false, LoggerHandler(std::weak_ptr<ILogger>{})),
                 GraphConstructionException);
}

/**
 * @brief A node object missing the "id" field must throw GraphConstructionException.
 */
TEST_F(JsonGraphReaderTest, missing_node_id_throws_graph_construction)
{
    EXPECT_THROW(m_reader.read(data("missing_node_id.json"), false, LoggerHandler(std::weak_ptr<ILogger>{})),
                 GraphConstructionException);
}

/**
 * @brief A node object missing the "color" field must throw GraphConstructionException.
 */
TEST_F(JsonGraphReaderTest, missing_node_color_throws_graph_construction)
{
    EXPECT_THROW(m_reader.read(data("missing_node_color.json"), false, LoggerHandler(std::weak_ptr<ILogger>{})),
                 GraphConstructionException);
}

/**
 * @brief A link object missing the "source" field must throw GraphConstructionException.
 */
TEST_F(JsonGraphReaderTest, missing_link_source_throws_graph_construction)
{
    EXPECT_THROW(m_reader.read(data("missing_link_source.json"), false, LoggerHandler(std::weak_ptr<ILogger>{})),
                 GraphConstructionException);
}

/**
 * @brief A link object missing the "target" field must throw GraphConstructionException.
 */
TEST_F(JsonGraphReaderTest, missing_link_target_throws_graph_construction)
{
    EXPECT_THROW(m_reader.read(data("missing_link_target.json"), false, LoggerHandler(std::weak_ptr<ILogger>{})),
                 GraphConstructionException);
}

/**
 * @brief A links array where some links have a "color" field and others do not must throw
 * GraphConstructionException (all-or-nothing rule).
 */
TEST_F(JsonGraphReaderTest, mixed_edge_colors_throws_graph_construction)
{
    EXPECT_THROW(m_reader.read(data("mixed_edge_colors.json"), false, LoggerHandler(std::weak_ptr<ILogger>{})),
                 GraphConstructionException);
}

// ── Empty graph ───────────────────────────────────────────────────────────────

/**
 * @brief An empty JSON graph produces a zero-vertex undirected, zero-edge ColoredGraph.
 */
TEST_F(JsonGraphReaderTest, empty_graph_undirected)
{
    const ColoredGraph graph =
        m_reader.read(data("empty_undirected.json"), false, LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 0U);
    EXPECT_EQ(graph.edge_count(), 0U);
    EXPECT_FALSE(graph.is_directed());
}

/**
 * @brief An empty JSON graph produces a zero-vertex directed, zero-edge ColoredGraph.
 */
TEST_F(JsonGraphReaderTest, empty_graph_directed)
{
    const ColoredGraph graph =
        m_reader.read(data("empty_directed.json"), true, LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 0U);
    EXPECT_EQ(graph.edge_count(), 0U);
    EXPECT_TRUE(graph.is_directed());
}

// ── Single node ───────────────────────────────────────────────────────────────

/**
 * @brief Single-node undirected JSON graph: one vertex, no edges, not directed.
 */
TEST_F(JsonGraphReaderTest, single_node_undirected)
{
    const ColoredGraph graph =
        m_reader.read(data("single_node_undirected.json"), false, LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 1U);
    EXPECT_EQ(graph.edge_count(), 0U);
    EXPECT_FALSE(graph.is_directed());
    EXPECT_EQ(graph.get_vertex_color(0), 0U);
    assert_neighbours(graph, 0, {});
}

/**
 * @brief Single-node directed JSON graph: one vertex, no edges, directed.
 */
TEST_F(JsonGraphReaderTest, single_node_directed)
{
    const ColoredGraph graph =
        m_reader.read(data("single_node_directed.json"), true, LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 1U);
    EXPECT_EQ(graph.edge_count(), 0U);
    EXPECT_TRUE(graph.is_directed());
    assert_neighbours(graph, 0, {});
    assert_neighbours(graph, 0, {}, true);
}

// ── Two nodes, no edges ───────────────────────────────────────────────────────

/**
 * @brief Two-node undirected JSON graph with no edges.
 */
TEST_F(JsonGraphReaderTest, two_nodes_no_edges_undirected)
{
    const ColoredGraph graph = m_reader.read(data("two_nodes_no_edges_undirected.json"), false,
                                             LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_EQ(graph.edge_count(), 0U);
    EXPECT_FALSE(graph.is_directed());
    assert_neighbours(graph, 0, {});
    assert_neighbours(graph, 1, {});
}

/**
 * @brief Two-node directed JSON graph with no edges.
 */
TEST_F(JsonGraphReaderTest, two_nodes_no_edges_directed)
{
    const ColoredGraph graph =
        m_reader.read(data("two_nodes_no_edges_directed.json"), true, LoggerHandler(std::weak_ptr<ILogger>{}));
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
TEST_F(JsonGraphReaderTest, two_nodes_one_edge_undirected)
{
    const ColoredGraph graph = m_reader.read(data("two_nodes_one_edge_undirected.json"), false,
                                             LoggerHandler(std::weak_ptr<ILogger>{}));
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
TEST_F(JsonGraphReaderTest, two_nodes_one_edge_directed)
{
    const ColoredGraph graph =
        m_reader.read(data("two_nodes_one_edge_directed.json"), true, LoggerHandler(std::weak_ptr<ILogger>{}));
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
TEST_F(JsonGraphReaderTest, triangle_same_vertex_color_undirected)
{
    const ColoredGraph graph = m_reader.read(data("triangle_same_vertex_color_undirected.json"),
                                             false, LoggerHandler(std::weak_ptr<ILogger>{}));
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
TEST_F(JsonGraphReaderTest, triangle_diff_vertex_colors_undirected)
{
    const ColoredGraph graph = m_reader.read(data("triangle_diff_vertex_colors_undirected.json"),
                                             false, LoggerHandler(std::weak_ptr<ILogger>{}));
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
TEST_F(JsonGraphReaderTest, triangle_two_same_vertex_color_undirected)
{
    const ColoredGraph graph = m_reader.read(
        data("triangle_two_same_vertex_color_undirected.json"), false, LoggerHandler(std::weak_ptr<ILogger>{}));
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
TEST_F(JsonGraphReaderTest, triangle_same_vertex_color_directed)
{
    const ColoredGraph graph = m_reader.read(data("triangle_same_vertex_color_directed.json"),
                                             true, LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 3U);
    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_TRUE(graph.is_directed());
    assert_neighbours(graph, 0, {1, 2});
    assert_neighbours(graph, 1, {2});
    assert_neighbours(graph, 2, {});
    assert_neighbours(graph, 2, {0, 1}, true);
    assert_neighbours(graph, 1, {0}, true);
    assert_neighbours(graph, 0, {}, true);
    EXPECT_EQ(graph.get_vertex_color(0), graph.get_vertex_color(1));
    EXPECT_EQ(graph.get_vertex_color(0), graph.get_vertex_color(2));
}

/**
 * @brief Triangle with all vertices different colors, directed.
 */
TEST_F(JsonGraphReaderTest, triangle_diff_vertex_colors_directed)
{
    const ColoredGraph graph = m_reader.read(data("triangle_diff_vertex_colors_directed.json"),
                                             true, LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 3U);
    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_TRUE(graph.is_directed());
    assert_neighbours(graph, 0, {1, 2});
    assert_neighbours(graph, 1, {2});
    assert_neighbours(graph, 2, {});
    assert_neighbours(graph, 2, {0, 1}, true);
    assert_neighbours(graph, 1, {0}, true);
    assert_neighbours(graph, 0, {}, true);
    EXPECT_NE(graph.get_vertex_color(0), graph.get_vertex_color(1));
    EXPECT_NE(graph.get_vertex_color(0), graph.get_vertex_color(2));
    EXPECT_NE(graph.get_vertex_color(1), graph.get_vertex_color(2));
}

/**
 * @brief Triangle with two vertices same color, one distinct, directed.
 */
TEST_F(JsonGraphReaderTest, triangle_two_same_vertex_color_directed)
{
    const ColoredGraph graph = m_reader.read(
        data("triangle_two_same_vertex_color_directed.json"), true, LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 3U);
    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_TRUE(graph.is_directed());
    assert_neighbours(graph, 0, {1, 2});
    assert_neighbours(graph, 1, {2});
    assert_neighbours(graph, 2, {});
    assert_neighbours(graph, 2, {0, 1}, true);
    assert_neighbours(graph, 1, {0}, true);
    assert_neighbours(graph, 0, {}, true);
    EXPECT_EQ(graph.get_vertex_color(0), graph.get_vertex_color(1));
    EXPECT_NE(graph.get_vertex_color(0), graph.get_vertex_color(2));
}

// ── Two nodes: bidirectional edges ────────────────────────────────────────────

/**
 * @brief Bidirectional uncolored edges, undirected: deduplicated to one edge.
 */
TEST_F(JsonGraphReaderTest, two_nodes_bidirectional_uncolored_undirected)
{
    const ColoredGraph graph =
        m_reader.read(data("two_nodes_bidirectional_uncolored_undirected.json"), false,
                      LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.edge_count(), 1U);
    EXPECT_FALSE(graph.is_edges_colored());
    EXPECT_FALSE(graph.is_directed());
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {0});
}

/**
 * @brief Bidirectional uncolored edges, directed: two distinct directed edges.
 */
TEST_F(JsonGraphReaderTest, two_nodes_bidirectional_uncolored_directed)
{
    const ColoredGraph graph = m_reader.read(
        data("two_nodes_bidirectional_uncolored_directed.json"), true, LoggerHandler(std::weak_ptr<ILogger>{}));
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
TEST_F(JsonGraphReaderTest, two_nodes_parallel_uncolored_undirected)
{
    const ColoredGraph graph = m_reader.read(
        data("two_nodes_parallel_uncolored_undirected.json"), false, LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.edge_count(), 1U);
    EXPECT_FALSE(graph.is_edges_colored());
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {0});
}

/**
 * @brief Parallel uncolored edges, directed: deduplicated to one edge.
 */
TEST_F(JsonGraphReaderTest, two_nodes_parallel_uncolored_directed)
{
    const ColoredGraph graph = m_reader.read(data("two_nodes_parallel_uncolored_directed.json"),
                                             true, LoggerHandler(std::weak_ptr<ILogger>{}));
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
TEST_F(JsonGraphReaderTest, two_nodes_bidirectional_same_edge_color_undirected)
{
    const ColoredGraph graph =
        m_reader.read(data("two_nodes_bidirectional_same_edge_color_undirected.json"), false,
                      LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.edge_count(), 1U);
    EXPECT_TRUE(graph.is_edges_colored());
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {0});
}

/**
 * @brief Bidirectional edges with the same color, directed: two colored edges.
 */
TEST_F(JsonGraphReaderTest, two_nodes_bidirectional_same_edge_color_directed)
{
    const ColoredGraph graph =
        m_reader.read(data("two_nodes_bidirectional_same_edge_color_directed.json"), true,
                      LoggerHandler(std::weak_ptr<ILogger>{}));
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
TEST_F(JsonGraphReaderTest, two_nodes_bidirectional_diff_edge_colors_undirected_throws)
{
    EXPECT_THROW(m_reader.read(data("two_nodes_bidirectional_diff_edge_colors_undirected.json"),
                               false, LoggerHandler(std::weak_ptr<ILogger>{})),
                 InvalidArgumentException);
}

/**
 * @brief Bidirectional edges with different colors, directed: two separately colored edges.
 */
TEST_F(JsonGraphReaderTest, two_nodes_bidirectional_diff_edge_colors_directed)
{
    const ColoredGraph graph =
        m_reader.read(data("two_nodes_bidirectional_diff_edge_colors_directed.json"), true,
                      LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.edge_count(), 2U);
    EXPECT_TRUE(graph.is_edges_colored());
    EXPECT_NE(graph.get_edge_color(0, 1), graph.get_edge_color(1, 0));
}

// ── Two nodes: parallel same edge color ───────────────────────────────────────

/**
 * @brief Parallel edges with the same color, undirected: deduplicated to one colored edge.
 */
TEST_F(JsonGraphReaderTest, two_nodes_parallel_same_edge_color_undirected)
{
    const ColoredGraph graph =
        m_reader.read(data("two_nodes_parallel_same_edge_color_undirected.json"), false,
                      LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.edge_count(), 1U);
    EXPECT_TRUE(graph.is_edges_colored());
}

/**
 * @brief Parallel edges with the same color, directed: deduplicated to one colored edge.
 */
TEST_F(JsonGraphReaderTest, two_nodes_parallel_same_edge_color_directed)
{
    const ColoredGraph graph =
        m_reader.read(data("two_nodes_parallel_same_edge_color_directed.json"), true,
                      LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.edge_count(), 1U);
    EXPECT_TRUE(graph.is_edges_colored());
}

// ── Two nodes: parallel different edge colors ─────────────────────────────────

/**
 * @brief Parallel edges with conflicting colors, undirected: must throw InvalidArgumentException.
 */
TEST_F(JsonGraphReaderTest, two_nodes_parallel_diff_edge_colors_undirected_throws)
{
    EXPECT_THROW(m_reader.read(data("two_nodes_parallel_diff_edge_colors_undirected.json"),
                               false, LoggerHandler(std::weak_ptr<ILogger>{})),
                 InvalidArgumentException);
}

/**
 * @brief Parallel edges with conflicting colors, directed: must throw InvalidArgumentException.
 */
TEST_F(JsonGraphReaderTest, two_nodes_parallel_diff_edge_colors_directed_throws)
{
    EXPECT_THROW(m_reader.read(data("two_nodes_parallel_diff_edge_colors_directed.json"), true,
                               LoggerHandler(std::weak_ptr<ILogger>{})),
                 InvalidArgumentException);
}

// ── Triangle edge colors, undirected ──────────────────────────────────────────

/**
 * @brief Triangle with all edges the same color, undirected.
 */
TEST_F(JsonGraphReaderTest, triangle_all_edges_same_color_undirected)
{
    const ColoredGraph graph = m_reader.read(
        data("triangle_all_edges_same_color_undirected.json"), false, LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_TRUE(graph.is_edges_colored());
    EXPECT_EQ(graph.get_edge_color(0, 1), graph.get_edge_color(1, 2));
    EXPECT_EQ(graph.get_edge_color(1, 2), graph.get_edge_color(0, 2));
}

/**
 * @brief Triangle with all edges different colors, undirected.
 *
 * Edges: (0,1)=1, (1,2)=2, (0,2)=3. Colors are symmetric for undirected edges.
 */
TEST_F(JsonGraphReaderTest, triangle_all_edges_diff_colors_undirected)
{
    const ColoredGraph graph = m_reader.read(
        data("triangle_all_edges_diff_colors_undirected.json"), false, LoggerHandler(std::weak_ptr<ILogger>{}));
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
TEST_F(JsonGraphReaderTest, triangle_two_edges_same_color_undirected)
{
    const ColoredGraph graph = m_reader.read(
        data("triangle_two_edges_same_color_undirected.json"), false, LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_TRUE(graph.is_edges_colored());
    EXPECT_EQ(graph.get_edge_color(0, 1), graph.get_edge_color(1, 2));
    EXPECT_NE(graph.get_edge_color(0, 2), graph.get_edge_color(1, 2));
}

// ── Triangle edge colors, directed ────────────────────────────────────────────

/**
 * @brief Triangle with all edges the same color, directed.
 */
TEST_F(JsonGraphReaderTest, triangle_all_edges_same_color_directed)
{
    const ColoredGraph graph = m_reader.read(data("triangle_all_edges_same_color_directed.json"),
                                             true, LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_TRUE(graph.is_directed());
    EXPECT_TRUE(graph.is_edges_colored());
    EXPECT_EQ(graph.get_edge_color(0, 1), graph.get_edge_color(0, 2));
}

/**
 * @brief Triangle with all edges different colors, directed.
 */
TEST_F(JsonGraphReaderTest, triangle_all_edges_diff_colors_directed)
{
    const ColoredGraph graph = m_reader.read(
        data("triangle_all_edges_diff_colors_directed.json"), true, LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_TRUE(graph.is_edges_colored());
    EXPECT_NE(graph.get_edge_color(0, 1), graph.get_edge_color(0, 2));
    EXPECT_NE(graph.get_edge_color(1, 2), graph.get_edge_color(0, 2));
}

/**
 * @brief Triangle with two edges same color, one distinct, directed.
 */
TEST_F(JsonGraphReaderTest, triangle_two_edges_same_color_directed)
{
    const ColoredGraph graph = m_reader.read(data("triangle_two_edges_same_color_directed.json"),
                                             true, LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_TRUE(graph.is_edges_colored());
    EXPECT_EQ(graph.get_edge_color(0, 1), graph.get_edge_color(1, 2));
    EXPECT_NE(graph.get_edge_color(0, 1), graph.get_edge_color(0, 2));
}

// ── Default colors ────────────────────────────────────────────────────────────

/**
 * @brief JSON graph with vertex color 0 and no edge colors: vertices are color 0 and edges
 * are uncolored.
 */
TEST_F(JsonGraphReaderTest, no_color_keys_defaults_to_zero)
{
    const ColoredGraph graph =
        m_reader.read(data("no_color_keys.json"), false, LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_EQ(graph.edge_count(), 1U);
    EXPECT_EQ(graph.get_vertex_color(0), 0U);
    EXPECT_EQ(graph.get_vertex_color(1), 0U);
    EXPECT_FALSE(graph.is_edges_colored());
}

// ── Non-consecutive node IDs ──────────────────────────────────────────────────

/**
 * @brief Non-consecutive node IDs (0, 5, 10) are remapped to dense consecutive indices
 * (0, 1, 2) deterministically; vertex colors and undirected adjacency are preserved.
 */
TEST_F(JsonGraphReaderTest, non_consecutive_node_ids_undirected)
{
    const ColoredGraph graph = m_reader.read(data("non_consecutive_node_ids_undirected.json"), false,
                                             LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 3U);
    EXPECT_EQ(graph.edge_count(), 2U);
    EXPECT_FALSE(graph.is_directed());
    EXPECT_EQ(graph.get_vertex_color(0), 1U);
    EXPECT_EQ(graph.get_vertex_color(1), 2U);
    EXPECT_EQ(graph.get_vertex_color(2), 3U);
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {0, 2});
    assert_neighbours(graph, 2, {1});
}

/**
 * @brief Non-consecutive node IDs (0, 5, 10) are remapped to dense consecutive indices
 * (0, 1, 2) deterministically; vertex colors and directed adjacency are preserved.
 */
TEST_F(JsonGraphReaderTest, non_consecutive_node_ids_directed)
{
    const ColoredGraph graph = m_reader.read(data("non_consecutive_node_ids_directed.json"), true,
                                             LoggerHandler(std::weak_ptr<ILogger>{}));
    EXPECT_EQ(graph.vertex_count(), 3U);
    EXPECT_EQ(graph.edge_count(), 2U);
    EXPECT_TRUE(graph.is_directed());
    EXPECT_EQ(graph.get_vertex_color(0), 1U);
    EXPECT_EQ(graph.get_vertex_color(1), 2U);
    EXPECT_EQ(graph.get_vertex_color(2), 3U);
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {2});
    assert_neighbours(graph, 2, {});
    assert_neighbours(graph, 0, {}, true);
    assert_neighbours(graph, 1, {0}, true);
    assert_neighbours(graph, 2, {1}, true);
}
