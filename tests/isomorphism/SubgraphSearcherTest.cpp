#include "SubgraphSearcher.h"

#include "ColoredGraph.h"
#include "PriorPolicy.h"

#include <cstdint>
#include <gtest/gtest.h>
#include <sstream>
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
    std::ostringstream output;
    const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, false, false, output};
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
    std::ostringstream output;
    const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, false, false, output};
    const uint64_t matches = searcher.find_all(triangle, edge_sub);
    EXPECT_EQ(matches, 6ULL);
    EXPECT_EQ(count_matches(output.str()), 6ULL);
}

/**
 * @brief Searching for K3 (triangle) inside K3 finds exactly 6 matches (3!
 *        automorphisms of the triangle).
 */
TEST(SubgraphSearcherTest, find_all_triangle_in_triangle)
{
    const ColoredGraph triangle = make_complete_graph(3U);
    std::ostringstream output;
    const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, false, false, output};
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
    std::ostringstream output;
    const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, false, false, output};
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
    std::ostringstream output;
    const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, false, false, output};
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
        std::ostringstream output;
        const SubgraphSearcher searcher{policy, false, false, output};
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
    std::ostringstream output;
    const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, true, false, output};
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

    std::ostringstream dir_out;
    const SubgraphSearcher dir_searcher{PriorPolicy::SUBGRAPH_DEGREE, true, false, dir_out};
    EXPECT_EQ(dir_searcher.find_all(directed_graph, directed_sub), 1ULL);

    std::ostringstream undir_out;
    const SubgraphSearcher undir_searcher{PriorPolicy::SUBGRAPH_DEGREE, false, false, undir_out};
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
    std::ostringstream output;
    const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, false, true, output};
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
    std::ostringstream output;
    const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, false, true, output};
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
    std::ostringstream output;
    const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, false, false, output};
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

    std::ostringstream non_induced_out;
    const SubgraphSearcher non_induced{PriorPolicy::SUBGRAPH_DEGREE, true, false, non_induced_out};
    EXPECT_EQ(non_induced.find_all(dir_path, dir_edge), 2ULL);

    std::ostringstream induced_out;
    const SubgraphSearcher induced{PriorPolicy::SUBGRAPH_DEGREE, true, true, induced_out};
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

    std::ostringstream output;
    const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, true, true, output};
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
    std::ostringstream output;
    const SubgraphSearcher searcher{PriorPolicy::CONSTANT, false, false, output};
    const uint64_t matches = searcher.find_all(k4, path3);
    EXPECT_EQ(matches, 24ULL);
}
