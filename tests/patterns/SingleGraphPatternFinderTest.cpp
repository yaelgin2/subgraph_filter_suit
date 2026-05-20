#include "SingleGraphPatternFinder.h"

#include "BoostGraph.h"
#include "ColoredGraph.h"
#include "FileLogger.h"
#include "InvalidArgumentException.h"
#include "LoggerHandler.h"

#include <cstdint>
#include <gtest/gtest.h>
#include <utility>
#include <vector>

using namespace sgf;

namespace
{

/**
 * @brief Per-vertex color and adjacency list extracted from a graph for isomorphism testing.
 */
struct GraphSignature
{
    std::vector<uint32_t> m_colors;
    std::vector<std::vector<uint32_t>> m_adjacency;
};

/**
 * @brief Build a GraphSignature from a BoostGraph using out-edges only.
 * @param graph Source Boost graph.
 * @return Extracted signature.
 */
GraphSignature make_boost_signature(const BoostGraph& graph)
{
    const uint32_t vertex_count = static_cast<uint32_t>(boost::num_vertices(graph));
    GraphSignature sig;
    sig.m_colors.resize(vertex_count);
    sig.m_adjacency.resize(vertex_count);

    for (uint32_t vertex_idx = 0U; vertex_idx < vertex_count; ++vertex_idx)
    {
        sig.m_colors[vertex_idx] = graph[vertex_idx].m_color;
        for (const auto& edge : boost::make_iterator_range(boost::out_edges(vertex_idx, graph)))
        {
            sig.m_adjacency[vertex_idx].push_back(
                static_cast<uint32_t>(boost::target(edge, graph)));
        }
    }

    return sig;
}

/**
 * @brief Build a GraphSignature from a ColoredGraph using out-neighbours only.
 * @param graph Source colored graph.
 * @return Extracted signature.
 */
GraphSignature make_colored_graph_signature(const ColoredGraph& graph)
{
    const uint32_t vertex_count = graph.vertex_count();
    GraphSignature sig;
    sig.m_colors.resize(vertex_count);
    sig.m_adjacency.resize(vertex_count);

    for (uint32_t vertex_idx = 0U; vertex_idx < vertex_count; ++vertex_idx)
    {
        sig.m_colors[vertex_idx] = graph.get_vertex_color(vertex_idx);
        const std::pair<std::vector<uint32_t>::const_iterator,
                        std::vector<uint32_t>::const_iterator>
            neighbour_range = graph.get_neighbours(vertex_idx);
        sig.m_adjacency[vertex_idx].assign(neighbour_range.first, neighbour_range.second);
    }

    return sig;
}

/**
 * @brief Build a GraphSignature from an edge list and per-vertex colors.
 *
 * For undirected graphs both directions are inserted; for directed only the
 * given direction is inserted, matching the out-edge representation used by
 * make_boost_signature and make_colored_graph_signature.
 *
 * @param edges Edge pairs (source, target).
 * @param colors Per-vertex color labels.
 * @param is_directed When false, the reverse edge is also added.
 * @return Extracted signature.
 */
GraphSignature make_edges_colors_signature(const std::vector<std::pair<uint32_t, uint32_t>>& edges,
                                           const std::vector<uint32_t>& colors,
                                           const bool is_directed)
{
    GraphSignature sig;
    sig.m_colors = colors;
    sig.m_adjacency.resize(colors.size());

    for (const auto& edge_pair : edges)
    {
        sig.m_adjacency[edge_pair.first].push_back(edge_pair.second);
        if (!is_directed)
        {
            sig.m_adjacency[edge_pair.second].push_back(edge_pair.first);
        }
    }

    return sig;
}

/**
 * @brief Check whether @p perm is a valid color- and edge-preserving bijection
 *        from @p candidate_sig to @p ref_sig.
 * @param candidate_sig Candidate graph signature.
 * @param ref_sig Reference graph signature.
 * @param perm Permutation mapping candidate vertex i to ref vertex perm[i].
 * @return True if all colors and directed edges are preserved under the mapping.
 */
bool is_valid_mapping(const GraphSignature& candidate_sig, const GraphSignature& ref_sig,
                      const std::vector<uint32_t>& perm)
{
    const uint32_t vertex_count = static_cast<uint32_t>(perm.size());
    for (uint32_t vertex_idx = 0U; vertex_idx < vertex_count; ++vertex_idx)
    {
        if (candidate_sig.m_colors[vertex_idx] != ref_sig.m_colors[perm[vertex_idx]])
        {
            return false;
        }
        const std::vector<uint32_t>& ref_adj = ref_sig.m_adjacency[perm[vertex_idx]];
        if (candidate_sig.m_adjacency[vertex_idx].size() != ref_adj.size())
        {
            return false;
        }
        for (const uint32_t neighbor : candidate_sig.m_adjacency[vertex_idx])
        {
            if (std::find(ref_adj.begin(), ref_adj.end(), perm[neighbor]) == ref_adj.end())
            {
                return false;
            }
        }
    }
    return true;
}

/**
 * @brief Test whether two graph signatures are isomorphic by trying every permutation
 *        of the reference vertex indices.
 * @param candidate_sig Candidate signature.
 * @param ref_sig Reference signature.
 * @return True if a color- and edge-preserving bijection exists.
 */
bool find_isomorphism(const GraphSignature& candidate_sig, const GraphSignature& ref_sig)
{
    const uint32_t vertex_count = static_cast<uint32_t>(candidate_sig.m_colors.size());
    std::vector<uint32_t> perm(vertex_count);
    for (uint32_t idx = 0U; idx < vertex_count; ++idx)
    {
        perm[idx] = idx;
    }
    do
    {
        if (is_valid_mapping(candidate_sig, ref_sig, perm))
        {
            return true;
        }
    } while (std::next_permutation(perm.begin(), perm.end()));
    return false;
}

/**
 * @brief Test whether two graph signatures are isomorphic.
 * @param candidate_sig Candidate signature.
 * @param ref_sig Reference signature.
 * @return True if a color- and edge-preserving bijection exists.
 */
bool are_graph_signatures_isomorphic(const GraphSignature& candidate_sig,
                                     const GraphSignature& ref_sig)
{
    const uint32_t vertex_count = static_cast<uint32_t>(candidate_sig.m_colors.size());
    if (vertex_count != static_cast<uint32_t>(ref_sig.m_colors.size()))
    {
        return false;
    }
    return find_isomorphism(candidate_sig, ref_sig);
}

/**
 * @brief Return true if @p pattern is isomorphic to the graph described by @p expected_edges
 *        and @p expected_colors.
 *
 * For directed graphs only the given edge direction is compared; for undirected both
 * directions are compared, matching the out-edge storage in the BoostGraph.
 *
 * @param pattern The BoostGraph to check.
 * @param expected_edges Edge pairs of the reference graph.
 * @param expected_colors Per-vertex color labels of the reference graph.
 * @param is_directed When true, edges are treated as directed.
 * @return True if an isomorphism exists.
 */
bool boost_graph_isomorphic_to(const BoostGraph& pattern,
                               const std::vector<std::pair<uint32_t, uint32_t>>& expected_edges,
                               const std::vector<uint32_t>& expected_colors, const bool is_directed)
{
    const GraphSignature pattern_sig = make_boost_signature(pattern);
    const GraphSignature ref_sig =
        make_edges_colors_signature(expected_edges, expected_colors, is_directed);
    return are_graph_signatures_isomorphic(pattern_sig, ref_sig);
}

/**
 * @brief Return true if @p pattern is isomorphic to @p reference.
 * @param pattern The BoostGraph to check.
 * @param reference The ColoredGraph to compare against.
 * @return True if an isomorphism exists.
 */
bool boost_graph_isomorphic_to(const BoostGraph& pattern, const ColoredGraph& reference)
{
    const GraphSignature pattern_sig = make_boost_signature(pattern);
    const GraphSignature ref_sig = make_colored_graph_signature(reference);
    return are_graph_signatures_isomorphic(pattern_sig, ref_sig);
}

class SingleGraphPatternFinderTest : public ::testing::Test
{
protected:
    /**
     * @brief Bundles the definition of a test graph so edges and colors are written once
     *        and shared between graph construction and pattern assertion.
     */
    struct GraphSpec
    {
        std::vector<std::pair<uint32_t, uint32_t>> m_edges;
        std::vector<uint32_t> m_colors;
        bool m_is_directed;

        /**
         * @brief Construct the ColoredGraph described by this spec.
         * @return Newly constructed ColoredGraph.
         */
        ColoredGraph to_graph() const
        {
            std::vector<std::pair<uint32_t, uint32_t>> edge_copy = m_edges;
            return ColoredGraph(static_cast<uint32_t>(m_colors.size()), edge_copy, m_colors,
                                m_is_directed);
        }
    };

    /**
     * @brief Creates a no-op LoggerHandler for use in tests.
     * @return LoggerHandler backed by an expired weak_ptr (all log calls are no-ops).
     */
    static LoggerHandler null_logger()
    {
        return LoggerHandler{std::weak_ptr<ILogger>{}};
    }

    /**
     * @brief Undirected empty graph with zero vertices and zero edges.
     * @return Empty ColoredGraph.
     */
    static ColoredGraph make_empty_graph()
    {
        std::vector<std::pair<uint32_t, uint32_t>> edges;
        const std::vector<uint32_t> colors;
        return ColoredGraph(0U, edges, colors, false);
    }

    /**
     * @brief Undirected path graph: 4 vertices (0-1-2-3), all color 0.
     * @return GraphSpec for the path-4 graph.
     */
    static GraphSpec make_path_4()
    {
        return {{{0U, 1U}, {1U, 2U}, {2U, 3U}}, {0U, 0U, 0U, 0U}, false};
    }

    /**
     * @brief Directed graph: 4 vertices, edges 0→1, 2→1, 2→3, all color 0.
     * @return GraphSpec for the directed path-4 graph.
     */
    static GraphSpec make_path_4_directed()
    {
        return {{{0U, 1U}, {2U, 1U}, {2U, 3U}}, {0U, 0U, 0U, 0U}, true};
    }

    /**
     * @brief Directed graph: 4 vertices, edges 0→1, 2→1, 2→3, distinct per-vertex colors.
     * @return GraphSpec for the directed colored path-4 graph.
     */
    static GraphSpec make_path_4_directed_colored()
    {
        return {{{0U, 1U}, {2U, 1U}, {2U, 3U}}, {0U, 1U, 2U, 3U}, true};
    }

    /**
     * @brief Undirected path graph: 4 vertices (0-1-2-3), distinct per-vertex colors.
     * @return GraphSpec for the colored path-4 graph.
     */
    static GraphSpec make_path_4_colored()
    {
        return {{{0U, 1U}, {1U, 2U}, {2U, 3U}}, {0U, 1U, 2U, 3U}, false};
    }

    /**
     * @brief Undirected graph: 5 vertices forming a path-4 plus an additional vertex.
     * @return GraphSpec for the path-4-plus-1 graph.
     */
    static GraphSpec make_path_4_with_1_added_vertex()
    {
        return {{{0U, 1U}, {2U, 1U}, {2U, 3U}, {1U, 4U}}, {0U, 0U, 0U, 0U, 0U}, false};
    }

    /**
     * @brief Directed graph: 5 vertices forming a directed path-4 plus an additional vertex.
     * @return GraphSpec for the directed path-4-plus-1 graph.
     */
    static GraphSpec make_path_4_with_1_added_vertex_directed()
    {
        return {{{0U, 1U}, {2U, 1U}, {2U, 3U}, {1U, 4U}}, {0U, 0U, 0U, 0U, 0U}, true};
    }

    /**
     * @brief Undirected triangle: 3 vertices (0-1-2-0), all color 0.
     * @return GraphSpec for the triangle-3 graph.
     */
    static GraphSpec make_triangle_3()
    {
        return {{{0U, 1U}, {1U, 2U}, {2U, 0U}}, {0U, 0U, 0U}, false};
    }

    /**
     * @brief Undirected triangle: 3 vertices, distinct per-vertex colors.
     * @return GraphSpec for the colored triangle-3 graph.
     */
    static GraphSpec make_triangle_3_colored()
    {
        return {{{0U, 1U}, {1U, 2U}, {2U, 0U}}, {0U, 1U, 2U}, false};
    }

    /**
     * @brief Directed triangle: 3 vertices, edges 0→1, 1→2, 0→2, all color 0.
     * @return GraphSpec for the directed triangle-3 graph.
     */
    static GraphSpec make_triangle_3_directed()
    {
        return {{{0U, 1U}, {1U, 2U}, {0U, 2U}}, {0U, 0U, 0U}, true};
    }

    /**
     * @brief Directed triangle: 3 vertices, edges 0→1, 1→2, 0→2, distinct per-vertex colors.
     * @return GraphSpec for the directed colored triangle-3 graph.
     */
    static GraphSpec make_triangle_3_colored_directed()
    {
        return {{{0U, 1U}, {1U, 2U}, {0U, 2U}}, {0U, 1U, 2U}, true};
    }

    /**
     * @brief Undirected star: center vertex 0 connected to leaves 1-4, all color 0.
     * @return GraphSpec for the star-5 graph.
     */
    static GraphSpec make_star_5()
    {
        return {{{0U, 1U}, {0U, 2U}, {0U, 3U}, {0U, 4U}}, {0U, 0U, 0U, 0U, 0U}, false};
    }

    /**
     * @brief Undirected 8-vertex graph with a branching path structure, all color 0.
     * @return GraphSpec for the complex graph.
     */
    static GraphSpec make_complex_graph()
    {
        return {{{0U, 1U}, {1U, 2U}, {1U, 3U}, {1U, 4U}, {4U, 5U}, {0U, 6U}, {6U, 7U}},
                {0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U},
                false};
    }

    /**
     * @brief Undirected 8-vertex graph with a branching path structure, distinct per-vertex colors.
     * @return GraphSpec for the colored complex graph.
     */
    static GraphSpec make_complex_graph_colored()
    {
        return {{{0U, 1U}, {1U, 2U}, {1U, 3U}, {1U, 4U}, {4U, 5U}, {0U, 6U}, {6U, 7U}},
                {0U, 1U, 2U, 2U, 2U, 3U, 1U, 4U},
                false};
    }

    /**
     * @brief Directed 8-vertex graph with a branching path structure, all color 0.
     * @return GraphSpec for the directed complex graph.
     */
    static GraphSpec make_complex_graph_directed()
    {
        return {{{0U, 1U}, {1U, 2U}, {1U, 3U}, {1U, 4U}, {4U, 5U}, {0U, 6U}, {6U, 7U}},
                {0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U},
                true};
    }

    /**
     * @brief Directed 8-vertex graph with a branching path structure, distinct per-vertex colors.
     * @return GraphSpec for the directed colored complex graph.
     */
    static GraphSpec make_complex_graph_colored_directed()
    {
        return {{{0U, 1U}, {1U, 2U}, {1U, 3U}, {1U, 4U}, {4U, 5U}, {0U, 6U}, {6U, 7U}},
                {0U, 1U, 2U, 2U, 2U, 3U, 1U, 4U},
                true};
    }

    /**
     * @brief Construct a finder with explicit parameters (background = graph spec).
     * @param spec GraphSpec whose graph is used as the background for the null model.
     * @return Constructed SingleGraphPatternFinder.
     */
    static SingleGraphPatternFinder make_finder(const GraphSpec& spec)
    {
        return SingleGraphPatternFinder(spec.to_graph(), spec.m_is_directed, 500U, 0.1, 0.9,
                                        null_logger());
    }

    /**
     * @brief Construct a finder with a custom background graph.
     * @param background_spec GraphSpec whose graph is used as the background for the null model.
     * @param is_directed Whether edges are treated as directed.
     * @return Constructed SingleGraphPatternFinder.
     */
    static SingleGraphPatternFinder make_finder(const GraphSpec& background_spec,
                                                const bool is_directed)
    {
        return SingleGraphPatternFinder(background_spec.to_graph(), is_directed, 500U, 0.1, 0.9,
                                        null_logger());
    }
};

TEST_F(SingleGraphPatternFinderTest, empty_background_graph_throws)
{
    EXPECT_THROW(SingleGraphPatternFinder(make_empty_graph(), false), InvalidArgumentException);
}

TEST_F(SingleGraphPatternFinderTest, empty_search_graph_throws)
{
    const GraphSpec spec = make_path_4();
    SingleGraphPatternFinder finder = make_finder(spec);
    ColoredGraph empty_graph = make_empty_graph();
    EXPECT_THROW(finder.find_pattern(empty_graph, 0.0), InvalidArgumentException);
}

TEST_F(SingleGraphPatternFinderTest, path_4_in_itself_found_graph)
{
    const GraphSpec spec = make_path_4();
    SingleGraphPatternFinder finder = make_finder(spec);
    ColoredGraph graph = spec.to_graph();

    const std::vector<BoostGraph> result = finder.find_pattern(graph, -1000.0);

    ASSERT_FALSE(result.empty());

    for (uint32_t idx = 0U; idx < static_cast<uint32_t>(result.size()); ++idx)
    {
        EXPECT_TRUE(
            boost_graph_isomorphic_to(result[idx], spec.m_edges, spec.m_colors, spec.m_is_directed))
            << "result[" << idx << "] is not isomorphic to expected";
    }
}

TEST_F(SingleGraphPatternFinderTest, path_4_directed_in_itself_found_graph)
{
    const GraphSpec spec = make_path_4_directed();
    SingleGraphPatternFinder finder = make_finder(spec);
    ColoredGraph graph = spec.to_graph();

    const std::vector<BoostGraph> result = finder.find_pattern(graph, -1000.0);

    ASSERT_FALSE(result.empty());

    for (uint32_t idx = 0U; idx < static_cast<uint32_t>(result.size()); ++idx)
    {
        EXPECT_TRUE(
            boost_graph_isomorphic_to(result[idx], spec.m_edges, spec.m_colors, spec.m_is_directed))
            << "result[" << idx << "] is not isomorphic to expected";
    }
}

TEST_F(SingleGraphPatternFinderTest, path_4_colored_in_itself_found_graph)
{
    const GraphSpec spec = make_path_4_colored();
    SingleGraphPatternFinder finder = make_finder(spec);
    ColoredGraph graph = spec.to_graph();

    const std::vector<BoostGraph> result = finder.find_pattern(graph, -1000.0);

    ASSERT_FALSE(result.empty());

    for (uint32_t idx = 0U; idx < static_cast<uint32_t>(result.size()); ++idx)
    {
        EXPECT_TRUE(
            boost_graph_isomorphic_to(result[idx], spec.m_edges, spec.m_colors, spec.m_is_directed))
            << "result[" << idx << "] is not isomorphic to expected";
    }
}

TEST_F(SingleGraphPatternFinderTest, path_4_colored_directed_in_itself_found_graph)
{
    const GraphSpec spec = make_path_4_directed_colored();
    SingleGraphPatternFinder finder = make_finder(spec);
    ColoredGraph graph = spec.to_graph();

    const std::vector<BoostGraph> result = finder.find_pattern(graph, -1000.0);

    ASSERT_FALSE(result.empty());

    for (uint32_t idx = 0U; idx < static_cast<uint32_t>(result.size()); ++idx)
    {
        EXPECT_TRUE(
            boost_graph_isomorphic_to(result[idx], spec.m_edges, spec.m_colors, spec.m_is_directed))
            << "result[" << idx << "] is not isomorphic to expected";
    }
}

TEST_F(SingleGraphPatternFinderTest, triangle_3_in_itself_found_graph)
{
    const GraphSpec spec = make_triangle_3();
    SingleGraphPatternFinder finder = make_finder(spec);
    ColoredGraph graph = spec.to_graph();

    const std::vector<BoostGraph> result = finder.find_pattern(graph, -1000.0);

    ASSERT_FALSE(result.empty());

    for (uint32_t idx = 0U; idx < static_cast<uint32_t>(result.size()); ++idx)
    {
        EXPECT_TRUE(
            boost_graph_isomorphic_to(result[idx], spec.m_edges, spec.m_colors, spec.m_is_directed))
            << "result[" << idx << "] is not isomorphic to expected";
    }
}

TEST_F(SingleGraphPatternFinderTest, triangle_3_directed_in_itself_found_graph)
{
    const GraphSpec spec = make_triangle_3_directed();
    SingleGraphPatternFinder finder = make_finder(spec);
    ColoredGraph graph = spec.to_graph();

    const std::vector<BoostGraph> result = finder.find_pattern(graph, -1000.0);

    ASSERT_FALSE(result.empty());

    for (uint32_t idx = 0U; idx < static_cast<uint32_t>(result.size()); ++idx)
    {
        EXPECT_TRUE(
            boost_graph_isomorphic_to(result[idx], spec.m_edges, spec.m_colors, spec.m_is_directed))
            << "result[" << idx << "] is not isomorphic to expected";
    }
}

TEST_F(SingleGraphPatternFinderTest, triangle_3_colored_in_itself_found_graph)
{
    const GraphSpec spec = make_triangle_3_colored();
    SingleGraphPatternFinder finder = make_finder(spec);
    ColoredGraph graph = spec.to_graph();

    const std::vector<BoostGraph> result = finder.find_pattern(graph, -1000.0);

    ASSERT_FALSE(result.empty());

    for (uint32_t idx = 0U; idx < static_cast<uint32_t>(result.size()); ++idx)
    {
        EXPECT_TRUE(
            boost_graph_isomorphic_to(result[idx], spec.m_edges, spec.m_colors, spec.m_is_directed))
            << "result[" << idx << "] is not isomorphic to expected";
    }
}

TEST_F(SingleGraphPatternFinderTest, triangle_3_colored_directed_in_itself_found_graph)
{
    const GraphSpec spec = make_triangle_3_colored_directed();
    SingleGraphPatternFinder finder = make_finder(spec);
    ColoredGraph graph = spec.to_graph();

    const std::vector<BoostGraph> result = finder.find_pattern(graph, -1000.0);

    ASSERT_FALSE(result.empty());

    for (uint32_t idx = 0U; idx < static_cast<uint32_t>(result.size()); ++idx)
    {
        EXPECT_TRUE(
            boost_graph_isomorphic_to(result[idx], spec.m_edges, spec.m_colors, spec.m_is_directed))
            << "result[" << idx << "] is not isomorphic to expected";
    }
}

TEST_F(SingleGraphPatternFinderTest, path_4_in_path_4_with_added_vertex_found_graph)
{
    const GraphSpec spec = make_path_4();
    SingleGraphPatternFinder finder =
        make_finder(make_path_4_with_1_added_vertex(), spec.m_is_directed);
    ColoredGraph graph = spec.to_graph();

    const std::vector<BoostGraph> result = finder.find_pattern(graph, -1000.0);

    ASSERT_FALSE(result.empty());

    for (uint32_t idx = 0U; idx < static_cast<uint32_t>(result.size()); ++idx)
    {
        EXPECT_TRUE(
            boost_graph_isomorphic_to(result[idx], spec.m_edges, spec.m_colors, spec.m_is_directed))
            << "result[" << idx << "] is not isomorphic to expected";
    }
}

TEST_F(SingleGraphPatternFinderTest, path_4_in_path_4_with_added_vertex_directed_found_graph)
{
    const GraphSpec spec = make_path_4_directed();
    SingleGraphPatternFinder finder =
        make_finder(make_path_4_with_1_added_vertex_directed(), spec.m_is_directed);
    ColoredGraph graph = spec.to_graph();

    const std::vector<BoostGraph> result = finder.find_pattern(graph, -1000.0);

    ASSERT_FALSE(result.empty());

    for (uint32_t idx = 0U; idx < static_cast<uint32_t>(result.size()); ++idx)
    {
        EXPECT_TRUE(
            boost_graph_isomorphic_to(result[idx], spec.m_edges, spec.m_colors, spec.m_is_directed))
            << "result[" << idx << "] is not isomorphic to expected";
    }
}

TEST_F(SingleGraphPatternFinderTest, star_5_in_itself_found_graph)
{
    const GraphSpec spec = make_star_5();
    SingleGraphPatternFinder finder = make_finder(spec);
    ColoredGraph graph = spec.to_graph();

    const std::vector<BoostGraph> result = finder.find_pattern(graph, -1000.0);

    ASSERT_FALSE(result.empty());

    for (uint32_t idx = 0U; idx < static_cast<uint32_t>(result.size()); ++idx)
    {
        EXPECT_TRUE(
            boost_graph_isomorphic_to(result[idx], spec.m_edges, spec.m_colors, spec.m_is_directed))
            << "result[" << idx << "] is not isomorphic to expected";
    }
}

TEST_F(SingleGraphPatternFinderTest, complex_graph_in_itself_found_graph)
{
    const GraphSpec spec = make_complex_graph();
    SingleGraphPatternFinder finder = make_finder(spec);
    ColoredGraph graph = spec.to_graph();

    const std::vector<BoostGraph> result = finder.find_pattern(graph, -1000.0);

    ASSERT_FALSE(result.empty());

    for (uint32_t idx = 0U; idx < static_cast<uint32_t>(result.size()); ++idx)
    {
        EXPECT_TRUE(
            boost_graph_isomorphic_to(result[idx], spec.m_edges, spec.m_colors, spec.m_is_directed))
            << "result[" << idx << "] is not isomorphic to expected";
    }
}

TEST_F(SingleGraphPatternFinderTest, complex_graph_directed_in_itself_found_graph)
{
    const GraphSpec spec = make_complex_graph_directed();
    SingleGraphPatternFinder finder = make_finder(spec);
    ColoredGraph graph = spec.to_graph();

    const std::vector<BoostGraph> result = finder.find_pattern(graph, -1000.0);

    ASSERT_FALSE(result.empty());

    for (uint32_t idx = 0U; idx < static_cast<uint32_t>(result.size()); ++idx)
    {
        EXPECT_TRUE(
            boost_graph_isomorphic_to(result[idx], spec.m_edges, spec.m_colors, spec.m_is_directed))
            << "result[" << idx << "] is not isomorphic to expected";
    }
}

TEST_F(SingleGraphPatternFinderTest, complex_graph_colored_in_itself_found_graph)
{
    const GraphSpec spec = make_complex_graph_colored();
    SingleGraphPatternFinder finder = make_finder(spec);
    ColoredGraph graph = spec.to_graph();

    const std::vector<BoostGraph> result = finder.find_pattern(graph, -1000.0);

    ASSERT_FALSE(result.empty());

    for (uint32_t idx = 0U; idx < static_cast<uint32_t>(result.size()); ++idx)
    {
        EXPECT_TRUE(
            boost_graph_isomorphic_to(result[idx], spec.m_edges, spec.m_colors, spec.m_is_directed))
            << "result[" << idx << "] is not isomorphic to expected";
    }
}

TEST_F(SingleGraphPatternFinderTest, complex_graph_colored_directed_in_itself_found_graph)
{
    const GraphSpec spec = make_complex_graph_colored_directed();
    SingleGraphPatternFinder finder = make_finder(spec);
    ColoredGraph graph = spec.to_graph();

    const std::vector<BoostGraph> result = finder.find_pattern(graph, -1000.0);

    ASSERT_FALSE(result.empty());

    for (uint32_t idx = 0U; idx < static_cast<uint32_t>(result.size()); ++idx)
    {
        EXPECT_TRUE(
            boost_graph_isomorphic_to(result[idx], spec.m_edges, spec.m_colors, spec.m_is_directed))
            << "result[" << idx << "] is not isomorphic to expected";
    }
}

}  // namespace
