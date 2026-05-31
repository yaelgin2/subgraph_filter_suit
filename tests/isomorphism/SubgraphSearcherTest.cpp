#include "SubgraphSearcher.h"

#include "ColoredGraph.h"
#include "LoggerHandler.h"
#include "MatchOutputWriter.h"
#include "PriorPolicy.h"
#include <queue>
#include <random>
#include <stdexcept>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

using namespace sgf;

// ── Graph-building helpers ────────────────────────────────────────────────────

namespace
{

/**
 * @brief Builds a single-color undirected complete graph K_n.
 * @param vertex_count Number of vertices.
 * @return Uncolored K_n as a ColoredGraph.
 */
ColoredGraph make_complete_graph(const uint32_t vertex_count)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    for (uint32_t source = 0; source < vertex_count; ++source)
    {
        for (uint32_t dest = source + 1; dest < vertex_count; ++dest)
        {
            edges.emplace_back(source, dest);
        }
    }
    const std::vector<uint32_t> colors(vertex_count, 0U);
    return {vertex_count, edges, colors};
}

/**
 * @brief Builds a single-edge undirected graph between vertices 0 and 1.
 * @param color Vertex color for both endpoints.
 * @return Two-vertex one-edge graph.
 */
ColoredGraph make_single_edge(const uint32_t color)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges{{0U, 1U}};
    const std::vector<uint32_t> colors{color, color};
    return {2U, edges, colors};
}

/**
 * @brief Builds an undirected path graph: 0-1-2-...(n-1).
 * @param vertex_count Number of vertices.
 * @return Path graph.
 */
ColoredGraph make_path_graph(const uint32_t vertex_count)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    for (uint32_t vertex = 0; vertex + 1U < vertex_count; ++vertex)
    {
        edges.emplace_back(vertex, vertex + 1U);
    }
    const std::vector<uint32_t> colors(vertex_count, 0U);
    return {vertex_count, edges, colors};
}

std::vector<uint32_t> make_random_colors(
    const uint32_t vertex_count,
    const uint32_t min_color,
    const uint32_t max_color,
    const uint32_t seed = 42U)
{
    if (min_color > max_color)
    {
        throw std::invalid_argument(
            "min_color must be smaller than or equal to max_color");
    }

    std::mt19937 generator(seed);

    std::uniform_int_distribution<uint32_t> color_distribution(
        min_color,
        max_color
    );

    std::vector<uint32_t> colors;
    colors.reserve(vertex_count);

    for (uint32_t vertex = 0U; vertex < vertex_count; ++vertex)
    {
        colors.push_back(color_distribution(generator));
    }

    return colors;
}

/**
 * @brief Creates a new graph by planting several disjoint copies of a pattern
 *        inside an existing host graph.
 *
 * Each planted copy receives its own separate range of vertex IDs, so the
 * copies never overlap with one another or with the original host graph.
 *
 * Example:
 *   host_size = 1000
 *   pattern_size = 8
 *   number_of_copies = 5
 *
 * Vertex ranges:
 *   original host: 0    - 999
 *   copy 0:        1000 - 1007
 *   copy 1:        1008 - 1015
 *   copy 2:        1016 - 1023
 *   copy 3:        1024 - 1031
 *   copy 4:        1032 - 1039
 *
 * @param host_size Number of vertices in the original host graph.
 * @param host_edges Edges of the original host graph.
 * @param host_colors Colors of the original host graph vertices.
 * @param pattern_edges Edges of the small graph that should be planted.
 * @param pattern_colors Colors of the small graph vertices.
 * @param number_of_copies Number of disjoint copies to plant.
 * @param directed Whether the resulting graph should be treated as directed.
 *
 * @return A new ColoredGraph containing the original host graph and all
 *         planted copies.
 */
ColoredGraph plant_graph_copies(
    const uint32_t host_size,
    std::vector<std::pair<uint32_t, uint32_t>> host_edges,
    std::vector<uint32_t> host_colors,
    const std::vector<std::pair<uint32_t, uint32_t>>& pattern_edges,
    const std::vector<uint32_t>& pattern_colors,
    const uint32_t number_of_copies,
    const bool directed)
{
    const uint32_t pattern_size =
        static_cast<uint32_t>(pattern_colors.size());

    for (uint32_t copy = 0U; copy < number_of_copies; ++copy)
    {
        // Each planted copy receives a separate range of vertex IDs.
        //
        // Example:
        // host_size = 1000
        // pattern_size = 8
        //
        // copy 0: vertices 1000 - 1007
        // copy 1: vertices 1008 - 1015
        // copy 2: vertices 1016 - 1023
        const uint32_t offset =
            host_size + copy * pattern_size;

        // Add the vertex colors of the current planted copy.
        host_colors.insert(
            host_colors.end(),
            pattern_colors.begin(),
            pattern_colors.end()
        );

        // Add the edges of the planted pattern.
        // The offset shifts the local vertex IDs of the pattern
        // into the current copy's separate range.
        for (const auto& [source, target] : pattern_edges)
        {
            host_edges.emplace_back(
                offset + source,
                offset + target
            );
        }

        // Connect the planted copy to the original host graph.
        //
        // copy 0 is connected to host vertex 0.
        // copy 1 is connected to host vertex 1.
        // copy 2 is connected to host vertex 2.
        //
        // The first vertex of the planted pattern is offset.
        host_edges.emplace_back(
            copy % host_size,
            offset
        );
    }

    // Total number of vertices after adding all planted copies.
    const uint32_t total_vertices =
        host_size + number_of_copies * pattern_size;

    return {
        total_vertices,
        host_edges,
        host_colors,
        directed
    };
}

/**
 * @brief Builds a connected undirected Erdos-Renyi G(n, p) graph.
 *
 * All vertices have color 0.

 * @param vertex_count Number of vertices.
 * @param edge_probability Probability that each possible edge exists.
 * @param seed Random seed for reproducible tests.
 * @return A undirected ColoredGraph.
 */
ColoredGraph make_gnp_graph(
    const uint32_t vertex_count,
    const double edge_probability,
    const uint32_t seed = 42U)
{
    if (edge_probability < 0.0 || edge_probability > 1.0)
    {
        throw std::invalid_argument(
            "edge_probability must be between 0 and 1");
    }

    std::mt19937 generator(seed);
    std::bernoulli_distribution include_edge(edge_probability);

    std::vector<std::pair<uint32_t, uint32_t>> edges;

    for (uint32_t source = 0U; source < vertex_count; ++source)
    {
        for (uint32_t dest = source + 1U; dest < vertex_count; ++dest)
        {
            if (include_edge(generator))
            {
                edges.emplace_back(source, dest);
            }
        }
    }

    const std::vector<uint32_t> colors(vertex_count, 0U);

    return {vertex_count, edges, colors};
}

/**
 * @brief Builds a directed graph with a single edge 0→1.
 * @return Directed single-edge graph, both vertices colored 0.
 */
ColoredGraph make_directed_single_edge()
{
    std::vector<std::pair<uint32_t, uint32_t>> edges{{0U, 1U}};
    const std::vector<uint32_t> colors{0U, 0U};
    return {2U, edges, colors, true};
}

/**
 * @brief MatchOutputWriter that captures written matches into an in-memory string.
 *
 * Used in tests to inspect match output without touching the filesystem.
 */
class StringMatchOutputWriter : public MatchOutputWriter
{
public:
    StringMatchOutputWriter() = default;

    /**
     * @brief Appends @p match and a newline to the internal capture buffer.
     * @param match Formatted match string.
     */
    void write_match(const std::string& match)
    {
        const std::lock_guard<std::mutex> lock{m_capture_mutex};
        m_captured += match;
        m_captured += "\n";
    }

    /**
     * @brief Returns the full captured output.
     * @return All match lines concatenated.
     */
    [[nodiscard]] std::string captured() const
    {
        const std::lock_guard<std::mutex> lock{m_capture_mutex};
        return m_captured;
    }

private:
    std::string m_captured;
    mutable std::mutex m_capture_mutex;
};

/**
 * @brief Counts newlines in @p text as a proxy for match count.
 * @param text Output string from the searcher.
 * @return Number of matches (newlines).
 */
uint64_t count_matches(const std::string& text)
{
    uint64_t count = 0ULL;
    for (const char ch : text)
    {
        if (ch == '\n')
        {
            ++count;
        }
    }
    return count;
}

/**
 * @brief Creates a null MatchOutputWriter for tests that only check match count.
 * @return unique_ptr to a StringMatchOutputWriter (output ignored).
 */
std::unique_ptr<MatchOutputWriter> make_null_writer()
{
    return std::make_unique<StringMatchOutputWriter>();
}

}  // namespace

// ── find_all — basic match counting ──────────────────────────────────────────

/**
 * @brief Searching for a single vertex (subgraph size 1) in a graph of N
 *        vertices finds exactly N matches when colours agree.
 */
TEST(SubgraphSearcherTest, find_all_single_vertex_subgraph)
{
    const ColoredGraph graph = make_complete_graph(4U);
    const std::vector<uint32_t> single_color{0U};
    std::vector<std::pair<uint32_t, uint32_t>> no_edges;
    ColoredGraph subgraph{1U, no_edges, single_color};
    const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, false, false, make_null_writer(),
                                    LoggerHandler::null()};
    const uint64_t matches = searcher.find_all(graph, subgraph);
    EXPECT_EQ(matches, 4ULL);
}

/**
 * @brief A single-edge subgraph in K3 (triangle) has 6 matches: 3 edges × 2
 *        orderings.
 */
TEST(SubgraphSearcherTest, find_all_edge_in_triangle)
{
    const ColoredGraph triangle = make_complete_graph(3U);
    const ColoredGraph edge_sub = make_single_edge(0U);
    std::unique_ptr<StringMatchOutputWriter> writer = std::make_unique<StringMatchOutputWriter>();
    StringMatchOutputWriter* capture = writer.get();
    const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, false, false, std::move(writer),
                                    LoggerHandler::null()};
    const uint64_t matches = searcher.find_all(triangle, edge_sub);
    EXPECT_EQ(matches, 6ULL);
    EXPECT_EQ(count_matches(capture->captured()), 6ULL);
}

/**
 * @brief Searching for K3 (triangle) inside K3 finds exactly 6 matches (3!
 *        automorphisms of the triangle).
 */
TEST(SubgraphSearcherTest, find_all_triangle_in_triangle)
{
    const ColoredGraph triangle = make_complete_graph(3U);
    const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, false, false, make_null_writer(),
                                    LoggerHandler::null()};
    const uint64_t matches = searcher.find_all(triangle, triangle);
    EXPECT_EQ(matches, 6ULL);
}

/**
 * @brief Searching for K4 (4-clique) inside K3 finds 0 matches.
 */
TEST(SubgraphSearcherTest, find_all_k4_in_k3_is_zero)
{
    const ColoredGraph k3 = make_complete_graph(3U);
    const ColoredGraph k4 = make_complete_graph(4U);
    const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, false, false, make_null_writer(),
                                    LoggerHandler::null()};
    const uint64_t matches = searcher.find_all(k3, k4);
    EXPECT_EQ(matches, 0ULL);
}

/**
 * @brief Color mismatch: a subgraph vertex with color 1 finds no matches in a
 *        graph where all vertices have color 0.
 */
TEST(SubgraphSearcherTest, find_all_color_mismatch_is_zero)
{
    const ColoredGraph graph = make_complete_graph(3U);
    const ColoredGraph mismatch_sub = make_single_edge(1U);
    const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, false, false, make_null_writer(),
                                    LoggerHandler::null()};
    const uint64_t matches = searcher.find_all(graph, mismatch_sub);
    EXPECT_EQ(matches, 0ULL);
}

// ── find_all — policy variants ────────────────────────────────────────────────

/**
 * @brief All non-COMBINED policies should yield the same match count for the
 *        triangle-in-triangle case.
 */
TEST(SubgraphSearcherTest, find_all_all_policies_same_count)
{
    const ColoredGraph triangle = make_complete_graph(3U);
    constexpr uint64_t EXPECTED = 6ULL;
    const std::vector<PriorPolicy> policies{
        PriorPolicy::SUBGRAPH_DEGREE_SQUARED, PriorPolicy::GRAPH_DEGREE_SQUARED,
        PriorPolicy::CONSTANT, PriorPolicy::RANDOM, PriorPolicy::SUBGRAPH_DEGREE};
    for (const PriorPolicy policy : policies)
    {
        const SubgraphSearcher searcher{policy, false, false, make_null_writer(),
                                        LoggerHandler::null()};
        EXPECT_EQ(searcher.find_all(triangle, triangle), EXPECTED)
            << "policy " << static_cast<uint32_t>(policy);
    }
}

// ── find_all — directed ───────────────────────────────────────────────────────

/**
 * @brief A directed single-edge subgraph (0→1) in a directed graph with only
 *        one edge (0→1) finds exactly 1 match.
 */
TEST(SubgraphSearcherTest, find_all_directed_single_edge_one_match)
{
    const ColoredGraph directed_graph = make_directed_single_edge();
    const ColoredGraph directed_sub = make_directed_single_edge();
    const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, true, false, make_null_writer(),
                                    LoggerHandler::null()};
    const uint64_t matches = searcher.find_all(directed_graph, directed_sub);
    EXPECT_EQ(matches, 1ULL);
}

/**
 * @brief Direction matters: a directed single-edge graph (0→1) has exactly 1
 *        isomorphism with a directed single-edge subgraph (0→1). An undirected
 *        single-edge graph with the same topology would give 2 (both orderings).
 */
TEST(SubgraphSearcherTest, find_all_directed_one_match_vs_undirected_two)
{
    const ColoredGraph directed_graph = make_directed_single_edge();
    const ColoredGraph undirected_graph = make_single_edge(0U);
    const ColoredGraph directed_sub = make_directed_single_edge();
    const ColoredGraph undirected_sub = make_single_edge(0U);

    const SubgraphSearcher dir_searcher{PriorPolicy::SUBGRAPH_DEGREE, true, false,
                                        make_null_writer(), LoggerHandler::null()};
    EXPECT_EQ(dir_searcher.find_all(directed_graph, directed_sub), 1ULL);

    const SubgraphSearcher undir_searcher{PriorPolicy::SUBGRAPH_DEGREE, false, false,
                                          make_null_writer(), LoggerHandler::null()};
    EXPECT_EQ(undir_searcher.find_all(undirected_graph, undirected_sub), 2ULL);
}

// ── find_all — induced ────────────────────────────────────────────────────────

/**
 * @brief An induced single-edge subgraph in K3 finds 6 matches: there are no
 *        "extra" edges to block any assignment since the subgraph uses only the
 *        immediate endpoints.
 */
TEST(SubgraphSearcherTest, find_all_induced_edge_in_triangle)
{
    const ColoredGraph triangle = make_complete_graph(3U);
    const ColoredGraph edge_sub = make_single_edge(0U);
    const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, false, true, make_null_writer(),
                                    LoggerHandler::null()};
    const uint64_t matches = searcher.find_all(triangle, edge_sub);
    // Induced: every graph-neighbor of v_g that is already matched must be a subgraph-neighbor.
    // The two matched vertices have each other as a common neighbor that's already matched,
    // and that edge exists in both graphs, so all 6 still succeed.
    EXPECT_EQ(matches, 6ULL);
}

/**
 * @brief Induced triangle-in-triangle finds 6 matches (same as non-induced
 *        since the subgraph is the full K3).
 */
TEST(SubgraphSearcherTest, find_all_induced_triangle_in_triangle)
{
    const ColoredGraph triangle = make_complete_graph(3U);
    const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, false, true, make_null_writer(),
                                    LoggerHandler::null()};
    const uint64_t matches = searcher.find_all(triangle, triangle);
    EXPECT_EQ(matches, 6ULL);
}

// ── find_all — empty graph ────────────────────────────────────────────────────

/**
 * @brief Searching in an empty host graph (0 vertices) returns 0 matches.
 */
TEST(SubgraphSearcherTest, find_all_empty_host_graph)
{
    std::vector<std::pair<uint32_t, uint32_t>> no_edges;
    const ColoredGraph empty_graph{0U, no_edges, {}};
    const ColoredGraph edge_sub = make_single_edge(0U);
    const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, false, false, make_null_writer(),
                                    LoggerHandler::null()};
    EXPECT_EQ(searcher.find_all(empty_graph, edge_sub), 0ULL);
}

/**
 * @brief Directed induced: a directed edge subgraph (0→1) inside a two-edge
 *        directed graph (0→1, 1→2) matches only the pair (0→1), not (1→2)
 *        because vertex 0 has no in-neighbor while 1 does — unless colors match.
 *        With all vertices color 0: both (0→1) and (1→2) qualify for the
 *        non-induced case, giving 2 matches. Induced adds no extra constraint
 *        here since each endpoint has at most one already-matched neighbor.
 */
TEST(SubgraphSearcherTest, find_all_directed_induced_path_matches)
{
    // Build directed path 0→1→2
    std::vector<std::pair<uint32_t, uint32_t>> edges{{0U, 1U}, {1U, 2U}};
    const std::vector<uint32_t> colors{0U, 0U, 0U};
    const ColoredGraph dir_path{3U, edges, colors, true};
    const ColoredGraph dir_edge = make_directed_single_edge();

    const SubgraphSearcher non_induced{PriorPolicy::SUBGRAPH_DEGREE, true, false,
                                       make_null_writer(), LoggerHandler::null()};
    EXPECT_EQ(non_induced.find_all(dir_path, dir_edge), 2ULL);

    const SubgraphSearcher induced{PriorPolicy::SUBGRAPH_DEGREE, true, true, make_null_writer(),
                                   LoggerHandler::null()};
    EXPECT_EQ(induced.find_all(dir_path, dir_edge), 2ULL);
}

/**
 * @brief Directed induced: subgraph is a two-cycle (0→1, 1→0). The host graph
 *        is a directed path (0→1, 1→2). The two-cycle requires both a→b and
 *        b→a edges; no such pair exists in the path, so matches = 0.
 */
TEST(SubgraphSearcherTest, find_all_directed_induced_two_cycle_no_match_in_path)
{
    std::vector<std::pair<uint32_t, uint32_t>> path_edges{{0U, 1U}, {1U, 2U}};
    const std::vector<uint32_t> path_colors{0U, 0U, 0U};
    const ColoredGraph dir_path{3U, path_edges, path_colors, true};

    std::vector<std::pair<uint32_t, uint32_t>> cycle_edges{{0U, 1U}, {1U, 0U}};
    const std::vector<uint32_t> cycle_colors{0U, 0U};
    const ColoredGraph two_cycle{2U, cycle_edges, cycle_colors, true};

    const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, true, true, make_null_writer(),
                                    LoggerHandler::null()};
    EXPECT_EQ(searcher.find_all(dir_path, two_cycle), 0ULL);
}

/**
 * @brief A path-3 subgraph (0-1-2) embedded in K4 has the correct match count.
 *
 * K4 has 4 vertices each of degree 3. A path of length 2 has 3 vertices.
 * There are P(4,3) = 24 ordered triples of distinct vertices, but only those
 * where the first and last are NOT directly connected form a valid path.
 * In K4 every pair is connected, so valid ordered triples (a-b-c with a-b and
 * b-c edges, a≠c) number: 4 choices for middle * 3*2 for ends = 24. But
 * a-c must NOT be an edge for induced, yet for non-induced all 24 are valid.
 */
TEST(SubgraphSearcherTest, find_all_path3_in_k4_non_induced)
{
    const ColoredGraph k4 = make_complete_graph(4U);
    const ColoredGraph path3 = make_path_graph(3U);
    const SubgraphSearcher searcher{PriorPolicy::CONSTANT, false, false, make_null_writer(),
                                    LoggerHandler::null()};
    const uint64_t matches = searcher.find_all(k4, path3);
    EXPECT_EQ(matches, 24ULL);
}

TEST(SubgraphSearcherTest, find_all_planted_undirected_edge_in_large_complete_graph)
{
    ColoredGraph graph = make_complete_graph(1000U);

    // Plant one edge with endpoint colors 1—1.
    graph.set_vertex_color(0U, 1U);
    graph.set_vertex_color(1U, 1U);

    const ColoredGraph subgraph = make_single_edge(1U);

    const SubgraphSearcher searcher{
        PriorPolicy::SUBGRAPH_DEGREE,
        false,  // undirected
        false,  // non-induced
        make_null_writer(),
        LoggerHandler::null()
    };

    const uint64_t matches = searcher.find_all(graph, subgraph);

    EXPECT_EQ(matches, 2ULL);
}

TEST(SubgraphSearcherTest, find_all_planted_directed_edge_in_large_complete_graph)
{
    ColoredGraph graph = make_complete_graph(1000U);

    // Plant one edge with endpoint colors 1—1.
    graph.set_vertex_color(0U, 1U);
    graph.set_vertex_color(1U, 1U);

    const ColoredGraph subgraph = make_single_edge(1U);

    const SubgraphSearcher searcher{
        PriorPolicy::SUBGRAPH_DEGREE,
        true,  // directed
        false,  // non-induced
        make_null_writer(),
        LoggerHandler::null()
    };

    const uint64_t matches = searcher.find_all(graph, subgraph);

    EXPECT_EQ(matches, 1ULL);
}

TEST(SubgraphSearcherTest, disconnected_gnp_graph_returns_zero)
{
    const ColoredGraph graph = make_gnp_graph(
        1000U,
        0.0,
        42U
    );

    const ColoredGraph subgraph = make_single_edge(0U);

    const SubgraphSearcher searcher{
        PriorPolicy::SUBGRAPH_DEGREE,
        false,  // undirected
        false,  // non-induced
        make_null_writer(),
        LoggerHandler::null()
    };

    const uint64_t matches = searcher.find_all(graph, subgraph);

    EXPECT_EQ(matches, 0ULL);
}

TEST(SubgraphSearcherTest, disconnected_graph_with_one_undirected_edge)
{
    constexpr uint32_t vertex_count = 1000U;

    std::vector<std::pair<uint32_t, uint32_t>> edges{
        {0U, 1U}
    };

    const std::vector<uint32_t> colors(vertex_count, 0U);

    const ColoredGraph graph{
        vertex_count,
        edges,
        colors
    };

    const ColoredGraph subgraph = make_single_edge(0U);

    const SubgraphSearcher searcher{
        PriorPolicy::SUBGRAPH_DEGREE,
        false,  // undirected
        false,  // non-induced
        make_null_writer(),
        LoggerHandler::null()
    };

    const uint64_t matches = searcher.find_all(graph, subgraph);

    EXPECT_EQ(matches, 2ULL);
}

TEST(SubgraphSearcherTest, disconnected_graph_with_one_directed_edge)
{
    constexpr uint32_t vertex_count = 1000U;

    std::vector<std::pair<uint32_t, uint32_t>> edges{
        {0U, 1U}
    };

    const std::vector<uint32_t> colors(vertex_count, 0U);

    const ColoredGraph graph{
        vertex_count,
        edges,
        colors,
        true  // directed graph
    };

    std::vector<std::pair<uint32_t, uint32_t>> subgraph_edges{
        {0U, 1U}
    };

    const std::vector<uint32_t> subgraph_colors{
        0U,
        0U
    };

    const ColoredGraph subgraph{
        2U,
        subgraph_edges,
        subgraph_colors,
        true  // directed subgraph
    };

    const SubgraphSearcher searcher{
        PriorPolicy::SUBGRAPH_DEGREE,
        true,   // directed search
        false,  // non-induced
        make_null_writer(),
        LoggerHandler::null()
    };

    const uint64_t matches = searcher.find_all(graph, subgraph);

    EXPECT_EQ(matches, 1ULL);
}



TEST(
    SubgraphSearcherTest,
    five_planted_colored_patterns_in_large_undirected_gnp_graph)
{
    constexpr uint32_t host_size = 1000U;
    constexpr uint32_t pattern_size = 6U;
    constexpr uint32_t number_of_copies = 5U;

    std::mt19937 generator(42U);

    // Create the large G(n,p) background graph.
    std::bernoulli_distribution include_host_edge(0.01);

    std::vector<std::pair<uint32_t, uint32_t>> host_edges;

    for (uint32_t source = 0U; source < host_size; ++source)
    {
        for (uint32_t target = source + 1U;
             target < host_size;
             ++target)
        {
            if (include_host_edge(generator))
            {
                host_edges.emplace_back(source, target);
            }
        }
    }

    // All background vertices have color 0.
    std::vector<uint32_t> host_colors(host_size, 0U);

    // Create a small colored graph.
    // p = 1 ensures that it is connected and prevents combinations
    // between separate planted copies.
    std::vector<std::pair<uint32_t, uint32_t>> pattern_edges;

    for (uint32_t source = 0U; source < pattern_size; ++source)
    {
        for (uint32_t target = source + 1U;
             target < pattern_size;
             ++target)
        {
            pattern_edges.emplace_back(source, target);
        }
    }

    const std::vector<uint32_t> pattern_colors =
        make_random_colors(
            pattern_size,
            1000U,
            9999U,
            123U
        );

    const ColoredGraph graph =
        plant_graph_copies(
            host_size,
            host_edges,
            host_colors,
            pattern_edges,
            pattern_colors,
            number_of_copies,
            false  // undirected
        );

    ColoredGraph subgraph{
        pattern_size,
        pattern_edges,
        pattern_colors,
        false  // undirected
    };

    const SubgraphSearcher searcher{
        PriorPolicy::SUBGRAPH_DEGREE,
        false,  // undirected
        false,  // non-induced
        make_null_writer(),
        LoggerHandler::null()
    };

    EXPECT_EQ(searcher.find_all(graph, subgraph), 5ULL);
}


TEST(
    SubgraphSearcherTest,
    five_planted_colored_patterns_induced_in_large_directed_gnp_graph)
{
    constexpr uint32_t host_size = 1000U;
    constexpr uint32_t pattern_size = 6U;
    constexpr uint32_t number_of_copies = 5U;

    std::mt19937 generator(42U);

    // Create the large directed G(n,p) background graph.
    std::bernoulli_distribution include_host_edge(0.01);

    std::vector<std::pair<uint32_t, uint32_t>> host_edges;

    for (uint32_t source = 0U; source < host_size; ++source)
    {
        for (uint32_t target = source + 1U;
             target < host_size;
             ++target)
        {
            if (include_host_edge(generator))
            {
                host_edges.emplace_back(source, target);
            }
        }
    }

    // All background vertices have color 0.
    std::vector<uint32_t> host_colors(host_size, 0U);

    // Create a small directed colored graph.
    std::vector<std::pair<uint32_t, uint32_t>> pattern_edges;

    for (uint32_t source = 0U; source < pattern_size; ++source)
    {
        for (uint32_t target = source + 1U;
             target < pattern_size;
             ++target)
        {
            pattern_edges.emplace_back(source, target);
        }
    }

    const std::vector<uint32_t> pattern_colors =
        make_random_colors(
            pattern_size,
            1000U,
            9999U,
            123U
        );

    const ColoredGraph graph =
        plant_graph_copies(
            host_size,
            host_edges,
            host_colors,
            pattern_edges,
            pattern_colors,
            number_of_copies,
            true  // directed
        );

    ColoredGraph subgraph{
        pattern_size,
        pattern_edges,
        pattern_colors,
        true  // directed
    };

    const SubgraphSearcher searcher{
        PriorPolicy::SUBGRAPH_DEGREE,
        true,   // directed
        true,  // induced
        make_null_writer(),
        LoggerHandler::null()
    };

    EXPECT_EQ(searcher.find_all(graph, subgraph), 5ULL);
}


TEST(
    SubgraphSearcherTest,
    disconnected_colored_vertices_subgraph_found_in_connected_clique)
{
    // G = K6, therefore the host graph is connected.
    ColoredGraph graph = make_complete_graph(6U);

    // Color distribution in G:
    // color 1: three vertices
    // color 2: two vertices
    // color 3: one vertex
    graph.set_vertex_color(0U, 1U);
    graph.set_vertex_color(1U, 1U);
    graph.set_vertex_color(2U, 1U);
    graph.set_vertex_color(3U, 2U);
    graph.set_vertex_color(4U, 2U);
    graph.set_vertex_color(5U, 3U);

    // S contains three isolated vertices with no edges.
    // Required colors: two vertices of color 1 and one vertex of color 2.
    std::vector<std::pair<uint32_t, uint32_t>> no_edges;

    const std::vector<uint32_t> subgraph_colors{
        1U,
        1U,
        2U
    };

    const ColoredGraph subgraph{
        3U,
        no_edges,
        subgraph_colors
    };

    const SubgraphSearcher searcher{
        PriorPolicy::SUBGRAPH_DEGREE,
        true,  // undirected
        true,  // non-induced
        make_null_writer(),
        LoggerHandler::null()
    };

    const uint64_t matches = searcher.find_all(graph, subgraph);

    // Choose two distinct color-1 vertices in order: 3 * 2.
    // Choose one color-2 vertex: 2.
    // Total: 3 * 2 * 2 = 12.
    EXPECT_EQ(matches, 0ULL);
}