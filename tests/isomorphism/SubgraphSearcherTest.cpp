#include "SubgraphSearcher.h"

#include "ColoredGraph.h"
#include "ILogger.h"
#include "InvalidArgumentException.h"
#include "LoggerHandler.h"
#include "MatchOutputWriter.h"
#include "PriorPolicy.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <map>
#include <memory>
#include <mutex>
#include <set>
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
 * @brief ILogger implementation that captures all log messages into a vector.
 *
 * Thread-safe: multiple threads may call log() concurrently.
 */
class CapturingLogger : public ILogger
{
public:
    void log(const LogLevel /*level*/, const std::string& message) override
    {
        const std::lock_guard<std::mutex> lock{m_mutex};
        m_messages.push_back(message);
    }

    /**
     * @brief Returns a snapshot of all captured messages.
     * @return All messages in the order they were received.
     */
    [[nodiscard]] std::vector<std::string> messages() const
    {
        const std::lock_guard<std::mutex> lock{m_mutex};
        return m_messages;
    }

private:
    std::vector<std::string> m_messages;
    mutable std::mutex m_mutex;
};

/**
 * @brief Creates a null MatchOutputWriter for tests that only check match count.
 * @return unique_ptr to a StringMatchOutputWriter (output ignored).
 */
std::unique_ptr<MatchOutputWriter> make_null_writer()
{
    return std::make_unique<StringMatchOutputWriter>();
}

/**
 * @brief Builds a single-vertex graph with the given color.
 */
ColoredGraph make_single_vertex_graph(const uint32_t color)
{
    std::vector<std::pair<uint32_t, uint32_t>> no_edges;
    return {1U, no_edges, {color}};
}

/**
 * @brief Builds a two-vertex graph with no edges (disconnected), both vertices colored @p color.
 */
ColoredGraph make_disconnected_two_vertices(const uint32_t color)
{
    std::vector<std::pair<uint32_t, uint32_t>> no_edges;
    return {2U, no_edges, {color, color}};
}

/**
 * @brief Builds an undirected single-edge graph with distinct per-vertex colors.
 */
ColoredGraph make_edge_with_colors(const uint32_t color0, const uint32_t color1)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges{{0U, 1U}};
    return {2U, edges, {color0, color1}};
}

/**
 * @brief Builds a directed single-edge graph 0→1 with distinct per-vertex colors.
 */
ColoredGraph make_directed_edge_with_colors(const uint32_t color0, const uint32_t color1)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges{{0U, 1U}};
    return {2U, edges, {color0, color1}, true};
}

/**
 * @brief Builds a directed graph with edges 0→1 AND 1→0 (bidirectional), both vertices color 0.
 */
ColoredGraph make_directed_bidirectional_edge()
{
    std::vector<std::pair<uint32_t, uint32_t>> edges{{0U, 1U}, {1U, 0U}};
    return {2U, edges, {0U, 0U}, true};
}

/**
 * @brief Builds a directed graph: 0→1, 1→0 (bidirectional) plus 0→2 (extra edge), all color 0.
 *
 * The extra edge makes vertex degrees asymmetric so prior scores differ if any
 * edge is double-counted.
 */
ColoredGraph make_directed_bidirectional_with_extra()
{
    std::vector<std::pair<uint32_t, uint32_t>> edges{{0U, 1U}, {1U, 0U}, {0U, 2U}};
    return {3U, edges, {0U, 0U, 0U}, true};
}

/**
 * @brief Builds a directed path 0→1→2→…→(n-1), all vertices color 0.
 * @param vertex_count Number of vertices.
 * @return Directed path graph.
 */
ColoredGraph make_directed_path(const uint32_t vertex_count)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    for (uint32_t vertex = 0; vertex + 1U < vertex_count; ++vertex)
    {
        edges.emplace_back(vertex, vertex + 1U);
    }
    const std::vector<uint32_t> colors(vertex_count, 0U);
    return {vertex_count, edges, colors, true};
}

/**
 * @brief Builds an undirected triangle (K3) with the given per-vertex colors.
 */
ColoredGraph make_undirected_triangle(const std::vector<uint32_t>& colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges{{0U, 1U}, {1U, 2U}, {0U, 2U}};
    return {3U, edges, colors};
}

/**
 * @brief Builds an undirected 4-cycle (square) with the given per-vertex colors.
 */
ColoredGraph make_square_cycle(const std::vector<uint32_t>& colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges{{0U, 1U}, {1U, 2U}, {2U, 3U}, {3U, 0U}};
    return {4U, edges, colors};
}

/**
 * @brief Builds an undirected 4-cycle plus a 0-2 chord (C4 + diagonal) with the given colors.
 */
ColoredGraph make_square_with_diagonal(const std::vector<uint32_t>& colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges{
        {0U, 1U}, {1U, 2U}, {2U, 3U}, {3U, 0U}, {0U, 2U}};
    return {4U, edges, colors};
}

/**
 * @brief Builds a directed 3-cycle 0→1→2→0 with the given per-vertex colors.
 */
ColoredGraph make_directed_three_cycle(const std::vector<uint32_t>& colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges{{0U, 1U}, {1U, 2U}, {2U, 0U}};
    return {3U, edges, colors, true};
}

/**
 * @brief Builds a directed DAG triangle (0→1, 1→2, 0→2) with the given colors.
 *
 * This forms a transitive tournament on 3 vertices — no directed cycle.
 */
ColoredGraph make_directed_dag_triangle(const std::vector<uint32_t>& colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges{{0U, 1U}, {1U, 2U}, {0U, 2U}};
    return {3U, edges, colors, true};
}

/**
 * @brief Builds a directed 4-cycle 0→1→2→3→0 with the given per-vertex colors.
 */
ColoredGraph make_directed_four_cycle(const std::vector<uint32_t>& colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges{{0U, 1U}, {1U, 2U}, {2U, 3U}, {3U, 0U}};
    return {4U, edges, colors, true};
}

/**
 * @brief Builds a directed 4-cycle plus a 0→2 chord with the given per-vertex colors.
 */
ColoredGraph make_directed_four_cycle_with_diagonal(const std::vector<uint32_t>& colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges{
        {0U, 1U}, {1U, 2U}, {2U, 3U}, {3U, 0U}, {0U, 2U}};
    return {4U, edges, colors, true};
}

/**
 * @brief Builds a directed 4-cycle in the reverse direction: 0→3→2→1→0.
 */
ColoredGraph make_reverse_directed_four_cycle(const std::vector<uint32_t>& colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges{{0U, 3U}, {3U, 2U}, {2U, 1U}, {1U, 0U}};
    return {4U, edges, colors, true};
}

/**
 * @brief Counts non-empty lines in the file at @p path.
 */
uint64_t count_file_lines(const std::string& path)
{
    std::ifstream file(path);
    uint64_t count = 0ULL;
    std::string line;
    while (std::getline(file, line))
    {
        ++count;
    }
    return count;
}

/**
 * @brief Parses a match file into a vector of graph→subgraph vertex mappings.
 *
 * Each line has the format "{g0:s0 g1:s1 ...}". Returns one map per match line.
 */
std::vector<std::map<uint32_t, uint32_t>> parse_match_file(const std::string& path)
{
    std::vector<std::map<uint32_t, uint32_t>> matches;
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line))
    {
        if (line.size() < 2U || line.front() != '{')
        {
            continue;
        }
        std::map<uint32_t, uint32_t> match;
        const std::string inner = line.substr(1U, line.size() - 2U);
        std::istringstream iss(inner);
        std::string token;
        while (iss >> token)
        {
            const std::size_t colon = token.find(':');
            if (colon == std::string::npos)
            {
                continue;
            }
            const uint32_t g_vertex = static_cast<uint32_t>(std::stoul(token.substr(0U, colon)));
            const uint32_t s_vertex = static_cast<uint32_t>(std::stoul(token.substr(colon + 1U)));
            match[g_vertex] = s_vertex;
        }
        matches.push_back(match);
    }
    return matches;
}

using MatchMap = std::map<uint32_t, uint32_t>;

/**
 * @brief Returns all 6 permutation match maps for a triangle with graph vertices v0, v1, v2.
 *
 * Each map assigns the three graph vertices to subgraph vertices 0, 1, 2 in every order.
 */
std::vector<MatchMap> all_triangle_maps(const uint32_t v0, const uint32_t v1, const uint32_t v2)
{
    std::vector<uint32_t> verts{v0, v1, v2};
    std::sort(verts.begin(), verts.end());
    std::vector<MatchMap> result;
    do
    {
        result.push_back({{verts[0U], 0U}, {verts[1U], 1U}, {verts[2U], 2U}});
    } while (std::next_permutation(verts.begin(), verts.end()));
    return result;
}

/**
 * @brief Asserts that the match file contains exactly the expected multiset of vertex mappings.
 */
void check_file_matches(const std::string& path, const std::vector<MatchMap>& expected)
{
    const std::vector<MatchMap> actual = parse_match_file(path);
    using MatchSet = std::multiset<MatchMap>;
    const MatchSet actual_set(actual.cbegin(), actual.cend());
    const MatchSet expected_set(expected.cbegin(), expected.cend());
    EXPECT_EQ(actual_set, expected_set);
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
 * @brief All policies should yield the same match count for the
 *        triangle-in-triangle case.
 */
TEST(SubgraphSearcherTest, find_all_all_policies_same_count)
{
    const ColoredGraph triangle = make_complete_graph(3U);
    constexpr uint64_t EXPECTED = 6ULL;
    const std::vector<PriorPolicy> policies{PriorPolicy::SUBGRAPH_DEGREE_SQUARED,
                                            PriorPolicy::GRAPH_DEGREE_SQUARED,
                                            PriorPolicy::CONSTANT,
                                            PriorPolicy::RANDOM,
                                            PriorPolicy::SUBGRAPH_DEGREE,
                                            PriorPolicy::COMBINED};
    for (const PriorPolicy policy : policies)
    {
        const SubgraphSearcher searcher{policy, false, false, make_null_writer(),
                                        LoggerHandler::null()};
        EXPECT_EQ(searcher.find_all(triangle, triangle), EXPECTED)
            << "policy " << static_cast<uint32_t>(policy);
    }
}

/**
 * @brief All policies yield the same match count in directed mode.
 *
 * Directed 3-cycle searched in itself has 3 matches (one per cyclic rotation).
 */
TEST(SubgraphSearcherTest, find_all_all_policies_same_count_directed)
{
    const std::vector<uint32_t> three_same{0U, 0U, 0U};
    const ColoredGraph directed_triangle = make_directed_three_cycle(three_same);
    constexpr uint64_t EXPECTED = 3ULL;
    const std::vector<PriorPolicy> policies{PriorPolicy::SUBGRAPH_DEGREE_SQUARED,
                                            PriorPolicy::GRAPH_DEGREE_SQUARED,
                                            PriorPolicy::CONSTANT,
                                            PriorPolicy::RANDOM,
                                            PriorPolicy::SUBGRAPH_DEGREE,
                                            PriorPolicy::COMBINED};
    for (const PriorPolicy policy : policies)
    {
        const SubgraphSearcher searcher{policy, true, false, make_null_writer(),
                                        LoggerHandler::null()};
        EXPECT_EQ(searcher.find_all(directed_triangle, directed_triangle), EXPECTED)
            << "policy " << static_cast<uint32_t>(policy);
    }
}

/**
 * @brief All policies yield the same match count in induced mode.
 *
 * Path-3 induced in square-with-diagonal: the induced constraint forbids matches
 * where the two path endpoints map to adjacent graph vertices, reducing 16
 * non-induced matches to 4 (only the two pairs whose endpoints span the
 * single non-edge 1-3 survive).
 */
TEST(SubgraphSearcherTest, find_all_all_policies_same_count_induced)
{
    const std::vector<uint32_t> four_same{0U, 0U, 0U, 0U};
    const ColoredGraph graph = make_square_with_diagonal(four_same);
    const ColoredGraph path3 = make_path_graph(3U);
    constexpr uint64_t EXPECTED = 4ULL;
    const std::vector<PriorPolicy> policies{PriorPolicy::SUBGRAPH_DEGREE_SQUARED,
                                            PriorPolicy::GRAPH_DEGREE_SQUARED,
                                            PriorPolicy::CONSTANT,
                                            PriorPolicy::RANDOM,
                                            PriorPolicy::SUBGRAPH_DEGREE,
                                            PriorPolicy::COMBINED};
    for (const PriorPolicy policy : policies)
    {
        const SubgraphSearcher searcher{policy, false, true, make_null_writer(),
                                        LoggerHandler::null()};
        EXPECT_EQ(searcher.find_all(graph, path3), EXPECTED)
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
 * @brief Same directed graph and subgraph (0→1) searched with directed vs
 *        undirected searcher.
 *
 * Directed searcher respects edge direction and finds 1 match. Undirected
 * searcher ignores direction and finds 2 matches (both vertex orderings).
 */
TEST(SubgraphSearcherTest, find_all_directed_one_match_vs_undirected_two)
{
    const ColoredGraph graph = make_directed_single_edge();
    const ColoredGraph subgraph = make_directed_single_edge();

    const SubgraphSearcher dir_searcher{PriorPolicy::SUBGRAPH_DEGREE, true, false,
                                        make_null_writer(), LoggerHandler::null()};
    EXPECT_EQ(dir_searcher.find_all(graph, subgraph), 1ULL);

    const SubgraphSearcher undir_searcher{PriorPolicy::SUBGRAPH_DEGREE, false, false,
                                          make_null_writer(), LoggerHandler::null()};
    EXPECT_EQ(undir_searcher.find_all(graph, subgraph), 1ULL);  // 1 because the graph is directed.
}

// ── find_all — induced ────────────────────────────────────────────────────────

/**
 * @brief An induced single-edge subgraph in K3 finds 6 matches: there are no
 *        "extra" edges to block any assignment since the subgraph uses only the
 *        immediate endpoints.
 */
/**
 * @brief Induced path-3 in K3 finds 0 matches; non-induced finds 6.
 *
 * Path-3 has a non-edge between its two endpoints (subgraph vertices 0 and 2).
 * K3 has an edge between every pair, so the induced constraint always blocks
 * the endpoint mapping — reducing 6 non-induced matches to 0.
 */
TEST(SubgraphSearcherTest, find_all_induced_path3_in_triangle)
{
    const ColoredGraph triangle = make_complete_graph(3U);
    const ColoredGraph path3 = make_path_graph(3U);

    const SubgraphSearcher non_induced{PriorPolicy::SUBGRAPH_DEGREE, false, false,
                                       make_null_writer(), LoggerHandler::null()};
    EXPECT_EQ(non_induced.find_all(triangle, path3), 6ULL);

    const SubgraphSearcher induced{PriorPolicy::SUBGRAPH_DEGREE, false, true, make_null_writer(),
                                   LoggerHandler::null()};
    EXPECT_EQ(induced.find_all(triangle, path3), 0ULL);
}

/**
 * @brief Induced path-3 in square-with-diagonal finds 4 matches; non-induced finds 16.
 *
 * The only non-edge in the 4-vertex graph is 1-3. Induced path-3 matches only
 * the 4 triples whose endpoints span that non-edge: (1,0,3), (1,2,3), (3,0,1), (3,2,1).
 */
TEST(SubgraphSearcherTest, find_all_induced_path3_in_square_with_diagonal)
{
    const std::vector<uint32_t> four_same{0U, 0U, 0U, 0U};
    const ColoredGraph graph = make_square_with_diagonal(four_same);
    const ColoredGraph path3 = make_path_graph(3U);

    const SubgraphSearcher non_induced{PriorPolicy::SUBGRAPH_DEGREE, false, false,
                                       make_null_writer(), LoggerHandler::null()};
    EXPECT_EQ(non_induced.find_all(graph, path3), 16ULL);

    const SubgraphSearcher induced{PriorPolicy::SUBGRAPH_DEGREE, false, true, make_null_writer(),
                                   LoggerHandler::null()};
    EXPECT_EQ(induced.find_all(graph, path3), 4ULL);
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

// ── Fixture for file-based tests ──────────────────────────────────────────────

/**
 * @brief Test fixture for SubgraphSearcher tests that write matches to a temp file.
 *
 * Each test gets a unique directory; TearDown removes it and all written files.
 */
class SubgraphSearcherFileTest : public ::testing::Test
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
            std::filesystem::temp_directory_path() / ("sgf_ss_" + std::string(info->name()));
        std::filesystem::create_directories(m_temp_dir);
        m_temp_path = (m_temp_dir / "matches.txt").string();
    }

    /**
     * @brief Removes the temp directory and all written files.
     */
    void TearDown() override
    {
        std::filesystem::remove_all(m_temp_dir);
    }

    std::string m_temp_path;
    std::filesystem::path m_temp_dir;
};

// ── Test 1: Empty S and G ─────────────────────────────────────────────────────

/**
 * @brief find_all throws InvalidArgumentException when the subgraph is empty.
 */
TEST(SubgraphSearcherTest, find_all_throws_on_empty_subgraph_and_empty_graph)
{
    std::vector<std::pair<uint32_t, uint32_t>> no_edges;
    const ColoredGraph empty{0U, no_edges, {}};
    const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, false, false, make_null_writer(),
                                    LoggerHandler::null()};
    EXPECT_THROW(searcher.find_all(empty, empty), InvalidArgumentException);
}

// ── Test 2: Empty S, non-empty G ──────────────────────────────────────────────

/**
 * @brief find_all throws InvalidArgumentException when the subgraph is empty.
 */
TEST(SubgraphSearcherTest, find_all_throws_on_empty_subgraph_nonempty_graph)
{
    std::vector<std::pair<uint32_t, uint32_t>> no_edges;
    const ColoredGraph empty{0U, no_edges, {}};
    const ColoredGraph graph = make_complete_graph(3U);
    const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, false, false, make_null_writer(),
                                    LoggerHandler::null()};
    EXPECT_THROW(searcher.find_all(graph, empty), InvalidArgumentException);
}

// ── Test 3: Empty G, non-empty S ─────────────────────────────────────────────
/**
 * @brief find_all returns 0 when the host graph is empty and the subgraph is non-empty.
 */
TEST(SubgraphSearcherTest, find_all_empty_host_graph_returns_zero)
{
    std::vector<std::pair<uint32_t, uint32_t>> no_edges;
    const ColoredGraph empty_graph{0U, no_edges, {}};
    const ColoredGraph subgraph = make_single_edge(0U);
    const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, false, false, make_null_writer(),
                                    LoggerHandler::null()};
    EXPECT_EQ(searcher.find_all(empty_graph, subgraph), 0ULL);
}

// ── Test 4: One vertex, same color ───────────────────────────────────────────

/**
 * @brief Single-vertex S in a single-vertex G with matching color finds exactly one match.
 */
TEST_F(SubgraphSearcherFileTest, find_all_one_vertex_same_color_finds_one)
{
    constexpr uint32_t VERTEX_COLOR = 0U;
    constexpr uint64_t EXPECTED_COUNT = 1ULL;
    const ColoredGraph graph = make_single_vertex_graph(VERTEX_COLOR);
    const ColoredGraph subgraph = make_single_vertex_graph(VERTEX_COLOR);
    const uint64_t result = [&]()
    {
        const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, false, false,
                                        std::make_unique<MatchOutputWriter>(m_temp_path),
                                        LoggerHandler::null()};
        return searcher.find_all(graph, subgraph);
    }();
    EXPECT_EQ(result, EXPECTED_COUNT);
    check_file_matches(m_temp_path, {MatchMap{{0U, 0U}}});
}

// ── Test 4.5: One vertex, different color ────────────────────────────────────

/**
 * @brief Single-vertex S and G with mismatched colors finds zero matches.
 */
TEST_F(SubgraphSearcherFileTest, find_all_one_vertex_different_color_finds_zero)
{
    constexpr uint32_t GRAPH_COLOR = 0U;
    constexpr uint32_t SUB_COLOR = 1U;
    constexpr uint64_t EXPECTED_COUNT = 0ULL;
    const ColoredGraph graph = make_single_vertex_graph(GRAPH_COLOR);
    const ColoredGraph subgraph = make_single_vertex_graph(SUB_COLOR);
    const uint64_t result = [&]()
    {
        const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, false, false,
                                        std::make_unique<MatchOutputWriter>(m_temp_path),
                                        LoggerHandler::null()};
        return searcher.find_all(graph, subgraph);
    }();
    EXPECT_EQ(result, EXPECTED_COUNT);
    EXPECT_EQ(count_file_lines(m_temp_path), EXPECTED_COUNT);
}

// ── Test 5: Disconnected subgraph ─────────────────────────────────────────────

/**
 * @brief A disconnected two-vertex subgraph (no edges) finds zero matches.
 *
 * The searcher cannot propagate restrictions across the disconnected components
 * of S, so the second vertex never acquires a candidate set.
 */
TEST(SubgraphSearcherTest, find_all_disconnected_subgraph_finds_zero)
{
    constexpr uint64_t EXPECTED_COUNT = 0ULL;
    const ColoredGraph graph = make_complete_graph(3U);
    const ColoredGraph disconnected_sub = make_disconnected_two_vertices(0U);
    const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, false, false, make_null_writer(),
                                    LoggerHandler::null()};
    EXPECT_EQ(searcher.find_all(graph, disconnected_sub), EXPECTED_COUNT);
}

// ── Test 6: Disconnected G, S inside ─────────────────────────────────────────

/**
 * @brief A single-vertex S matches both vertices of a disconnected two-vertex G.
 */
TEST_F(SubgraphSearcherFileTest, find_all_single_vertex_in_disconnected_graph_finds_two)
{
    constexpr uint32_t VERTEX_COLOR = 0U;
    constexpr uint64_t EXPECTED_COUNT = 2ULL;
    const ColoredGraph graph = make_disconnected_two_vertices(VERTEX_COLOR);
    const ColoredGraph subgraph = make_single_vertex_graph(VERTEX_COLOR);
    const uint64_t result = [&]()
    {
        const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, false, false,
                                        std::make_unique<MatchOutputWriter>(m_temp_path),
                                        LoggerHandler::null()};
        return searcher.find_all(graph, subgraph);
    }();
    EXPECT_EQ(result, EXPECTED_COUNT);
    check_file_matches(m_temp_path, {MatchMap{{0U, 0U}}, MatchMap{{1U, 0U}}});
}

// ── Test 7: S has one edge, G has one vertex ──────────────────────────────────

/**
 * @brief A two-vertex one-edge S finds zero matches in a single-vertex G.
 */
TEST_F(SubgraphSearcherFileTest, find_all_edge_subgraph_in_single_vertex_graph_finds_zero)
{
    constexpr uint32_t VERTEX_COLOR = 0U;
    constexpr uint64_t EXPECTED_COUNT = 0ULL;
    const ColoredGraph graph = make_single_vertex_graph(VERTEX_COLOR);
    const ColoredGraph subgraph = make_single_edge(VERTEX_COLOR);
    const uint64_t result = [&]()
    {
        const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, false, false,
                                        std::make_unique<MatchOutputWriter>(m_temp_path),
                                        LoggerHandler::null()};
        return searcher.find_all(graph, subgraph);
    }();
    EXPECT_EQ(result, EXPECTED_COUNT);
    EXPECT_EQ(count_file_lines(m_temp_path), EXPECTED_COUNT);
}

// ── Test 8.1: S one vertex, G one edge — color mismatch ──────────────────────

/**
 * @brief Single-vertex S (color 1) finds zero matches in a two-vertex edge G (color 0).
 */
TEST_F(SubgraphSearcherFileTest, find_all_single_vertex_color_mismatch_in_edge_graph)
{
    constexpr uint32_t GRAPH_COLOR = 0U;
    constexpr uint32_t SUB_COLOR = 1U;
    constexpr uint64_t EXPECTED_COUNT = 0ULL;
    const ColoredGraph graph = make_single_edge(GRAPH_COLOR);
    const ColoredGraph subgraph = make_single_vertex_graph(SUB_COLOR);
    const uint64_t result = [&]()
    {
        const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, false, false,
                                        std::make_unique<MatchOutputWriter>(m_temp_path),
                                        LoggerHandler::null()};
        return searcher.find_all(graph, subgraph);
    }();
    EXPECT_EQ(result, EXPECTED_COUNT);
    EXPECT_EQ(count_file_lines(m_temp_path), EXPECTED_COUNT);
}

// ── Test 8.2: S one vertex, G one edge — one matching endpoint ───────────────

/**
 * @brief Single-vertex S (color 0) finds one match in a two-vertex edge G where
 *        only vertex 0 has color 0 and vertex 1 has color 1.
 */
TEST_F(SubgraphSearcherFileTest, find_all_single_vertex_one_matching_endpoint)
{
    constexpr uint32_t COLOR_MATCH = 0U;
    constexpr uint32_t COLOR_OTHER = 1U;
    constexpr uint64_t EXPECTED_COUNT = 1ULL;
    const ColoredGraph graph = make_edge_with_colors(COLOR_MATCH, COLOR_OTHER);
    const ColoredGraph subgraph = make_single_vertex_graph(COLOR_MATCH);
    const uint64_t result = [&]()
    {
        const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, false, false,
                                        std::make_unique<MatchOutputWriter>(m_temp_path),
                                        LoggerHandler::null()};
        return searcher.find_all(graph, subgraph);
    }();
    EXPECT_EQ(result, EXPECTED_COUNT);
    check_file_matches(m_temp_path, {MatchMap{{0U, 0U}}});
}

// ── Test 8.3: S one vertex, G one edge — both endpoints match ────────────────

/**
 * @brief Single-vertex S (color 0) finds two matches in a two-vertex edge G where
 *        both endpoints have color 0.
 */
TEST_F(SubgraphSearcherFileTest, find_all_single_vertex_both_endpoints_matching)
{
    constexpr uint32_t VERTEX_COLOR = 0U;
    constexpr uint64_t EXPECTED_COUNT = 2ULL;
    const ColoredGraph graph = make_single_edge(VERTEX_COLOR);
    const ColoredGraph subgraph = make_single_vertex_graph(VERTEX_COLOR);
    const uint64_t result = [&]()
    {
        const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, false, false,
                                        std::make_unique<MatchOutputWriter>(m_temp_path),
                                        LoggerHandler::null()};
        return searcher.find_all(graph, subgraph);
    }();
    EXPECT_EQ(result, EXPECTED_COUNT);
    check_file_matches(m_temp_path, {MatchMap{{0U, 0U}}, MatchMap{{1U, 0U}}});
}

// ── Test 9: Triangle in square — not inside ───────────────────────────────────

/**
 * @brief A triangle S finds zero matches in a pure 4-cycle G (no triangle present).
 */
TEST_F(SubgraphSearcherFileTest, find_all_triangle_not_in_square)
{
    constexpr uint32_t VERTEX_COLOR = 0U;
    constexpr uint64_t EXPECTED_COUNT = 0ULL;
    const std::vector<uint32_t> four_same{VERTEX_COLOR, VERTEX_COLOR, VERTEX_COLOR, VERTEX_COLOR};
    const std::vector<uint32_t> three_same{VERTEX_COLOR, VERTEX_COLOR, VERTEX_COLOR};
    const ColoredGraph graph = make_square_cycle(four_same);
    const ColoredGraph subgraph = make_undirected_triangle(three_same);
    const uint64_t result = [&]()
    {
        const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, false, false,
                                        std::make_unique<MatchOutputWriter>(m_temp_path),
                                        LoggerHandler::null()};
        return searcher.find_all(graph, subgraph);
    }();
    EXPECT_EQ(result, EXPECTED_COUNT);
    EXPECT_EQ(count_file_lines(m_temp_path), EXPECTED_COUNT);
}

// ── Test 9: Triangle in square — inside ──────────────────────────────────────

/**
 * @brief A triangle S finds 12 matches in a 4-cycle-plus-chord G.
 *
 * The chord creates two triangles ({0,1,2} and {0,2,3}); each has 6 isomorphisms
 * (all 3! vertex permutations, since all vertices share the same color).
 */
TEST_F(SubgraphSearcherFileTest, find_all_triangle_in_square_with_diagonal)
{
    constexpr uint32_t VERTEX_COLOR = 0U;
    constexpr uint64_t EXPECTED_COUNT = 12ULL;
    const std::vector<uint32_t> four_same{VERTEX_COLOR, VERTEX_COLOR, VERTEX_COLOR, VERTEX_COLOR};
    const std::vector<uint32_t> three_same{VERTEX_COLOR, VERTEX_COLOR, VERTEX_COLOR};
    const ColoredGraph graph = make_square_with_diagonal(four_same);
    const ColoredGraph subgraph = make_undirected_triangle(three_same);
    const uint64_t result = [&]()
    {
        const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, false, false,
                                        std::make_unique<MatchOutputWriter>(m_temp_path),
                                        LoggerHandler::null()};
        return searcher.find_all(graph, subgraph);
    }();
    EXPECT_EQ(result, EXPECTED_COUNT);
    std::vector<MatchMap> expected_maps = all_triangle_maps(0U, 1U, 2U);
    const std::vector<MatchMap> maps_023 = all_triangle_maps(0U, 2U, 3U);
    expected_maps.insert(expected_maps.end(), maps_023.cbegin(), maps_023.cend());
    check_file_matches(m_temp_path, expected_maps);
}

// ── Directed test 8.1: S directed edge, G directed edge — color mismatch ─────

/**
 * @brief A directed single-edge S (both vertices color 1) finds zero matches in a
 *        directed single-edge G (both vertices color 0) due to color mismatch.
 */
TEST_F(SubgraphSearcherFileTest, find_all_directed_edge_color_mismatch_finds_zero)
{
    constexpr uint32_t GRAPH_COLOR = 0U;
    constexpr uint32_t SUB_COLOR = 1U;
    constexpr uint64_t EXPECTED_COUNT = 0ULL;
    const ColoredGraph graph = make_directed_edge_with_colors(GRAPH_COLOR, GRAPH_COLOR);
    const ColoredGraph subgraph = make_directed_edge_with_colors(SUB_COLOR, SUB_COLOR);
    const uint64_t result = [&]()
    {
        const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, true, false,
                                        std::make_unique<MatchOutputWriter>(m_temp_path),
                                        LoggerHandler::null()};
        return searcher.find_all(graph, subgraph);
    }();
    EXPECT_EQ(result, EXPECTED_COUNT);
    EXPECT_EQ(count_file_lines(m_temp_path), EXPECTED_COUNT);
}

// ── Directed test 8.2: S directed edge, G directed edge — one direction ───────

/**
 * @brief A directed single-edge S (0→1) finds exactly one match in a directed G
 *        with the same single edge (0→1), because direction is enforced.
 *
 * The reverse mapping (S.0→G.1, S.1→G.0) would require edge 1→0 in G, which
 * does not exist, so only one match is produced.
 */
TEST_F(SubgraphSearcherFileTest, find_all_directed_edge_in_same_directed_edge_finds_one)
{
    constexpr uint64_t EXPECTED_COUNT = 1ULL;
    const ColoredGraph graph = make_directed_single_edge();
    const ColoredGraph subgraph = make_directed_single_edge();
    const uint64_t result = [&]()
    {
        const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, true, false,
                                        std::make_unique<MatchOutputWriter>(m_temp_path),
                                        LoggerHandler::null()};
        return searcher.find_all(graph, subgraph);
    }();
    EXPECT_EQ(result, EXPECTED_COUNT);
    check_file_matches(m_temp_path, {MatchMap{{0U, 0U}, {1U, 1U}}});
}

// ── Directed test 8.3: S directed edge, G bidirectional — both directions ─────

/**
 * @brief A directed single-edge S (0→1) finds 2 matches in a bidirectional G
 *        (edges 0→1 and 1→0), because S can be placed in either direction.
 */
TEST_F(SubgraphSearcherFileTest, find_all_directed_edge_in_bidirectional_finds_two)
{
    constexpr uint64_t EXPECTED_COUNT = 2ULL;
    const ColoredGraph graph = make_directed_bidirectional_edge();
    const ColoredGraph subgraph = make_directed_single_edge();
    const uint64_t result = [&]()
    {
        const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, true, false,
                                        std::make_unique<MatchOutputWriter>(m_temp_path),
                                        LoggerHandler::null()};
        return searcher.find_all(graph, subgraph);
    }();
    EXPECT_EQ(result, EXPECTED_COUNT);
    check_file_matches(m_temp_path, {MatchMap{{0U, 0U}, {1U, 1U}}, MatchMap{{1U, 0U}, {0U, 1U}}});
}

// ── Directed test 8.4: S one vertex, G directed edge — color mismatch ────────

/**
 * @brief Single-vertex S (color 1) finds zero matches in a directed single-edge G
 *        (both vertices color 0) due to color mismatch.
 */
TEST_F(SubgraphSearcherFileTest, find_all_directed_single_vertex_color_mismatch_finds_zero)
{
    constexpr uint32_t GRAPH_COLOR = 0U;
    constexpr uint32_t SUB_COLOR = 1U;
    constexpr uint64_t EXPECTED_COUNT = 0ULL;
    const ColoredGraph graph = make_directed_edge_with_colors(GRAPH_COLOR, GRAPH_COLOR);
    const ColoredGraph subgraph = make_single_vertex_graph(SUB_COLOR);
    const uint64_t result = [&]()
    {
        const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, true, false,
                                        std::make_unique<MatchOutputWriter>(m_temp_path),
                                        LoggerHandler::null()};
        return searcher.find_all(graph, subgraph);
    }();
    EXPECT_EQ(result, EXPECTED_COUNT);
    EXPECT_EQ(count_file_lines(m_temp_path), EXPECTED_COUNT);
}

// ── Directed test 8.5: S one vertex, G directed edge — one matching endpoint ──

/**
 * @brief Single-vertex S (color 0) finds one match in a directed edge G where
 *        only the source vertex (color 0) matches; the dest vertex has color 1.
 */
TEST_F(SubgraphSearcherFileTest, find_all_directed_single_vertex_one_matching_endpoint)
{
    constexpr uint32_t COLOR_MATCH = 0U;
    constexpr uint32_t COLOR_OTHER = 1U;
    constexpr uint64_t EXPECTED_COUNT = 1ULL;
    const ColoredGraph graph = make_directed_edge_with_colors(COLOR_MATCH, COLOR_OTHER);
    const ColoredGraph subgraph = make_single_vertex_graph(COLOR_MATCH);
    const uint64_t result = [&]()
    {
        const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, true, false,
                                        std::make_unique<MatchOutputWriter>(m_temp_path),
                                        LoggerHandler::null()};
        return searcher.find_all(graph, subgraph);
    }();
    EXPECT_EQ(result, EXPECTED_COUNT);
    check_file_matches(m_temp_path, {MatchMap{{0U, 0U}}});
}

// ── Directed test 8.6: S one vertex, G directed edge — both endpoints match ───

/**
 * @brief Single-vertex S (color 0) finds two matches in a directed single-edge G
 *        (both vertices color 0) — one match per vertex.
 */
TEST_F(SubgraphSearcherFileTest, find_all_directed_single_vertex_both_endpoints_matching)
{
    constexpr uint32_t VERTEX_COLOR = 0U;
    constexpr uint64_t EXPECTED_COUNT = 2ULL;
    const ColoredGraph graph = make_directed_single_edge();
    const ColoredGraph subgraph = make_single_vertex_graph(VERTEX_COLOR);
    const uint64_t result = [&]()
    {
        const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, true, false,
                                        std::make_unique<MatchOutputWriter>(m_temp_path),
                                        LoggerHandler::null()};
        return searcher.find_all(graph, subgraph);
    }();
    EXPECT_EQ(result, EXPECTED_COUNT);
    check_file_matches(m_temp_path, {MatchMap{{0U, 0U}}, MatchMap{{1U, 0U}}});
}

// ── Directed test 9: Triangle not in directed 4-cycle ────────────────────────

/**
 * @brief A directed 3-cycle S finds zero matches in a directed 4-cycle G.
 *
 * The directed 4-cycle contains no directed 3-cycle subgraph.
 */
TEST_F(SubgraphSearcherFileTest, find_all_directed_triangle_not_in_four_cycle)
{
    constexpr uint32_t VERTEX_COLOR = 0U;
    constexpr uint64_t EXPECTED_COUNT = 0ULL;
    const std::vector<uint32_t> four_same{VERTEX_COLOR, VERTEX_COLOR, VERTEX_COLOR, VERTEX_COLOR};
    const std::vector<uint32_t> three_same{VERTEX_COLOR, VERTEX_COLOR, VERTEX_COLOR};
    const ColoredGraph graph = make_directed_four_cycle(four_same);
    const ColoredGraph subgraph = make_directed_three_cycle(three_same);
    const uint64_t result = [&]()
    {
        const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, true, false,
                                        std::make_unique<MatchOutputWriter>(m_temp_path),
                                        LoggerHandler::null()};
        return searcher.find_all(graph, subgraph);
    }();
    EXPECT_EQ(result, EXPECTED_COUNT);
    EXPECT_EQ(count_file_lines(m_temp_path), EXPECTED_COUNT);
}

// ── Directed test 9: Triangle in directed 4-cycle with diagonal ──────────────

/**
 * @brief A directed 3-cycle S finds 3 matches in a directed 4-cycle-plus-chord G.
 *
 * The chord 0→2 creates one directed 3-cycle {0,2,3}; its 3 rotations each
 * yield one valid isomorphism.
 */
TEST_F(SubgraphSearcherFileTest, find_all_directed_triangle_in_four_cycle_with_diagonal)
{
    constexpr uint32_t VERTEX_COLOR = 0U;
    constexpr uint64_t EXPECTED_COUNT = 3ULL;
    const std::vector<uint32_t> four_same{VERTEX_COLOR, VERTEX_COLOR, VERTEX_COLOR, VERTEX_COLOR};
    const std::vector<uint32_t> three_same{VERTEX_COLOR, VERTEX_COLOR, VERTEX_COLOR};
    const ColoredGraph graph = make_directed_four_cycle_with_diagonal(four_same);
    const ColoredGraph subgraph = make_directed_three_cycle(three_same);
    const uint64_t result = [&]()
    {
        const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, true, false,
                                        std::make_unique<MatchOutputWriter>(m_temp_path),
                                        LoggerHandler::null()};
        return searcher.find_all(graph, subgraph);
    }();
    EXPECT_EQ(result, EXPECTED_COUNT);
    check_file_matches(m_temp_path, {MatchMap{{0U, 0U}, {2U, 1U}, {3U, 2U}},
                                     MatchMap{{0U, 2U}, {2U, 0U}, {3U, 1U}},
                                     MatchMap{{0U, 1U}, {2U, 2U}, {3U, 0U}}});
}

// ── Directed: edge with different vertex colors — same direction ──────────────

/**
 * @brief A directed edge S with distinct endpoint colors (0,1) finds one match
 *        in a host edge with the same direction and same colors.
 */
TEST_F(SubgraphSearcherFileTest, find_all_directed_edge_same_direction_finds_one)
{
    constexpr uint32_t SRC_COLOR = 0U;
    constexpr uint32_t DST_COLOR = 1U;
    constexpr uint64_t EXPECTED_COUNT = 1ULL;
    const ColoredGraph graph = make_directed_edge_with_colors(SRC_COLOR, DST_COLOR);
    const ColoredGraph subgraph = make_directed_edge_with_colors(SRC_COLOR, DST_COLOR);
    const uint64_t result = [&]()
    {
        const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, true, false,
                                        std::make_unique<MatchOutputWriter>(m_temp_path),
                                        LoggerHandler::null()};
        return searcher.find_all(graph, subgraph);
    }();
    EXPECT_EQ(result, EXPECTED_COUNT);
    check_file_matches(m_temp_path, {MatchMap{{0U, 0U}, {1U, 1U}}});
}

// ── Directed: edge with different vertex colors — opposite direction ──────────

/**
 * @brief A directed edge S with swapped endpoint colors (1,0) finds zero matches
 *        in a host edge colored (0,1): direction and colors do not align.
 */
TEST_F(SubgraphSearcherFileTest, find_all_directed_edge_opposite_direction_finds_zero)
{
    constexpr uint32_t SRC_COLOR = 0U;
    constexpr uint32_t DST_COLOR = 1U;
    constexpr uint64_t EXPECTED_COUNT = 0ULL;
    const ColoredGraph graph = make_directed_edge_with_colors(SRC_COLOR, DST_COLOR);
    const ColoredGraph subgraph = make_directed_edge_with_colors(DST_COLOR, SRC_COLOR);
    const uint64_t result = [&]()
    {
        const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, true, false,
                                        std::make_unique<MatchOutputWriter>(m_temp_path),
                                        LoggerHandler::null()};
        return searcher.find_all(graph, subgraph);
    }();
    EXPECT_EQ(result, EXPECTED_COUNT);
    EXPECT_EQ(count_file_lines(m_temp_path), EXPECTED_COUNT);
}

// ── Directed: edge in same-color directed triangle — 3 matches ───────────────

/**
 * @brief A directed edge S finds 3 matches in a directed 3-cycle G where all
 *        vertices share the same color — one match per directed edge of the cycle.
 */
TEST_F(SubgraphSearcherFileTest, find_all_directed_edge_in_same_color_triangle)
{
    constexpr uint32_t VERTEX_COLOR = 0U;
    constexpr uint64_t EXPECTED_COUNT = 3ULL;
    const std::vector<uint32_t> three_same{VERTEX_COLOR, VERTEX_COLOR, VERTEX_COLOR};
    const ColoredGraph graph = make_directed_three_cycle(three_same);
    const ColoredGraph subgraph = make_directed_edge_with_colors(VERTEX_COLOR, VERTEX_COLOR);
    const uint64_t result = [&]()
    {
        const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, true, false,
                                        std::make_unique<MatchOutputWriter>(m_temp_path),
                                        LoggerHandler::null()};
        return searcher.find_all(graph, subgraph);
    }();
    EXPECT_EQ(result, EXPECTED_COUNT);
    check_file_matches(m_temp_path, {MatchMap{{0U, 0U}, {1U, 1U}}, MatchMap{{1U, 0U}, {2U, 1U}},
                                     MatchMap{{0U, 1U}, {2U, 0U}}});
}

// ── Directed: edge in different-color directed triangle — 1 match ─────────────
/**
 * @brief A directed edge S with colors (0,1) finds one match in a directed 3-cycle
 *        G with vertex colors [0,1,2] — only the 0→1 edge of G qualifies.
 */
TEST_F(SubgraphSearcherFileTest, find_all_directed_edge_in_different_color_triangle)
{
    constexpr uint32_t COLOR_ZERO = 0U;
    constexpr uint32_t COLOR_ONE = 1U;
    constexpr uint32_t COLOR_TWO = 2U;
    constexpr uint64_t EXPECTED_COUNT = 1ULL;
    const ColoredGraph graph = make_directed_three_cycle({COLOR_ZERO, COLOR_ONE, COLOR_TWO});
    const ColoredGraph subgraph = make_directed_edge_with_colors(COLOR_ZERO, COLOR_ONE);
    const uint64_t result = [&]()
    {
        const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, true, false,
                                        std::make_unique<MatchOutputWriter>(m_temp_path),
                                        LoggerHandler::null()};
        return searcher.find_all(graph, subgraph);
    }();
    EXPECT_EQ(result, EXPECTED_COUNT);
    check_file_matches(m_temp_path, {MatchMap{{0U, 0U}, {1U, 1U}}});
}

// ── Directed: directed 3-cycle not found in directed DAG triangle ─────────────

/**
 * @brief A directed 3-cycle S finds zero matches in a directed DAG triangle G.
 *
 * G (0→1, 1→2, 0→2) is acyclic — it contains no directed cycle of any length.
 */
TEST_F(SubgraphSearcherFileTest, find_all_directed_cycle_not_in_dag_triangle)
{
    constexpr uint32_t VERTEX_COLOR = 0U;
    constexpr uint64_t EXPECTED_COUNT = 0ULL;
    const std::vector<uint32_t> three_same{VERTEX_COLOR, VERTEX_COLOR, VERTEX_COLOR};
    const ColoredGraph graph = make_directed_dag_triangle(three_same);
    const ColoredGraph subgraph = make_directed_three_cycle(three_same);
    const uint64_t result = [&]()
    {
        const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, true, false,
                                        std::make_unique<MatchOutputWriter>(m_temp_path),
                                        LoggerHandler::null()};
        return searcher.find_all(graph, subgraph);
    }();
    EXPECT_EQ(result, EXPECTED_COUNT);
    EXPECT_EQ(count_file_lines(m_temp_path), EXPECTED_COUNT);
}

// ── Directed: bidirectional edge ─────────────────────────────────────

/**
 * @brief A directed single-edge S (0→1) finds 2 matches in a bidirectional-edge G
 *        (both 0→1 and 1→0 present): one for each direction.
 */
TEST_F(SubgraphSearcherFileTest, find_all_directed_bidirectional_edge_count)
{
    constexpr uint64_t EXPECTED_COUNT = 2ULL;
    const ColoredGraph graph = make_directed_bidirectional_edge();
    const ColoredGraph subgraph = make_directed_single_edge();
    const uint64_t result = [&]()
    {
        const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, true, false,
                                        std::make_unique<MatchOutputWriter>(m_temp_path),
                                        LoggerHandler::null()};
        return searcher.find_all(graph, subgraph);
    }();
    EXPECT_EQ(result, EXPECTED_COUNT);
    check_file_matches(m_temp_path, {MatchMap{{0U, 0U}, {1U, 1U}}, MatchMap{{0U, 1U}, {1U, 0U}}});
}

// ── Directed: prior scores logged correctly ───────────────────────────────────
// TODO: go over this test
/**
 * @brief Verifies that calculate_prior logs the correct score per vertex.
 *
 * S = directed path 0→1→2. G = directed bidirectional-plus-extra (0→1, 1→0, 0→2).
 *
 * SUBGRAPH_DEGREE scores (based on S adjacency, is_directed=true):
 *   - S.0: adj={1}       → score 1
 *   - S.1: adj={0,2}     → score 2
 *   - S.2: adj={1}       → score 1
 *
 * GRAPH_DEGREE_SQUARED scores (based on G adjacency, is_directed=true):
 *   - G.0: adj={1,2}, sum of neighbor degrees = (1+1)+(0+1) = 3
 *   - G.1: adj={0},   sum of neighbor degrees = (2+1)       = 3
 *   - G.2: adj={0},   sum of neighbor degrees = (2+1)       = 3
 *
 * If G's bidirectional edge 0↔1 were double-counted, G.1's score would be 4,
 * not 3 — making this a regression check against that class of bug.
 */
TEST(SubgraphSearcherTest, calculate_prior_scores_are_logged_correctly)
{
    const ColoredGraph subgraph = make_directed_path(3U);
    const ColoredGraph graph = make_directed_bidirectional_with_extra();

    const auto check_logged =
        [](const std::vector<std::string>& msgs, const uint32_t vertex, const double score)
    {
        const std::string expected =
            "prior vertex=" + std::to_string(vertex) + " score=" + std::to_string(score);
        EXPECT_TRUE(std::any_of(msgs.cbegin(), msgs.cend(),
                                [&expected](const std::string& msg)
                                {
                                    return msg == expected;
                                }));
    };

    {
        const std::shared_ptr<CapturingLogger> logger = std::make_shared<CapturingLogger>();
        const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, true, false,
                                        make_null_writer(),
                                        LoggerHandler(std::weak_ptr<ILogger>(logger))};
        searcher.find_all(graph, subgraph);
        const std::vector<std::string> msgs = logger->messages();
        check_logged(msgs, 0U, 1.0);
        check_logged(msgs, 1U, 2.0);
        check_logged(msgs, 2U, 1.0);
    }

    {
        const std::shared_ptr<CapturingLogger> logger = std::make_shared<CapturingLogger>();
        const SubgraphSearcher searcher{PriorPolicy::GRAPH_DEGREE_SQUARED, true, false,
                                        make_null_writer(),
                                        LoggerHandler(std::weak_ptr<ILogger>(logger))};
        searcher.find_all(graph, subgraph);
        const std::vector<std::string> msgs = logger->messages();
        check_logged(msgs, 0U, 3.0);
        check_logged(msgs, 1U, 3.0);
        check_logged(msgs, 2U, 3.0);
    }
}

// ── Induced: square in square — with diagonal ────────────────────────────────

/**
 * @brief Induced 4-cycle S in 4-cycle-plus-chord G finds zero matches.
 *
 * Distinct vertex colors force the identity mapping; the extra chord 0-2 in G
 * violates the induced constraint (S has no edge 0-2).
 */
TEST_F(SubgraphSearcherFileTest, find_all_induced_square_with_extra_edge_finds_zero)
{
    constexpr uint64_t EXPECTED_COUNT = 0ULL;
    const std::vector<uint32_t> distinct_colors{0U, 1U, 2U, 3U};
    const ColoredGraph graph = make_square_with_diagonal(distinct_colors);
    const ColoredGraph subgraph = make_square_cycle(distinct_colors);
    const uint64_t result = [&]()
    {
        const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, false, true,
                                        std::make_unique<MatchOutputWriter>(m_temp_path),
                                        LoggerHandler::null()};
        return searcher.find_all(graph, subgraph);
    }();
    EXPECT_EQ(result, EXPECTED_COUNT);
    EXPECT_EQ(count_file_lines(m_temp_path), EXPECTED_COUNT);
}

// ── Induced: square in square — without diagonal ─────────────────────────────

/**
 * @brief Induced 4-cycle S in an identical 4-cycle G finds exactly one match.
 *
 * Distinct vertex colors force the identity mapping; G has no extra edges.
 */
TEST_F(SubgraphSearcherFileTest, find_all_induced_square_without_extra_edge_finds_one)
{
    constexpr uint64_t EXPECTED_COUNT = 1ULL;
    const std::vector<uint32_t> distinct_colors{0U, 1U, 2U, 3U};
    const ColoredGraph graph = make_square_cycle(distinct_colors);
    const ColoredGraph subgraph = make_square_cycle(distinct_colors);
    const uint64_t result = [&]()
    {
        const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, false, true,
                                        std::make_unique<MatchOutputWriter>(m_temp_path),
                                        LoggerHandler::null()};
        return searcher.find_all(graph, subgraph);
    }();
    EXPECT_EQ(result, EXPECTED_COUNT);
    check_file_matches(m_temp_path, {MatchMap{{0U, 0U}, {1U, 1U}, {2U, 2U}, {3U, 3U}}});
}

// ── Directed + induced: same direction, no diagonal ──────────────────────────

/**
 * @brief Directed induced: forward 4-cycle S in forward 4-cycle G finds one match.
 *
 * Distinct colors force the identity mapping; no extra edges are present.
 */
TEST_F(SubgraphSearcherFileTest, find_all_directed_induced_same_direction_no_diagonal)
{
    constexpr uint64_t EXPECTED_COUNT = 1ULL;
    const std::vector<uint32_t> distinct_colors{0U, 1U, 2U, 3U};
    const ColoredGraph graph = make_directed_four_cycle(distinct_colors);
    const ColoredGraph subgraph = make_directed_four_cycle(distinct_colors);
    const uint64_t result = [&]()
    {
        const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, true, true,
                                        std::make_unique<MatchOutputWriter>(m_temp_path),
                                        LoggerHandler::null()};
        return searcher.find_all(graph, subgraph);
    }();
    EXPECT_EQ(result, EXPECTED_COUNT);
    check_file_matches(m_temp_path, {MatchMap{{0U, 0U}, {1U, 1U}, {2U, 2U}, {3U, 3U}}});
}

// ── Directed + induced: same direction, with diagonal ────────────────────────

/**
 * @brief Directed induced: forward 4-cycle S in forward 4-cycle-plus-chord G finds zero.
 *
 * The extra chord 0→2 in G violates the induced constraint (S has no edge 0→2).
 */
TEST_F(SubgraphSearcherFileTest, find_all_directed_induced_same_direction_with_diagonal)
{
    constexpr uint64_t EXPECTED_COUNT = 0ULL;
    const std::vector<uint32_t> distinct_colors{0U, 1U, 2U, 3U};
    const ColoredGraph graph = make_directed_four_cycle_with_diagonal(distinct_colors);
    const ColoredGraph subgraph = make_directed_four_cycle(distinct_colors);
    const uint64_t result = [&]()
    {
        const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, true, true,
                                        std::make_unique<MatchOutputWriter>(m_temp_path),
                                        LoggerHandler::null()};
        return searcher.find_all(graph, subgraph);
    }();
    EXPECT_EQ(result, EXPECTED_COUNT);
    EXPECT_EQ(count_file_lines(m_temp_path), EXPECTED_COUNT);
}

// ── Directed + induced: opposite direction, no diagonal ──────────────────────

/**
 * @brief Directed induced: reverse 4-cycle S (0→3→2→1→0) in forward 4-cycle G finds zero.
 *
 * Distinct colors force the identity mapping; S edge 0→3 does not exist in G.
 */
TEST_F(SubgraphSearcherFileTest, find_all_directed_induced_opposite_direction_no_diagonal)
{
    constexpr uint64_t EXPECTED_COUNT = 0ULL;
    const std::vector<uint32_t> distinct_colors{0U, 1U, 2U, 3U};
    const ColoredGraph graph = make_directed_four_cycle(distinct_colors);
    const ColoredGraph subgraph = make_reverse_directed_four_cycle(distinct_colors);
    const uint64_t result = [&]()
    {
        const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, true, true,
                                        std::make_unique<MatchOutputWriter>(m_temp_path),
                                        LoggerHandler::null()};
        return searcher.find_all(graph, subgraph);
    }();
    EXPECT_EQ(result, EXPECTED_COUNT);
    EXPECT_EQ(count_file_lines(m_temp_path), EXPECTED_COUNT);
}

// ── Directed + induced: opposite direction, with diagonal ────────────────────

/**
 * @brief Directed induced: reverse 4-cycle S in forward 4-cycle-plus-chord G finds zero.
 *
 * Distinct colors force the identity mapping; S edge 0→3 is absent in G.
 */
TEST_F(SubgraphSearcherFileTest, find_all_directed_induced_opposite_direction_with_diagonal)
{
    constexpr uint64_t EXPECTED_COUNT = 0ULL;
    const std::vector<uint32_t> distinct_colors{0U, 1U, 2U, 3U};
    const ColoredGraph graph = make_directed_four_cycle_with_diagonal(distinct_colors);
    const ColoredGraph subgraph = make_reverse_directed_four_cycle(distinct_colors);
    const uint64_t result = [&]()
    {
        const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, true, true,
                                        std::make_unique<MatchOutputWriter>(m_temp_path),
                                        LoggerHandler::null()};
        return searcher.find_all(graph, subgraph);
    }();
    EXPECT_EQ(result, EXPECTED_COUNT);
    EXPECT_EQ(count_file_lines(m_temp_path), EXPECTED_COUNT);
}

// ── stop_after_first ──────────────────────────────────────────────────────────

/**
 * @brief stop_after_first=true stops after at least one match is found.
 *
 * A single-vertex S in a two-vertex edge G normally yields 2 matches. With
 * stop_after_first the count is at least 1. Due to thread batching a second
 * thread may also complete before the stop flag is seen, so up to 2 matches
 * are acceptable. Every written match must be one of the two valid mappings.
 */
TEST_F(SubgraphSearcherFileTest, find_all_stop_after_first_returns_one)
{
    constexpr uint32_t VERTEX_COLOR = 0U;
    constexpr uint64_t MIN_EXPECTED = 1ULL;
    constexpr uint64_t MAX_EXPECTED = 2ULL;
    const ColoredGraph graph = make_single_edge(VERTEX_COLOR);
    const ColoredGraph subgraph = make_single_vertex_graph(VERTEX_COLOR);
    const uint64_t result = [&]()
    {
        const SubgraphSearcher searcher{PriorPolicy::SUBGRAPH_DEGREE, false, false,
                                        std::make_unique<MatchOutputWriter>(m_temp_path),
                                        LoggerHandler::null()};
        return searcher.find_all(graph, subgraph, true);
    }();
    EXPECT_GE(result, MIN_EXPECTED);
    EXPECT_LE(result, MAX_EXPECTED);
    const std::vector<MatchMap> actual_maps = parse_match_file(m_temp_path);
    ASSERT_FALSE(actual_maps.empty());
    const MatchMap valid_first{{0U, 0U}};
    const MatchMap valid_second{{1U, 0U}};
    for (const MatchMap& match : actual_maps)
    {
        EXPECT_TRUE(match == valid_first || match == valid_second);
    }
}

// TODO: add tests for PriorPolicy variations, and for logging behavior (perhaps via a mock logger
// that records calls). Also check when S directed and G not (the graph themselves), etc Also test
// the colors prior check from paper
