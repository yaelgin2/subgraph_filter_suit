#include "BoostGraph.h"
#include "IOConstants.h"
#include "JsonPatternWriter.h"
#include "SgfPathDoesntExistException.h"

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <gtest/gtest.h>
#include <limits>
#include <sstream>
#include <string>

using namespace sgf;

// ── Fixture ───────────────────────────────────────────────────────────────────

/**
 * @brief Test fixture for JsonPatternIOManager::write().
 *
 * Each test builds a BoostGraph, writes it to a temporary file, then parses
 * the resulting JSON to assert structural correctness. The fixture creates and
 * destroys the temp file around every individual test case.
 */
class JsonPatternWriterTest : public ::testing::Test
{
protected:
    /**
     * @brief Adds a vertex with @p color to @p graph.
     * @return Descriptor of the new vertex.
     */
    static boost::graph_traits<BoostGraph>::vertex_descriptor
    add_vertex_with_color(BoostGraph& graph, const uint32_t color)
    {
        VertexProperties props;
        props.m_color = color;
        return boost::add_vertex(props, graph);
    }

    /**
     * @brief Adds a directed edge from @p src to @p dst with @p color to @p graph.
     */
    static void add_edge_with_color(
        BoostGraph& graph,
        const boost::graph_traits<BoostGraph>::vertex_descriptor src,
        const boost::graph_traits<BoostGraph>::vertex_descriptor dst,
        const uint32_t color)
    {
        EdgeProperties props;
        props.m_color = color;
        boost::add_edge(src, dst, props, graph);
    }

    /**
     * @brief Writes @p graph to the temp file and returns the parsed JSON root object.
     */
    boost::json::object write_and_parse(const BoostGraph& graph)
    {
        m_writer.write(graph, m_temp_path);
        std::ifstream file(m_temp_path);
        std::ostringstream buffer;
        buffer << file.rdbuf();
        const boost::json::value root_value = boost::json::parse(buffer.str());
        return root_value.as_object();
    }

    /**
     * @brief Returns the node object at position @p index in the nodes array.
     */
    static const boost::json::object& get_node(const boost::json::object& root,
                                               const std::size_t index)
    {
        return root.at(IOConstants::JSON_NODES_KEY).as_array().at(index).as_object();
    }

    /**
     * @brief Returns the link object at position @p index in the links array.
     */
    static const boost::json::object& get_link(const boost::json::object& root,
                                               const std::size_t index)
    {
        return root.at(IOConstants::JSON_LINKS_KEY).as_array().at(index).as_object();
    }

    void SetUp() override
    {
        m_temp_path = std::string(testing::TempDir()) + "json_pattern_writer_test.json";
    }

    void TearDown() override
    {
        std::remove(m_temp_path.c_str());
    }

    JsonPatternIOManager m_writer;
    std::string m_temp_path;
};

// ── Error path ────────────────────────────────────────────────────────────────

/**
 * @brief Writing to a path whose parent directory does not exist throws
 * SgfPathDoesntExistException.
 */
TEST_F(JsonPatternWriterTest, write_to_nonexistent_directory_throws_path_doesnt_exist)
{
    const BoostGraph graph;
    EXPECT_THROW(m_writer.write(graph, "/no_such_directory_xyz/test.json"),
                 SgfPathDoesntExistException);
}

// ── Empty graph ───────────────────────────────────────────────────────────────

/**
 * @brief An empty graph produces a JSON object with empty nodes and links arrays.
 */
TEST_F(JsonPatternWriterTest, empty_graph_produces_empty_nodes_and_links)
{
    const BoostGraph graph;
    const boost::json::object root = write_and_parse(graph);
    EXPECT_EQ(root.at(IOConstants::JSON_NODES_KEY).as_array().size(), 0U);
    EXPECT_EQ(root.at(IOConstants::JSON_LINKS_KEY).as_array().size(), 0U);
}

// ── Single node ───────────────────────────────────────────────────────────────

/**
 * @brief A graph with one vertex and no edges writes exactly one node with the
 * correct id and color, and an empty links array.
 */
TEST_F(JsonPatternWriterTest, single_node_no_edges_writes_correct_node)
{
    constexpr uint32_t NODE_COLOR = 7U;
    BoostGraph graph;
    add_vertex_with_color(graph, NODE_COLOR);
    const boost::json::object root = write_and_parse(graph);
    const boost::json::array& nodes = root.at(IOConstants::JSON_NODES_KEY).as_array();
    ASSERT_EQ(nodes.size(), 1U);
    EXPECT_EQ(root.at(IOConstants::JSON_LINKS_KEY).as_array().size(), 0U);
    const boost::json::object& node = get_node(root, 0U);
    EXPECT_EQ(node.at(IOConstants::JSON_NODE_ID_KEY).as_int64(), 0);
    EXPECT_EQ(node.at(IOConstants::JSON_COLOR_KEY).as_int64(), static_cast<int64_t>(NODE_COLOR));
}

// ── Two nodes, no edges ───────────────────────────────────────────────────────

/**
 * @brief Two vertices with no edges write two node objects with correct ids and
 * colors, and an empty links array.
 */
TEST_F(JsonPatternWriterTest, two_nodes_no_edges_writes_two_nodes_no_links)
{
    constexpr uint32_t COLOR_FIRST = 2U;
    constexpr uint32_t COLOR_SECOND = 4U;
    BoostGraph graph;
    add_vertex_with_color(graph, COLOR_FIRST);
    add_vertex_with_color(graph, COLOR_SECOND);
    const boost::json::object root = write_and_parse(graph);
    ASSERT_EQ(root.at(IOConstants::JSON_NODES_KEY).as_array().size(), 2U);
    EXPECT_EQ(root.at(IOConstants::JSON_LINKS_KEY).as_array().size(), 0U);
    EXPECT_EQ(get_node(root, 0U).at(IOConstants::JSON_NODE_ID_KEY).as_int64(), 0);
    EXPECT_EQ(get_node(root, 1U).at(IOConstants::JSON_NODE_ID_KEY).as_int64(), 1);
    EXPECT_EQ(get_node(root, 0U).at(IOConstants::JSON_COLOR_KEY).as_int64(),
              static_cast<int64_t>(COLOR_FIRST));
    EXPECT_EQ(get_node(root, 1U).at(IOConstants::JSON_COLOR_KEY).as_int64(),
              static_cast<int64_t>(COLOR_SECOND));
}

// ── Two nodes, one edge ───────────────────────────────────────────────────────

/**
 * @brief An edge with color zero is serialized with "color": 0.
 *
 * The writer always includes the color field, even when its value is zero.
 */
TEST_F(JsonPatternWriterTest, two_nodes_zero_color_edge_serializes_color_zero)
{
    BoostGraph graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor vertex_0 =
        add_vertex_with_color(graph, 0U);
    const boost::graph_traits<BoostGraph>::vertex_descriptor vertex_1 =
        add_vertex_with_color(graph, 0U);
    add_edge_with_color(graph, vertex_0, vertex_1, 0U);
    const boost::json::object root = write_and_parse(graph);
    ASSERT_EQ(root.at(IOConstants::JSON_LINKS_KEY).as_array().size(), 1U);
    const boost::json::object& link = get_link(root, 0U);
    EXPECT_EQ(link.at(IOConstants::JSON_SOURCE_KEY).as_int64(), 0);
    EXPECT_EQ(link.at(IOConstants::JSON_TARGET_KEY).as_int64(), 1);
    EXPECT_EQ(link.at(IOConstants::JSON_COLOR_KEY).as_int64(), 0);
}

/**
 * @brief An edge with a non-zero color is serialized with the correct color value.
 */
TEST_F(JsonPatternWriterTest, two_nodes_colored_edge_writes_correct_edge_color)
{
    constexpr uint32_t EDGE_COLOR = 9U;
    BoostGraph graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor vertex_0 =
        add_vertex_with_color(graph, 0U);
    const boost::graph_traits<BoostGraph>::vertex_descriptor vertex_1 =
        add_vertex_with_color(graph, 0U);
    add_edge_with_color(graph, vertex_0, vertex_1, EDGE_COLOR);
    const boost::json::object root = write_and_parse(graph);
    ASSERT_EQ(root.at(IOConstants::JSON_LINKS_KEY).as_array().size(), 1U);
    const boost::json::object& link = get_link(root, 0U);
    EXPECT_EQ(link.at(IOConstants::JSON_SOURCE_KEY).as_int64(), 0);
    EXPECT_EQ(link.at(IOConstants::JSON_TARGET_KEY).as_int64(), 1);
    EXPECT_EQ(link.at(IOConstants::JSON_COLOR_KEY).as_int64(), static_cast<int64_t>(EDGE_COLOR));
}

/**
 * @brief Two vertices with different colors connected by one edge produce
 * two node objects with distinct color values.
 */
TEST_F(JsonPatternWriterTest, two_nodes_different_vertex_colors_one_edge)
{
    constexpr uint32_t COLOR_0 = 3U;
    constexpr uint32_t COLOR_1 = 8U;
    constexpr uint32_t EDGE_COLOR = 1U;
    BoostGraph graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor vertex_0 =
        add_vertex_with_color(graph, COLOR_0);
    const boost::graph_traits<BoostGraph>::vertex_descriptor vertex_1 =
        add_vertex_with_color(graph, COLOR_1);
    add_edge_with_color(graph, vertex_0, vertex_1, EDGE_COLOR);
    const boost::json::object root = write_and_parse(graph);
    ASSERT_EQ(root.at(IOConstants::JSON_NODES_KEY).as_array().size(), 2U);
    EXPECT_EQ(get_node(root, 0U).at(IOConstants::JSON_COLOR_KEY).as_int64(),
              static_cast<int64_t>(COLOR_0));
    EXPECT_EQ(get_node(root, 1U).at(IOConstants::JSON_COLOR_KEY).as_int64(),
              static_cast<int64_t>(COLOR_1));
    EXPECT_EQ(get_link(root, 0U).at(IOConstants::JSON_COLOR_KEY).as_int64(),
              static_cast<int64_t>(EDGE_COLOR));
}

// ── Triangle: all same color ──────────────────────────────────────────────────

/**
 * @brief Triangle graph where all nodes and all edges share the same color.
 *
 * Adds three vertices and three directed edges (0→1, 1→2, 0→2).
 * Verifies that all three node color values and all three link color values
 * match the expected uniform color.
 */
TEST_F(JsonPatternWriterTest, triangle_all_same_vertex_and_edge_color)
{
    constexpr uint32_t VERTEX_COLOR = 5U;
    constexpr uint32_t EDGE_COLOR = 3U;
    BoostGraph graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor vertex_0 =
        add_vertex_with_color(graph, VERTEX_COLOR);
    const boost::graph_traits<BoostGraph>::vertex_descriptor vertex_1 =
        add_vertex_with_color(graph, VERTEX_COLOR);
    const boost::graph_traits<BoostGraph>::vertex_descriptor vertex_2 =
        add_vertex_with_color(graph, VERTEX_COLOR);
    add_edge_with_color(graph, vertex_0, vertex_1, EDGE_COLOR);
    add_edge_with_color(graph, vertex_1, vertex_2, EDGE_COLOR);
    add_edge_with_color(graph, vertex_0, vertex_2, EDGE_COLOR);
    const boost::json::object root = write_and_parse(graph);
    ASSERT_EQ(root.at(IOConstants::JSON_NODES_KEY).as_array().size(), 3U);
    ASSERT_EQ(root.at(IOConstants::JSON_LINKS_KEY).as_array().size(), 3U);
    const int64_t expected_vertex_color = static_cast<int64_t>(VERTEX_COLOR);
    const int64_t expected_edge_color = static_cast<int64_t>(EDGE_COLOR);
    EXPECT_EQ(get_node(root, 0U).at(IOConstants::JSON_COLOR_KEY).as_int64(), expected_vertex_color);
    EXPECT_EQ(get_node(root, 1U).at(IOConstants::JSON_COLOR_KEY).as_int64(), expected_vertex_color);
    EXPECT_EQ(get_node(root, 2U).at(IOConstants::JSON_COLOR_KEY).as_int64(), expected_vertex_color);
    EXPECT_EQ(get_link(root, 0U).at(IOConstants::JSON_COLOR_KEY).as_int64(), expected_edge_color);
    EXPECT_EQ(get_link(root, 1U).at(IOConstants::JSON_COLOR_KEY).as_int64(), expected_edge_color);
    EXPECT_EQ(get_link(root, 2U).at(IOConstants::JSON_COLOR_KEY).as_int64(), expected_edge_color);
}

// ── Triangle: all different node colors ───────────────────────────────────────

/**
 * @brief Triangle graph where all three nodes have distinct colors.
 *
 * Verifies that the writer preserves each vertex's individual color.
 */
TEST_F(JsonPatternWriterTest, triangle_all_different_vertex_colors)
{
    constexpr uint32_t COLOR_0 = 1U;
    constexpr uint32_t COLOR_1 = 2U;
    constexpr uint32_t COLOR_2 = 3U;
    BoostGraph graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor vertex_0 =
        add_vertex_with_color(graph, COLOR_0);
    const boost::graph_traits<BoostGraph>::vertex_descriptor vertex_1 =
        add_vertex_with_color(graph, COLOR_1);
    const boost::graph_traits<BoostGraph>::vertex_descriptor vertex_2 =
        add_vertex_with_color(graph, COLOR_2);
    add_edge_with_color(graph, vertex_0, vertex_1, 0U);
    add_edge_with_color(graph, vertex_1, vertex_2, 0U);
    add_edge_with_color(graph, vertex_0, vertex_2, 0U);
    const boost::json::object root = write_and_parse(graph);
    ASSERT_EQ(root.at(IOConstants::JSON_NODES_KEY).as_array().size(), 3U);
    EXPECT_EQ(get_node(root, 0U).at(IOConstants::JSON_COLOR_KEY).as_int64(),
              static_cast<int64_t>(COLOR_0));
    EXPECT_EQ(get_node(root, 1U).at(IOConstants::JSON_COLOR_KEY).as_int64(),
              static_cast<int64_t>(COLOR_1));
    EXPECT_EQ(get_node(root, 2U).at(IOConstants::JSON_COLOR_KEY).as_int64(),
              static_cast<int64_t>(COLOR_2));
}

// ── Triangle: all different edge colors ───────────────────────────────────────

/**
 * @brief Triangle graph where all three edges have distinct colors.
 *
 * Verifies that each edge's individual color is preserved independently.
 * Edge order in the JSON follows per-vertex iteration: 0→1, 0→2, 1→2.
 */
TEST_F(JsonPatternWriterTest, triangle_all_different_edge_colors)
{
    constexpr uint32_t EDGE_COLOR_01 = 4U;
    constexpr uint32_t EDGE_COLOR_12 = 5U;
    constexpr uint32_t EDGE_COLOR_02 = 6U;
    BoostGraph graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor vertex_0 =
        add_vertex_with_color(graph, 0U);
    const boost::graph_traits<BoostGraph>::vertex_descriptor vertex_1 =
        add_vertex_with_color(graph, 0U);
    const boost::graph_traits<BoostGraph>::vertex_descriptor vertex_2 =
        add_vertex_with_color(graph, 0U);
    add_edge_with_color(graph, vertex_0, vertex_1, EDGE_COLOR_01);
    add_edge_with_color(graph, vertex_1, vertex_2, EDGE_COLOR_12);
    add_edge_with_color(graph, vertex_0, vertex_2, EDGE_COLOR_02);
    const boost::json::object root = write_and_parse(graph);
    ASSERT_EQ(root.at(IOConstants::JSON_LINKS_KEY).as_array().size(), 3U);
    EXPECT_EQ(get_link(root, 0U).at(IOConstants::JSON_COLOR_KEY).as_int64(),
              static_cast<int64_t>(EDGE_COLOR_01));
    EXPECT_EQ(get_link(root, 1U).at(IOConstants::JSON_COLOR_KEY).as_int64(),
              static_cast<int64_t>(EDGE_COLOR_02));
    EXPECT_EQ(get_link(root, 2U).at(IOConstants::JSON_COLOR_KEY).as_int64(),
              static_cast<int64_t>(EDGE_COLOR_12));
}

// ── Edge cases ────────────────────────────────────────────────────────────────

/**
 * @brief UINT32_MAX vertex color is preserved exactly in the JSON output.
 *
 * Boost.JSON stores integers as int64_t; UINT32_MAX fits without loss.
 */
TEST_F(JsonPatternWriterTest, large_vertex_color_value_is_preserved)
{
    constexpr uint32_t LARGE_COLOR = std::numeric_limits<uint32_t>::max();
    BoostGraph graph;
    add_vertex_with_color(graph, LARGE_COLOR);
    const boost::json::object root = write_and_parse(graph);
    ASSERT_EQ(root.at(IOConstants::JSON_NODES_KEY).as_array().size(), 1U);
    EXPECT_EQ(get_node(root, 0U).at(IOConstants::JSON_COLOR_KEY).as_int64(),
              static_cast<int64_t>(LARGE_COLOR));
}

/**
 * @brief A self-loop edge is serialized with equal "source" and "target" values.
 *
 * The writer does not validate self-loops; it serializes whatever is in the
 * BoostGraph without modification.
 */
TEST_F(JsonPatternWriterTest, self_loop_is_serialized_with_equal_source_and_target)
{
    constexpr uint32_t LOOP_EDGE_COLOR = 5U;
    BoostGraph graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor vertex_0 =
        add_vertex_with_color(graph, 0U);
    add_edge_with_color(graph, vertex_0, vertex_0, LOOP_EDGE_COLOR);
    const boost::json::object root = write_and_parse(graph);
    ASSERT_EQ(root.at(IOConstants::JSON_LINKS_KEY).as_array().size(), 1U);
    const boost::json::object& link = get_link(root, 0U);
    EXPECT_EQ(link.at(IOConstants::JSON_SOURCE_KEY).as_int64(), 0);
    EXPECT_EQ(link.at(IOConstants::JSON_TARGET_KEY).as_int64(), 0);
    EXPECT_EQ(link.at(IOConstants::JSON_COLOR_KEY).as_int64(),
              static_cast<int64_t>(LOOP_EDGE_COLOR));
}

/**
 * @brief Two parallel edges between the same pair of vertices are both
 * serialized as separate link objects in the JSON output.
 */
TEST_F(JsonPatternWriterTest, parallel_edges_are_both_serialized)
{
    constexpr uint32_t EDGE_COLOR_FIRST = 2U;
    constexpr uint32_t EDGE_COLOR_SECOND = 7U;
    BoostGraph graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor vertex_0 =
        add_vertex_with_color(graph, 0U);
    const boost::graph_traits<BoostGraph>::vertex_descriptor vertex_1 =
        add_vertex_with_color(graph, 0U);
    add_edge_with_color(graph, vertex_0, vertex_1, EDGE_COLOR_FIRST);
    add_edge_with_color(graph, vertex_0, vertex_1, EDGE_COLOR_SECOND);
    const boost::json::object root = write_and_parse(graph);
    ASSERT_EQ(root.at(IOConstants::JSON_LINKS_KEY).as_array().size(), 2U);
    EXPECT_EQ(get_link(root, 0U).at(IOConstants::JSON_COLOR_KEY).as_int64(),
              static_cast<int64_t>(EDGE_COLOR_FIRST));
    EXPECT_EQ(get_link(root, 1U).at(IOConstants::JSON_COLOR_KEY).as_int64(),
              static_cast<int64_t>(EDGE_COLOR_SECOND));
}
