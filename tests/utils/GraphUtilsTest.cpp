#include "GraphUtils.h"

#include "BoostGraph.h"
#include "ColoredGraph.h"
#include "ColoredGraphTestHelpers.h"
#include "Constants.h"
#include "GraphConstructionException.h"
#include "InvalidArgumentException.h"

#include <boost/graph/graph_traits.hpp>
#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

using namespace sgf;
using namespace test_helpers;

// ── Helpers ───────────────────────────────────────────────────────────────────

namespace
{

/**
 * @brief Adds a vertex with the given color to @p graph and returns its descriptor.
 * @param color Vertex color.
 * @param graph The BoostGraph to add to.
 * @return The new vertex descriptor.
 */
boost::graph_traits<BoostGraph>::vertex_descriptor add_colored_vertex(const uint32_t color,
                                                                      BoostGraph& graph)
{
    VertexProperties props;
    props.m_color = color;
    return boost::add_vertex(props, graph);
}

/**
 * @brief Adds an uncolored edge between @p src and @p dest.
 * @param src Source vertex descriptor.
 * @param dest Destination vertex descriptor.
 * @param graph The BoostGraph to add to.
 */
void add_uncolored_edge(const boost::graph_traits<BoostGraph>::vertex_descriptor src,
                        const boost::graph_traits<BoostGraph>::vertex_descriptor dest,
                        BoostGraph& graph)
{
    EdgeProperties props;
    props.m_color = 0;
    boost::add_edge(src, dest, props, graph);
}

/**
 * @brief Adds a colored edge between @p src and @p dest.
 * @param src Source vertex descriptor.
 * @param dest Destination vertex descriptor.
 * @param color Edge color.
 * @param graph The BoostGraph to add to.
 */
void add_colored_edge(const boost::graph_traits<BoostGraph>::vertex_descriptor src,
                      const boost::graph_traits<BoostGraph>::vertex_descriptor dest,
                      const uint32_t color, BoostGraph& graph)
{
    EdgeProperties props;
    props.m_color = color;
    boost::add_edge(src, dest, props, graph);
}

}  // namespace

// ── Empty graph ───────────────────────────────────────────────────────────────

/**
 * @brief Converting an empty BoostGraph produces a zero-vertex, zero-edge ColoredGraph.
 */
TEST(GraphUtilsTest, empty_graph_undirected)
{
    const BoostGraph boost_graph;
    const ColoredGraph graph = GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, false);

    EXPECT_EQ(graph.vertex_count(), 0U);
    EXPECT_EQ(graph.edge_count(), 0U);
    EXPECT_FALSE(graph.is_directed());
    EXPECT_FALSE(graph.is_edges_colored());
}

/**
 * @brief Converting an empty BoostGraph as directed also gives zero vertices and edges.
 */
TEST(GraphUtilsTest, empty_graph_directed)
{
    const BoostGraph boost_graph;
    const ColoredGraph graph = GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, true);

    EXPECT_EQ(graph.vertex_count(), 0U);
    EXPECT_EQ(graph.edge_count(), 0U);
    EXPECT_TRUE(graph.is_directed());
    EXPECT_FALSE(graph.is_edges_colored());
}

// ── Single node, no edges ─────────────────────────────────────────────────────

/**
 * @brief One vertex, no edges, undirected: correct vertex count and color.
 */
TEST(GraphUtilsTest, single_node_no_edges_undirected)
{
    BoostGraph boost_graph;
    add_colored_vertex(0, boost_graph);
    const ColoredGraph graph = GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, false);

    EXPECT_EQ(graph.vertex_count(), 1U);
    EXPECT_EQ(graph.edge_count(), 0U);
    EXPECT_FALSE(graph.is_directed());
    EXPECT_EQ(graph.get_vertex_color(0), 0U);
    assert_neighbours(graph, 0, {});
}

/**
 * @brief One vertex, no edges, directed: correct vertex count and color.
 */
TEST(GraphUtilsTest, single_node_no_edges_directed)
{
    BoostGraph boost_graph;
    add_colored_vertex(0, boost_graph);
    const ColoredGraph graph = GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, true);

    EXPECT_EQ(graph.vertex_count(), 1U);
    EXPECT_EQ(graph.edge_count(), 0U);
    EXPECT_TRUE(graph.is_directed());
    EXPECT_EQ(graph.get_vertex_color(0), 0U);
    assert_neighbours(graph, 0, {});
    assert_neighbours(graph, 0, {}, true);
}

// ── Two nodes, no edges ───────────────────────────────────────────────────────

/**
 * @brief Two vertices, no edges, undirected.
 */
TEST(GraphUtilsTest, two_nodes_no_edges_undirected)
{
    BoostGraph boost_graph;
    add_colored_vertex(0, boost_graph);
    add_colored_vertex(0, boost_graph);
    const ColoredGraph graph = GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, false);

    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_EQ(graph.edge_count(), 0U);
    EXPECT_FALSE(graph.is_directed());
    assert_neighbours(graph, 0, {});
    assert_neighbours(graph, 1, {});
}

/**
 * @brief Two vertices, no edges, directed.
 */
TEST(GraphUtilsTest, two_nodes_no_edges_directed)
{
    BoostGraph boost_graph;
    add_colored_vertex(0, boost_graph);
    add_colored_vertex(0, boost_graph);
    const ColoredGraph graph = GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, true);

    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_EQ(graph.edge_count(), 0U);
    EXPECT_TRUE(graph.is_directed());
    assert_neighbours(graph, 0, {});
    assert_neighbours(graph, 1, {});
}

// ── Two nodes, one edge ───────────────────────────────────────────────────────

/**
 * @brief Two vertices with one edge, undirected: both vertices see each other as neighbour.
 */
TEST(GraphUtilsTest, two_nodes_one_edge_undirected)
{
    BoostGraph boost_graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor v0 =
        add_colored_vertex(0, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v1 =
        add_colored_vertex(0, boost_graph);
    add_uncolored_edge(v0, v1, boost_graph);
    const ColoredGraph graph = GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, false);

    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_EQ(graph.edge_count(), 1U);
    EXPECT_FALSE(graph.is_directed());
    EXPECT_FALSE(graph.is_edges_colored());
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {0});
}

/**
 * @brief Two vertices with one directed edge 0→1.
 */
TEST(GraphUtilsTest, two_nodes_one_edge_directed)
{
    BoostGraph boost_graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor v0 =
        add_colored_vertex(0, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v1 =
        add_colored_vertex(0, boost_graph);
    add_uncolored_edge(v0, v1, boost_graph);
    const ColoredGraph graph = GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, true);

    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_EQ(graph.edge_count(), 1U);
    EXPECT_TRUE(graph.is_directed());
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {});
    assert_neighbours(graph, 0, {}, true);
    assert_neighbours(graph, 1, {0}, true);
}

// ── Triangle vertex colors ─────────────────────────────────────────────────────

/**
 * @brief Triangle with all vertices the same color, undirected.
 */
TEST(GraphUtilsTest, triangle_all_same_vertex_color_undirected)
{
    BoostGraph boost_graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor v0 =
        add_colored_vertex(0, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v1 =
        add_colored_vertex(0, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v2 =
        add_colored_vertex(0, boost_graph);
    add_uncolored_edge(v0, v1, boost_graph);
    add_uncolored_edge(v1, v2, boost_graph);
    add_uncolored_edge(v0, v2, boost_graph);
    const ColoredGraph graph = GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, false);

    EXPECT_EQ(graph.vertex_count(), 3U);
    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_EQ(graph.get_vertex_color(0), 0U);
    EXPECT_EQ(graph.get_vertex_color(1), 0U);
    EXPECT_EQ(graph.get_vertex_color(2), 0U);
    EXPECT_FALSE(graph.is_directed());
    assert_neighbours(graph, 0, {1, 2});
    assert_neighbours(graph, 1, {0, 2});
    assert_neighbours(graph, 2, {0, 1});
}

/**
 * @brief Triangle with all vertices different colors, undirected.
 */
TEST(GraphUtilsTest, triangle_all_diff_vertex_colors_undirected)
{
    BoostGraph boost_graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor v0 =
        add_colored_vertex(0, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v1 =
        add_colored_vertex(1, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v2 =
        add_colored_vertex(2, boost_graph);
    add_uncolored_edge(v0, v1, boost_graph);
    add_uncolored_edge(v1, v2, boost_graph);
    add_uncolored_edge(v0, v2, boost_graph);
    const ColoredGraph graph = GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, false);

    EXPECT_EQ(graph.vertex_count(), 3U);
    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_EQ(graph.get_vertex_color(0), 0U);
    EXPECT_EQ(graph.get_vertex_color(1), 1U);
    EXPECT_EQ(graph.get_vertex_color(2), 2U);
    EXPECT_FALSE(graph.is_directed());
    assert_neighbours(graph, 0, {1, 2});
    assert_neighbours(graph, 1, {0, 2});
    assert_neighbours(graph, 2, {0, 1});
}

/**
 * @brief Triangle with two vertices sharing a color, one distinct, undirected.
 */
TEST(GraphUtilsTest, triangle_two_same_vertex_color_undirected)
{
    BoostGraph boost_graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor v0 =
        add_colored_vertex(0, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v1 =
        add_colored_vertex(0, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v2 =
        add_colored_vertex(1, boost_graph);
    add_uncolored_edge(v0, v1, boost_graph);
    add_uncolored_edge(v1, v2, boost_graph);
    add_uncolored_edge(v0, v2, boost_graph);
    const ColoredGraph graph = GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, false);

    EXPECT_EQ(graph.get_vertex_color(0), 0U);
    EXPECT_EQ(graph.get_vertex_color(1), 0U);
    EXPECT_EQ(graph.get_vertex_color(2), 1U);

    EXPECT_FALSE(graph.is_directed());
    assert_neighbours(graph, 0, {1, 2});
    assert_neighbours(graph, 1, {0, 2});
    assert_neighbours(graph, 2, {0, 1});
}

/**
 * @brief Triangle with all vertices the same color, directed.
 */
TEST(GraphUtilsTest, triangle_all_same_vertex_color_directed)
{
    BoostGraph boost_graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor v0 =
        add_colored_vertex(0, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v1 =
        add_colored_vertex(0, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v2 =
        add_colored_vertex(0, boost_graph);
    add_uncolored_edge(v0, v1, boost_graph);
    add_uncolored_edge(v1, v2, boost_graph);
    add_uncolored_edge(v0, v2, boost_graph);
    const ColoredGraph graph = GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, true);

    EXPECT_EQ(graph.get_vertex_color(0), 0U);
    EXPECT_EQ(graph.get_vertex_color(1), 0U);
    EXPECT_EQ(graph.get_vertex_color(2), 0U);

    EXPECT_EQ(graph.vertex_count(), 3U);
    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_TRUE(graph.is_directed());
    assert_neighbours(graph, 0, {1, 2});
    assert_neighbours(graph, 1, {2});
    assert_neighbours(graph, 2, {});
    assert_neighbours(graph, 0, {}, true);
    assert_neighbours(graph, 1, {0}, true);
    assert_neighbours(graph, 2, {0, 1}, true);
}

/**
 * @brief Triangle with all vertices different colors, directed.
 */
TEST(GraphUtilsTest, triangle_all_diff_vertex_colors_directed)
{
    BoostGraph boost_graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor v0 =
        add_colored_vertex(0, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v1 =
        add_colored_vertex(1, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v2 =
        add_colored_vertex(2, boost_graph);
    add_uncolored_edge(v0, v1, boost_graph);
    add_uncolored_edge(v1, v2, boost_graph);
    add_uncolored_edge(v0, v2, boost_graph);
    const ColoredGraph graph = GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, true);

    EXPECT_EQ(graph.get_vertex_color(0), 0U);
    EXPECT_EQ(graph.get_vertex_color(1), 1U);
    EXPECT_EQ(graph.get_vertex_color(2), 2U);

    EXPECT_EQ(graph.vertex_count(), 3U);
    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_TRUE(graph.is_directed());
    assert_neighbours(graph, 0, {1, 2});
    assert_neighbours(graph, 1, {2});
    assert_neighbours(graph, 2, {});
    assert_neighbours(graph, 0, {}, true);
    assert_neighbours(graph, 1, {0}, true);
    assert_neighbours(graph, 2, {0, 1}, true);
}

/**
 * @brief Triangle with two vertices same color, one distinct, directed.
 */
TEST(GraphUtilsTest, triangle_two_same_vertex_color_directed)
{
    BoostGraph boost_graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor v0 =
        add_colored_vertex(0, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v1 =
        add_colored_vertex(0, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v2 =
        add_colored_vertex(1, boost_graph);
    add_uncolored_edge(v0, v1, boost_graph);
    add_uncolored_edge(v1, v2, boost_graph);
    add_uncolored_edge(v0, v2, boost_graph);
    const ColoredGraph graph = GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, true);

    EXPECT_EQ(graph.get_vertex_color(0), 0U);
    EXPECT_EQ(graph.get_vertex_color(1), 0U);
    EXPECT_EQ(graph.get_vertex_color(2), 1U);

    EXPECT_EQ(graph.vertex_count(), 3U);
    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_TRUE(graph.is_directed());
    assert_neighbours(graph, 0, {1, 2});
    assert_neighbours(graph, 1, {2});
    assert_neighbours(graph, 2, {});
    assert_neighbours(graph, 0, {}, true);
    assert_neighbours(graph, 1, {0}, true);
    assert_neighbours(graph, 2, {0, 1}, true);
}

// ── Two nodes, two edges (bidirectional) ──────────────────────────────────────

/**
 * @brief Two directed edges 0→1 and 1→0, converted to undirected: one edge, not colored.
 */
TEST(GraphUtilsTest, two_nodes_bidirectional_uncolored_undirected)
{
    BoostGraph boost_graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor v0 =
        add_colored_vertex(0, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v1 =
        add_colored_vertex(0, boost_graph);
    add_uncolored_edge(v0, v1, boost_graph);
    add_uncolored_edge(v1, v0, boost_graph);
    const ColoredGraph graph = GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, false);

    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_FALSE(graph.is_directed());

    EXPECT_EQ(graph.edge_count(), 1U);
    EXPECT_FALSE(graph.is_edges_colored());
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {0});
}

/**
 * @brief Two directed edges 0→1 and 1→0, both uncolored, converted to directed: two edges.
 */
TEST(GraphUtilsTest, two_nodes_bidirectional_uncolored_directed)
{
    BoostGraph boost_graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor v0 =
        add_colored_vertex(0, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v1 =
        add_colored_vertex(0, boost_graph);
    add_uncolored_edge(v0, v1, boost_graph);
    add_uncolored_edge(v1, v0, boost_graph);
    const ColoredGraph graph = GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, true);

    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_TRUE(graph.is_directed());
    EXPECT_EQ(graph.edge_count(), 2U);
    EXPECT_FALSE(graph.is_edges_colored());
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {0});
    assert_neighbours(graph, 0, {1}, true);
    assert_neighbours(graph, 1, {0}, true);
}

// ── Two nodes, two parallel edges (same direction) ────────────────────────────

/**
 * @brief Two parallel 0→1 edges, uncolored, undirected: deduplicated to one edge.
 */
TEST(GraphUtilsTest, two_nodes_parallel_uncolored_undirected)
{
    BoostGraph boost_graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor v0 =
        add_colored_vertex(0, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v1 =
        add_colored_vertex(0, boost_graph);
    add_uncolored_edge(v0, v1, boost_graph);
    add_uncolored_edge(v0, v1, boost_graph);
    const ColoredGraph graph = GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, false);

    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_FALSE(graph.is_directed());
    EXPECT_EQ(graph.edge_count(), 1U);
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {0});
}

/**
 * @brief Two parallel 0→1 edges, uncolored, directed: deduplicated to one edge.
 */
TEST(GraphUtilsTest, two_nodes_parallel_uncolored_directed)
{
    BoostGraph boost_graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor v0 =
        add_colored_vertex(0, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v1 =
        add_colored_vertex(0, boost_graph);
    add_uncolored_edge(v0, v1, boost_graph);
    add_uncolored_edge(v0, v1, boost_graph);
    const ColoredGraph graph = GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, true);

    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_TRUE(graph.is_directed());
    EXPECT_EQ(graph.edge_count(), 1U);
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {});
}

// ── Two nodes, two edges, same edge color ─────────────────────────────────────

/**
 * @brief Bidirectional edges with the same color, undirected: one colored edge.
 */
TEST(GraphUtilsTest, two_nodes_bidirectional_same_edge_color_undirected)
{
    BoostGraph boost_graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor v0 =
        add_colored_vertex(0, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v1 =
        add_colored_vertex(0, boost_graph);
    add_colored_edge(v0, v1, 1, boost_graph);
    add_colored_edge(v1, v0, 1, boost_graph);
    const ColoredGraph graph = GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, false);

    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_FALSE(graph.is_directed());
    EXPECT_EQ(graph.edge_count(), 1U);
    EXPECT_TRUE(graph.is_edges_colored());
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {0});
    EXPECT_EQ(graph.get_edge_color(0, 1), 1U);
    EXPECT_EQ(graph.get_edge_color(1, 0), 1U);
}

/**
 * @brief Bidirectional edges with the same color, directed: two colored edges.
 */
TEST(GraphUtilsTest, two_nodes_bidirectional_same_edge_color_directed)
{
    BoostGraph boost_graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor v0 =
        add_colored_vertex(0, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v1 =
        add_colored_vertex(0, boost_graph);
    add_colored_edge(v0, v1, 1, boost_graph);
    add_colored_edge(v1, v0, 1, boost_graph);
    const ColoredGraph graph = GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, true);

    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_TRUE(graph.is_directed());
    EXPECT_EQ(graph.edge_count(), 2U);
    EXPECT_TRUE(graph.is_edges_colored());
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {0});
    EXPECT_EQ(graph.get_edge_color(0, 1), 1U);
    EXPECT_EQ(graph.get_edge_color(1, 0), 1U);
}

/**
 * @brief Parallel 0→1 edges with the same color, undirected: one colored edge.
 */
TEST(GraphUtilsTest, two_nodes_parallel_same_edge_color_undirected)
{
    BoostGraph boost_graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor v0 =
        add_colored_vertex(0, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v1 =
        add_colored_vertex(0, boost_graph);
    add_colored_edge(v0, v1, 1, boost_graph);
    add_colored_edge(v0, v1, 1, boost_graph);
    const ColoredGraph graph = GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, false);

    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_FALSE(graph.is_directed());
    EXPECT_EQ(graph.edge_count(), 1U);
    EXPECT_TRUE(graph.is_edges_colored());
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {0});
    EXPECT_EQ(graph.get_edge_color(0, 1), 1U);
    EXPECT_EQ(graph.get_edge_color(1, 0), 1U);
}

/**
 * @brief Parallel 0→1 edges with the same color, directed: one colored edge after dedup.
 */
TEST(GraphUtilsTest, two_nodes_parallel_same_edge_color_directed)
{
    BoostGraph boost_graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor v0 =
        add_colored_vertex(0, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v1 =
        add_colored_vertex(0, boost_graph);
    add_colored_edge(v0, v1, 1, boost_graph);
    add_colored_edge(v0, v1, 1, boost_graph);
    const ColoredGraph graph = GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, true);

    EXPECT_EQ(graph.vertex_count(), 2U);
    EXPECT_TRUE(graph.is_directed());
    EXPECT_EQ(graph.edge_count(), 1U);
    EXPECT_TRUE(graph.is_edges_colored());
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {});
    EXPECT_EQ(graph.get_edge_color(0, 1), 1U);
}

// ── Two nodes, two edges, different edge colors ───────────────────────────────

/**
 * @brief Bidirectional edges with conflicting colors, undirected: must throw.
 *
 * Converting 0→1 (color=1) and 1→0 (color=2) to undirected creates a conflict:
 * both reverse edges give the same endpoint pair different colors.
 */
TEST(GraphUtilsTest, two_nodes_bidirectional_diff_edge_colors_undirected_throws)
{
    BoostGraph boost_graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor v0 =
        add_colored_vertex(0, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v1 =
        add_colored_vertex(0, boost_graph);
    add_colored_edge(v0, v1, 1, boost_graph);
    add_colored_edge(v1, v0, 2, boost_graph);
    EXPECT_THROW(GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, false),
                 InvalidArgumentException);
}

/**
 * @brief Bidirectional edges with different colors, directed: two separately colored edges.
 */
TEST(GraphUtilsTest, two_nodes_bidirectional_diff_edge_colors_directed)
{
    BoostGraph boost_graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor v0 =
        add_colored_vertex(0, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v1 =
        add_colored_vertex(0, boost_graph);
    add_colored_edge(v0, v1, 1, boost_graph);
    add_colored_edge(v1, v0, 2, boost_graph);
    const ColoredGraph graph = GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, true);

    EXPECT_EQ(graph.edge_count(), 2U);
    EXPECT_TRUE(graph.is_edges_colored());
    EXPECT_EQ(graph.get_edge_color(0, 1), 1U);
    EXPECT_EQ(graph.get_edge_color(1, 0), 2U);
    assert_neighbours(graph, 0, {1});
    assert_neighbours(graph, 1, {0});
}

/**
 * @brief Parallel 0→1 edges with conflicting colors must throw.
 */
TEST(GraphUtilsTest, two_nodes_parallel_diff_edge_colors_undirected_throws)
{
    BoostGraph boost_graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor v0 =
        add_colored_vertex(0, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v1 =
        add_colored_vertex(0, boost_graph);
    add_colored_edge(v0, v1, 1, boost_graph);
    add_colored_edge(v0, v1, 2, boost_graph);
    EXPECT_THROW(GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, false),
                 InvalidArgumentException);
}

/**
 * @brief Parallel 0→1 edges with conflicting colors, directed, must throw.
 */
TEST(GraphUtilsTest, two_nodes_parallel_diff_edge_colors_directed_throws)
{
    BoostGraph boost_graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor v0 =
        add_colored_vertex(0, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v1 =
        add_colored_vertex(0, boost_graph);
    add_colored_edge(v0, v1, 1, boost_graph);
    add_colored_edge(v0, v1, 2, boost_graph);
    EXPECT_THROW(GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, true),
                 InvalidArgumentException);
}

// ── Triangle edge colors ───────────────────────────────────────────────────────

/**
 * @brief Triangle with all edges the same color, undirected.
 */
TEST(GraphUtilsTest, triangle_all_edges_same_color_undirected)
{
    BoostGraph boost_graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor v0 =
        add_colored_vertex(0, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v1 =
        add_colored_vertex(0, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v2 =
        add_colored_vertex(0, boost_graph);
    add_colored_edge(v0, v1, 1, boost_graph);
    add_colored_edge(v1, v2, 1, boost_graph);
    add_colored_edge(v0, v2, 1, boost_graph);
    const ColoredGraph graph = GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, false);

    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_TRUE(graph.is_edges_colored());
    EXPECT_EQ(graph.get_edge_color(0, 1), 1U);
    EXPECT_EQ(graph.get_edge_color(1, 2), 1U);
    EXPECT_EQ(graph.get_edge_color(0, 2), 1U);
}

/**
 * @brief Triangle with all edges different colors, undirected.
 *
 * Edges: (0,1)=1, (1,2)=2, (0,2)=3. Neighbours sorted ascending, colors aligned.
 */
TEST(GraphUtilsTest, triangle_all_edges_diff_colors_undirected)
{
    BoostGraph boost_graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor v0 =
        add_colored_vertex(0, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v1 =
        add_colored_vertex(0, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v2 =
        add_colored_vertex(0, boost_graph);
    add_colored_edge(v0, v1, 1, boost_graph);
    add_colored_edge(v1, v2, 2, boost_graph);
    add_colored_edge(v0, v2, 3, boost_graph);
    const ColoredGraph graph = GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, false);

    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_TRUE(graph.is_edges_colored());
    // vertex 0: neighbours [1,2], edge colors [1,3]
    assert_edge_colors(graph, 0, {1, 3});
    // vertex 1: neighbours [0,2], edge colors [1,2]
    assert_edge_colors(graph, 1, {1, 2});
    // vertex 2: neighbours [0,1], edge colors [3,2]
    assert_edge_colors(graph, 2, {3, 2});
}

/**
 * @brief Triangle with two edges sharing a color and one distinct, undirected.
 */
TEST(GraphUtilsTest, triangle_two_edges_same_color_undirected)
{
    BoostGraph boost_graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor v0 =
        add_colored_vertex(0, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v1 =
        add_colored_vertex(0, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v2 =
        add_colored_vertex(0, boost_graph);
    add_colored_edge(v0, v1, 1, boost_graph);
    add_colored_edge(v1, v2, 1, boost_graph);
    add_colored_edge(v0, v2, 2, boost_graph);
    const ColoredGraph graph = GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, false);

    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_TRUE(graph.is_edges_colored());
    EXPECT_EQ(graph.get_edge_color(0, 1), 1U);
    EXPECT_EQ(graph.get_edge_color(1, 2), 1U);
    EXPECT_EQ(graph.get_edge_color(0, 2), 2U);
}

/**
 * @brief Triangle with all edges the same color, directed.
 */
TEST(GraphUtilsTest, triangle_all_edges_same_color_directed)
{
    BoostGraph boost_graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor v0 =
        add_colored_vertex(0, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v1 =
        add_colored_vertex(0, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v2 =
        add_colored_vertex(0, boost_graph);
    add_colored_edge(v0, v1, 1, boost_graph);
    add_colored_edge(v1, v2, 1, boost_graph);
    add_colored_edge(v0, v2, 1, boost_graph);
    const ColoredGraph graph = GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, true);

    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_TRUE(graph.is_edges_colored());
    EXPECT_TRUE(graph.is_directed());
    assert_edge_colors(graph, 0, {1, 1});
    assert_edge_colors(graph, 1, {1});
}

/**
 * @brief Triangle with all edges different colors, directed.
 */
TEST(GraphUtilsTest, triangle_all_edges_diff_colors_directed)
{
    BoostGraph boost_graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor v0 =
        add_colored_vertex(0, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v1 =
        add_colored_vertex(0, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v2 =
        add_colored_vertex(0, boost_graph);
    add_colored_edge(v0, v1, 1, boost_graph);
    add_colored_edge(v1, v2, 2, boost_graph);
    add_colored_edge(v0, v2, 3, boost_graph);
    const ColoredGraph graph = GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, true);

    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_TRUE(graph.is_edges_colored());
    // vertex 0 out-edges: [1,2], colors [1,3]
    assert_edge_colors(graph, 0, {1, 3});
    // vertex 1 out-edges: [2], color [2]
    assert_edge_colors(graph, 1, {2});
    // vertex 2 out-edges: []
    assert_edge_colors(graph, 2, {});
}

/**
 * @brief Triangle with two edges same color, one distinct, directed.
 */
TEST(GraphUtilsTest, triangle_two_edges_same_color_directed)
{
    BoostGraph boost_graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor v0 =
        add_colored_vertex(0, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v1 =
        add_colored_vertex(0, boost_graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v2 =
        add_colored_vertex(0, boost_graph);
    add_colored_edge(v0, v1, 1, boost_graph);
    add_colored_edge(v1, v2, 1, boost_graph);
    add_colored_edge(v0, v2, 2, boost_graph);
    const ColoredGraph graph = GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, true);

    EXPECT_EQ(graph.edge_count(), 3U);
    EXPECT_TRUE(graph.is_edges_colored());
    assert_edge_colors(graph, 0, {1, 2});
    assert_edge_colors(graph, 1, {1});
}

// ── Exception: vertex color overflow ─────────────────────────────────────────

/**
 * @brief A vertex color exceeding MAX_VERTEX_COLOR must throw GraphConstructionException.
 */
TEST(GraphUtilsTest, vertex_color_exceeds_max_throws)
{
    BoostGraph boost_graph;
    add_colored_vertex(SgfConstants::MAX_VERTEX_COLOR + 1, boost_graph);
    EXPECT_THROW(GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, false),
                 GraphConstructionException);
}
