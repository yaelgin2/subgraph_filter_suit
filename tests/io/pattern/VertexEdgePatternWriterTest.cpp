#include "VertexEdgePatternWriter.h"

#include "BoostGraph.h"
#include "IOConstants.h"
#include <array>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_traits.hpp>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace sgf;

// ── Fixture ───────────────────────────────────────────────────────────────────

/**
 * @brief Test fixture for VertexEdgePatternWriter.
 *
 * Each test gets a unique temporary directory derived from the test name.
 * The writer is exercised against BoostGraph instances built in memory;
 * no reader dependency is required.  Output files are read back with simple
 * line-by-line parsers to verify content.
 */
class VertexEdgePatternWriterTest : public ::testing::Test
{
protected:
    /**
     * @brief Creates a unique temp directory for this test instance.
     */
    void SetUp() override
    {
        const ::testing::TestInfo* const info =
            ::testing::UnitTest::GetInstance()->current_test_info();
        m_temp_dir =
            std::filesystem::temp_directory_path() / ("sgf_pw_" + std::string(info->name()));
        std::filesystem::create_directories(m_temp_dir);
        m_base = (m_temp_dir / "graph").string();
    }

    /**
     * @brief Removes the temp directory and all written files.
     */
    void TearDown() override
    {
        std::filesystem::remove_all(m_temp_dir);
    }

    /**
     * @brief Parses the .node_labels file into a vertex-id to color map.
     * @param base_path Base path without suffix.
     * @return Map of vertex id to color.
     */
    static std::map<uint32_t, uint32_t> parse_node_labels(const std::string& base_path)
    {
        std::ifstream file(base_path + IOConstants::NODE_LABELS_SUFFIX);
        std::map<uint32_t, uint32_t> result;
        std::string line;
        while (std::getline(file, line))
        {
            if (line.empty())
            {
                continue;
            }
            std::istringstream stream(line);
            uint32_t vertex_id = 0;
            uint32_t color = 0;
            stream >> vertex_id >> color;
            result[vertex_id] = color;
        }
        return result;
    }

    /**
     * @brief Parses the .edge file into a list of (src, dst, color) triples.
     * @param base_path Base path without suffix.
     * @return Vector of {src, dst, color} arrays in file order.
     */
    static std::vector<std::array<uint32_t, 3>> parse_edges(const std::string& base_path)
    {
        std::ifstream file(base_path + IOConstants::EDGE_SUFFIX);
        std::vector<std::array<uint32_t, 3>> result;
        std::string line;
        while (std::getline(file, line))
        {
            if (line.empty())
            {
                continue;
            }
            std::istringstream stream(line);
            uint32_t src = 0;
            uint32_t dst = 0;
            uint32_t color = 0;
            stream >> src >> dst >> color;
            result.push_back({src, dst, color});
        }
        return result;
    }

    std::filesystem::path m_temp_dir;
    std::string m_base;
    VertexEdgePatternWriter m_writer;
};

// ── Empty graph ───────────────────────────────────────────────────────────────

/**
 * @brief Empty graph produces empty node_labels and edge files.
 */
TEST_F(VertexEdgePatternWriterTest, empty_graph_creates_empty_files)
{
    const BoostGraph graph;
    m_writer.write(graph, m_base);
    EXPECT_TRUE(parse_node_labels(m_base).empty());
    EXPECT_TRUE(parse_edges(m_base).empty());
}

// ── Single vertex ─────────────────────────────────────────────────────────────

/**
 * @brief Single vertex with default color (0) writes exactly one node_labels line with color 0.
 */
TEST_F(VertexEdgePatternWriterTest, single_vertex_default_color_writes_vertex_line)
{
    BoostGraph graph;
    boost::add_vertex(VertexProperties{0}, graph);
    m_writer.write(graph, m_base);
    const std::map<uint32_t, uint32_t> labels = parse_node_labels(m_base);
    ASSERT_EQ(labels.size(), 1U);
    EXPECT_EQ(labels.at(0U), 0U);
    EXPECT_TRUE(parse_edges(m_base).empty());
}

/**
 * @brief Single vertex with a non-default color writes the correct color value.
 */
TEST_F(VertexEdgePatternWriterTest, single_vertex_custom_color_writes_correct_color)
{
    BoostGraph graph;
    boost::add_vertex(VertexProperties{7}, graph);
    m_writer.write(graph, m_base);
    const std::map<uint32_t, uint32_t> labels = parse_node_labels(m_base);
    ASSERT_EQ(labels.size(), 1U);
    EXPECT_EQ(labels.at(0U), 7U);
    EXPECT_TRUE(parse_edges(m_base).empty());
}

// ── Two vertices, no edges ────────────────────────────────────────────────────

/**
 * @brief Two vertices with different colors and no edges write two label lines and no edge lines.
 */
TEST_F(VertexEdgePatternWriterTest, two_vertices_no_edges_writes_no_edge_lines)
{
    BoostGraph graph;
    boost::add_vertex(VertexProperties{3}, graph);
    boost::add_vertex(VertexProperties{7}, graph);
    m_writer.write(graph, m_base);
    const std::map<uint32_t, uint32_t> labels = parse_node_labels(m_base);
    ASSERT_EQ(labels.size(), 2U);
    EXPECT_EQ(labels.at(0U), 3U);
    EXPECT_EQ(labels.at(1U), 7U);
    EXPECT_TRUE(parse_edges(m_base).empty());
}

// ── Two vertices, one edge ────────────────────────────────────────────────────

/**
 * @brief Two vertices with one edge of color 0 writes the edge line with color 0.
 *
 * The writer always emits edge colors because BoostGraph always carries edge
 * color properties, even when the value is the default zero.
 */
TEST_F(VertexEdgePatternWriterTest, two_vertices_one_edge_default_color_writes_zero_color)
{
    BoostGraph graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor src =
        boost::add_vertex(VertexProperties{0}, graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor dst =
        boost::add_vertex(VertexProperties{0}, graph);
    boost::add_edge(src, dst, EdgeProperties{0}, graph);
    m_writer.write(graph, m_base);
    const std::vector<std::array<uint32_t, 3>> edges = parse_edges(m_base);
    ASSERT_EQ(edges.size(), 1U);
    EXPECT_EQ(edges[0][0], 0U);
    EXPECT_EQ(edges[0][1], 1U);
    EXPECT_EQ(edges[0][2], 0U);
}

/**
 * @brief Two vertices with one edge of a non-default color writes the correct edge color.
 */
TEST_F(VertexEdgePatternWriterTest, two_vertices_one_edge_custom_color_writes_correct_color)
{
    BoostGraph graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor src =
        boost::add_vertex(VertexProperties{0}, graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor dst =
        boost::add_vertex(VertexProperties{0}, graph);
    boost::add_edge(src, dst, EdgeProperties{9}, graph);
    m_writer.write(graph, m_base);
    const std::vector<std::array<uint32_t, 3>> edges = parse_edges(m_base);
    ASSERT_EQ(edges.size(), 1U);
    EXPECT_EQ(edges[0][0], 0U);
    EXPECT_EQ(edges[0][1], 1U);
    EXPECT_EQ(edges[0][2], 9U);
}

// ── Triangle: vertex colors ───────────────────────────────────────────────────

/**
 * @brief Triangle with all vertices the same color writes three identical vertex colors.
 *
 * Edge iteration order for adjacency_list<vecS, vecS, directedS> is deterministic:
 * out-edges of vertex 0 first (insertion order), then vertex 1, etc.
 * Edges added as v0→v1, v0→v2, v1→v2 appear in that order in the output.
 */
TEST_F(VertexEdgePatternWriterTest, triangle_same_vertex_color_writes_correct_labels)
{
    BoostGraph graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor v0 =
        boost::add_vertex(VertexProperties{2}, graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v1 =
        boost::add_vertex(VertexProperties{2}, graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v2 =
        boost::add_vertex(VertexProperties{2}, graph);
    boost::add_edge(v0, v1, EdgeProperties{0}, graph);
    boost::add_edge(v0, v2, EdgeProperties{0}, graph);
    boost::add_edge(v1, v2, EdgeProperties{0}, graph);
    m_writer.write(graph, m_base);
    const std::map<uint32_t, uint32_t> labels = parse_node_labels(m_base);
    ASSERT_EQ(labels.size(), 3U);
    EXPECT_EQ(labels.at(0U), 2U);
    EXPECT_EQ(labels.at(1U), 2U);
    EXPECT_EQ(labels.at(2U), 2U);
    const std::vector<std::array<uint32_t, 3>> edges = parse_edges(m_base);
    ASSERT_EQ(edges.size(), 3U);
    EXPECT_EQ(edges[0][0], 0U);
    EXPECT_EQ(edges[0][1], 1U);
    EXPECT_EQ(edges[0][2], 0U);
    EXPECT_EQ(edges[1][0], 0U);
    EXPECT_EQ(edges[1][1], 2U);
    EXPECT_EQ(edges[1][2], 0U);
    EXPECT_EQ(edges[2][0], 1U);
    EXPECT_EQ(edges[2][1], 2U);
    EXPECT_EQ(edges[2][2], 0U);
}

/**
 * @brief Triangle with all different vertex colors writes each color at the correct vertex id.
 */
TEST_F(VertexEdgePatternWriterTest, triangle_different_vertex_colors_writes_correct_labels)
{
    BoostGraph graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor v0 =
        boost::add_vertex(VertexProperties{1}, graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v1 =
        boost::add_vertex(VertexProperties{2}, graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v2 =
        boost::add_vertex(VertexProperties{3}, graph);
    boost::add_edge(v0, v1, EdgeProperties{0}, graph);
    boost::add_edge(v0, v2, EdgeProperties{0}, graph);
    boost::add_edge(v1, v2, EdgeProperties{0}, graph);
    m_writer.write(graph, m_base);
    const std::map<uint32_t, uint32_t> labels = parse_node_labels(m_base);
    ASSERT_EQ(labels.size(), 3U);
    EXPECT_EQ(labels.at(0U), 1U);
    EXPECT_EQ(labels.at(1U), 2U);
    EXPECT_EQ(labels.at(2U), 3U);
    const std::vector<std::array<uint32_t, 3>> edges = parse_edges(m_base);
    ASSERT_EQ(edges.size(), 3U);
    EXPECT_EQ(edges[0][0], 0U);
    EXPECT_EQ(edges[0][1], 1U);
    EXPECT_EQ(edges[1][0], 0U);
    EXPECT_EQ(edges[1][1], 2U);
    EXPECT_EQ(edges[2][0], 1U);
    EXPECT_EQ(edges[2][1], 2U);
}

// ── Triangle: edge colors ─────────────────────────────────────────────────────

/**
 * @brief Triangle with all edges the same color writes the correct src, dst, and color for each.
 */
TEST_F(VertexEdgePatternWriterTest, triangle_same_edge_color_writes_correct_edges)
{
    BoostGraph graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor v0 =
        boost::add_vertex(VertexProperties{0}, graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v1 =
        boost::add_vertex(VertexProperties{0}, graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v2 =
        boost::add_vertex(VertexProperties{0}, graph);
    boost::add_edge(v0, v1, EdgeProperties{5}, graph);
    boost::add_edge(v0, v2, EdgeProperties{5}, graph);
    boost::add_edge(v1, v2, EdgeProperties{5}, graph);
    m_writer.write(graph, m_base);
    const std::vector<std::array<uint32_t, 3>> edges = parse_edges(m_base);
    ASSERT_EQ(edges.size(), 3U);
    EXPECT_EQ(edges[0][0], 0U);
    EXPECT_EQ(edges[0][1], 1U);
    EXPECT_EQ(edges[0][2], 5U);
    EXPECT_EQ(edges[1][0], 0U);
    EXPECT_EQ(edges[1][1], 2U);
    EXPECT_EQ(edges[1][2], 5U);
    EXPECT_EQ(edges[2][0], 1U);
    EXPECT_EQ(edges[2][1], 2U);
    EXPECT_EQ(edges[2][2], 5U);
}

/**
 * @brief Triangle with all edges having different colors writes each edge's color correctly.
 */
TEST_F(VertexEdgePatternWriterTest, triangle_different_edge_colors_writes_correct_edges)
{
    BoostGraph graph;
    const boost::graph_traits<BoostGraph>::vertex_descriptor v0 =
        boost::add_vertex(VertexProperties{0}, graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v1 =
        boost::add_vertex(VertexProperties{0}, graph);
    const boost::graph_traits<BoostGraph>::vertex_descriptor v2 =
        boost::add_vertex(VertexProperties{0}, graph);
    boost::add_edge(v0, v1, EdgeProperties{1}, graph);
    boost::add_edge(v0, v2, EdgeProperties{2}, graph);
    boost::add_edge(v1, v2, EdgeProperties{3}, graph);
    m_writer.write(graph, m_base);
    const std::vector<std::array<uint32_t, 3>> edges = parse_edges(m_base);
    ASSERT_EQ(edges.size(), 3U);
    EXPECT_EQ(edges[0][0], 0U);
    EXPECT_EQ(edges[0][1], 1U);
    EXPECT_EQ(edges[0][2], 1U);
    EXPECT_EQ(edges[1][0], 0U);
    EXPECT_EQ(edges[1][1], 2U);
    EXPECT_EQ(edges[1][2], 2U);
    EXPECT_EQ(edges[2][0], 1U);
    EXPECT_EQ(edges[2][1], 2U);
    EXPECT_EQ(edges[2][2], 3U);
}

// ── Error handling ────────────────────────────────────────────────────────────

/**
 * @brief Writing to a path whose parent directory does not exist creates the directory.
 */
TEST_F(VertexEdgePatternWriterTest, write_creates_nonexistent_parent_directory)
{
    const BoostGraph graph;
    const std::filesystem::path new_dir = m_temp_dir / "new_subdir";
    const std::string base_path = (new_dir / "graph").string();
    EXPECT_NO_THROW(m_writer.write(graph, base_path));
    EXPECT_TRUE(std::filesystem::exists(base_path + IOConstants::NODE_LABELS_SUFFIX));
    EXPECT_TRUE(std::filesystem::exists(base_path + IOConstants::EDGE_SUFFIX));
}
