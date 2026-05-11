#include "PathProcessor.h"

#include "ColoredGraph.h"
#include "FileLogger.h"
#include "GroupEnumerationPreprocessor.h"
#include "ILogger.h"
#include "LoggerHandler.h"

#include <algorithm>
#include <cstdint>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>
using namespace sgf;

namespace
{

LoggerHandler make_null_logger()
{
    return LoggerHandler(std::weak_ptr<ILogger>{});
}

std::unordered_map<__int128_t, uint32_t, Int128Hash> run(const ColoredGraph& graph)
{
    PathProcessor processor(graph, make_null_logger());
    return processor.calculate();
}

__int128_t pack_colors(const std::vector<uint32_t>& colors)
{
    constexpr uint32_t COLOR_BITS = 24U;
    __int128_t packed = 0;
    for (const uint32_t color : colors)
    {
        packed <<= COLOR_BITS;
        packed |= color;
    }
    return packed;
}

__int128_t undirected_motif(const std::vector<uint32_t>& colors)
{
    const std::vector<uint32_t> rev(colors.crbegin(), colors.crend());
    return std::min(pack_colors(colors), pack_colors(rev));
}

ColoredGraph make_undirected_chain(std::vector<uint32_t> colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {1U, 2U}, {2U, 3U}, {3U, 4U}};
    return ColoredGraph(5U, edges, colors, false);
}

ColoredGraph make_directed_chain(std::vector<uint32_t> colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {1U, 2U}, {2U, 3U}, {3U, 4U}};
    return ColoredGraph(5U, edges, colors, true);
}

ColoredGraph make_reversed_directed_chain(std::vector<uint32_t> colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{4U, 3U}, {3U, 2U}, {2U, 1U}, {1U, 0U}};
    return ColoredGraph(5U, edges, colors, true);
}

// Mask covering the lower 120 bits (5 vertices × 24 color bits).
// All keys returned from an all-zero-color graph must have this portion equal to 0.
const __int128_t COLOR_PORTION_MASK = (static_cast<__int128_t>(1) << (5U * 24U)) - 1;

}  // namespace

// ── Edge cases ────────────────────────────────────────────────────────────────

TEST(PathProcessorTest, EmptyGraph)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    const ColoredGraph graph(0U, edges, {}, false);
    EXPECT_TRUE(run(graph).empty());
}

TEST(PathProcessorTest, OneVertex)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    const ColoredGraph graph(1U, edges, {1U}, false);
    EXPECT_TRUE(run(graph).empty());
}

TEST(PathProcessorTest, TwoVerticesUndirected)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}};
    const ColoredGraph graph(2U, edges, {1U, 2U}, false);
    EXPECT_TRUE(run(graph).empty());
}

TEST(PathProcessorTest, TwoVerticesDirected)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}};
    const ColoredGraph graph(2U, edges, {1U, 2U}, true);
    EXPECT_TRUE(run(graph).empty());
}

TEST(PathProcessorTest, ThreeVerticesChainUndirected)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {1U, 2U}};
    const ColoredGraph graph(3U, edges, {1U, 2U, 3U}, false);
    EXPECT_TRUE(run(graph).empty());
}

TEST(PathProcessorTest, ThreeVerticesTriangle)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {1U, 2U}, {0U, 2U}};
    const ColoredGraph graph(3U, edges, {1U, 2U, 3U}, false);
    EXPECT_TRUE(run(graph).empty());
}

TEST(PathProcessorTest, FourVerticesChainUndirected)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {1U, 2U}, {2U, 3U}};
    const ColoredGraph graph(4U, edges, {1U, 2U, 3U, 4U}, false);
    EXPECT_TRUE(run(graph).empty());
}

TEST(PathProcessorTest, FourVerticesChainDirected)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {1U, 2U}, {2U, 3U}};
    const ColoredGraph graph(4U, edges, {1U, 2U, 3U, 4U}, true);
    EXPECT_TRUE(run(graph).empty());
}

TEST(PathProcessorTest, FourVerticesCompleteUndirected)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {0U, 2U}, {0U, 3U},
                                                        {1U, 2U}, {1U, 3U}, {2U, 3U}};
    const ColoredGraph graph(4U, edges, {1U, 2U, 3U, 4U}, false);
    EXPECT_TRUE(run(graph).empty());
}

TEST(PathProcessorTest, FiveDisconnectedVertices)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    const ColoredGraph graph(5U, edges, {1U, 2U, 3U, 4U, 5U}, false);
    EXPECT_TRUE(run(graph).empty());
}

TEST(PathProcessorTest, FourChainPlusIsolatedVertexUndirected)
{
    // 0-1-2-3 is a chain; vertex 4 is isolated.
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {1U, 2U}, {2U, 3U}};
    const ColoredGraph graph(5U, edges, {1U, 2U, 3U, 4U, 5U}, false);
    EXPECT_TRUE(run(graph).empty());
}

TEST(PathProcessorTest, FourChainPlusIsolatedVertexDirected)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {1U, 2U}, {2U, 3U}};
    const ColoredGraph graph(5U, edges, {1U, 2U, 3U, 4U, 5U}, true);
    EXPECT_TRUE(run(graph).empty());
}

TEST(PathProcessorTest, TwoDisconnectedEdgesUndirected)
{
    // 0-1 and 3-4 are disconnected; vertex 2 is isolated.
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {3U, 4U}};
    const ColoredGraph graph(5U, edges, {1U, 2U, 3U, 4U, 5U}, false);
    EXPECT_TRUE(run(graph).empty());
}

TEST(PathProcessorTest, StarK14Undirected)
{
    // Center=0 connects to all leaves; no 5-vertex simple path exists.
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {0U, 2U}, {0U, 3U}, {0U, 4U}};
    const ColoredGraph graph(5U, edges, {0U, 0U, 0U, 0U, 0U}, false);
    EXPECT_TRUE(run(graph).empty());
}

TEST(PathProcessorTest, StarK14Directed)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {0U, 2U}, {0U, 3U}, {0U, 4U}};
    const ColoredGraph graph(5U, edges, {0U, 0U, 0U, 0U, 0U}, true);
    EXPECT_TRUE(run(graph).empty());
}

// ── Undirected chain: all 120 permutations of colors 1–5 ─────────────────────

TEST(PathProcessorTest, UndirectedChainAllColorPermutations)
{
    std::vector<uint32_t> colors = {1U, 2U, 3U, 4U, 5U};
    do
    {
        const ColoredGraph graph = make_undirected_chain(colors);
        const std::unordered_map<__int128_t, uint32_t, Int128Hash> result = run(graph);

        ASSERT_EQ(result.size(), 1U)
            << "expected exactly one motif for colors [" << colors[0] << "," << colors[1] << ","
            << colors[2] << "," << colors[3] << "," << colors[4] << "]";

        const __int128_t expected_key = undirected_motif(colors);
        ASSERT_EQ(result.count(expected_key), 1U) << "expected motif key not found";
        EXPECT_EQ(result.at(expected_key), 1U) << "expected path count of 1";
    } while (std::next_permutation(colors.begin(), colors.end()));
}

TEST(PathProcessorTest, UndirectedChainReversePermutationSameMotif)
{
    // A color sequence and its reverse must canonicalize to the same motif.
    const std::vector<uint32_t> fwd = {1U, 2U, 3U, 4U, 5U};
    const std::vector<uint32_t> rev(fwd.crbegin(), fwd.crend());

    const std::unordered_map<__int128_t, uint32_t, Int128Hash> fwd_result = run(make_undirected_chain(fwd));
    const std::unordered_map<__int128_t, uint32_t, Int128Hash> rev_result = run(make_undirected_chain(rev));

    ASSERT_EQ(fwd_result.size(), 1U);
    ASSERT_EQ(rev_result.size(), 1U);
    EXPECT_EQ(fwd_result.begin()->first, rev_result.begin()->first);
}

// ── Directed chain: all 120 permutations of colors 1–5 ───────────────────────

TEST(PathProcessorTest, DirectedChainAllColorPermutations)
{
    // Both forward and backward traversals of the same directed path must
    // canonicalize to the same motif key.
    std::vector<uint32_t> colors = {1U, 2U, 3U, 4U, 5U};
    do
    {
        const ColoredGraph graph = make_directed_chain(colors);
        const std::unordered_map<__int128_t, uint32_t, Int128Hash> result = run(graph);

        ASSERT_EQ(result.size(), 1U)
            << "directed chain should produce one unique motif for colors [" << colors[0] << ","
            << colors[1] << "," << colors[2] << "," << colors[3] << "," << colors[4] << "]";
    } while (std::next_permutation(colors.begin(), colors.end()));
}

std::string int128_to_binary_string(__uint128_t value)
{
    std::string result(128, '0');

    for (int i = 127; i >= 0; --i)
    {
        if (value & (static_cast<__uint128_t>(1) << i))
        {
            result[127 - i] = '1';
        }
    }

    return result;
}

std::string int128_to_string(__int128_t value)
{
    if (value == 0)
    {
        return "0";
    }

    bool negative = value < 0;
    __uint128_t temp =
        negative ? -static_cast<__uint128_t>(value) : static_cast<__uint128_t>(value);

    std::string result;

    while (temp > 0)
    {
        result.push_back(static_cast<char>('0' + (temp % 10)));
        temp /= 10;
    }

    if (negative)
    {
        result.push_back('-');
    }

    std::reverse(result.begin(), result.end());
    return result;
}

TEST(PathProcessorTest, DirectedChainReversePermutationSameMotif)
{
    const std::vector<uint32_t> fwd = {1U, 2U, 3U, 4U, 5U};
    const std::vector<uint32_t> rev(fwd.crbegin(), fwd.crend());

    const std::unordered_map<__int128_t, uint32_t, Int128Hash> fwd_result =
        run(make_directed_chain(fwd));
    const std::unordered_map<__int128_t, uint32_t, Int128Hash> rev_result =
        run(make_reversed_directed_chain(rev));

    ASSERT_EQ(fwd_result.size(), 1U);
    ASSERT_EQ(rev_result.size(), 1U);
    EXPECT_EQ(fwd_result.begin()->first, rev_result.begin()->first);
}

// ── All-zero colors: verify motif key is 0 (undirected) / color bits are 0 ───

TEST(PathProcessorTest, UndirectedChainAllZeroColors)
{
    const std::vector<uint32_t> zero = {0U, 0U, 0U, 0U, 0U};
    const std::unordered_map<__int128_t, uint32_t, Int128Hash> result = run(make_undirected_chain(zero));
    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.count(static_cast<__int128_t>(0)), 1U);
    EXPECT_EQ(result.at(static_cast<__int128_t>(0)), 1U);
}

TEST(PathProcessorTest, DirectedChainAllZeroColors)
{
    const std::vector<uint32_t> zero = {0U, 0U, 0U, 0U, 0U};
    const std::unordered_map<__int128_t, uint32_t, Int128Hash> result = run(make_directed_chain(zero));
    ASSERT_FALSE(result.empty());
    for (const std::pair<const __int128_t, uint32_t>& entry : result)
    {
        EXPECT_EQ(entry.first & COLOR_PORTION_MASK, static_cast<__int128_t>(0))
            << "color bits must be zero when all vertex colors are zero";
    }
}

// ── 5-vertex graphs with extra edges, all colors 0 ───────────────────────────
// For undirected graphs, motif key must be 0 for any found path.
// For directed graphs, color bits of every motif key must be 0.

TEST(PathProcessorTest, UndirectedChordGraph02AllZeroColors)
{
    // Chain 0-1-2-3-4 plus chord 0-2.
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {0U, 1U}, {1U, 2U}, {2U, 3U}, {3U, 4U}, {0U, 2U}};
    const std::vector<uint32_t> zero = {0U, 0U, 0U, 0U, 0U};
    const ColoredGraph graph(5U, edges, zero, false);
    const std::unordered_map<__int128_t, uint32_t, Int128Hash> result = run(graph);
    ASSERT_FALSE(result.empty());
    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.count(static_cast<__int128_t>(0)), 1U);
}

TEST(PathProcessorTest, DirectedChordGraph02AllZeroColors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {0U, 1U}, {1U, 2U}, {2U, 3U}, {3U, 4U}, {0U, 2U}};
    const std::vector<uint32_t> zero = {0U, 0U, 0U, 0U, 0U};
    const ColoredGraph graph(5U, edges, zero, true);
    for (const std::pair<const __int128_t, uint32_t>& entry : run(graph))
    {
        EXPECT_EQ(entry.first & COLOR_PORTION_MASK, static_cast<__int128_t>(0));
    }
}

TEST(PathProcessorTest, UndirectedChordGraph13AllZeroColors)
{
    // Chain 0-1-2-3-4 plus chord 1-3.
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {0U, 1U}, {1U, 2U}, {2U, 3U}, {3U, 4U}, {1U, 3U}};
    const std::vector<uint32_t> zero = {0U, 0U, 0U, 0U, 0U};
    const ColoredGraph graph(5U, edges, zero, false);
    const std::unordered_map<__int128_t, uint32_t, Int128Hash> result = run(graph);
    ASSERT_FALSE(result.empty());
    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.count(static_cast<__int128_t>(0)), 1U);
}

TEST(PathProcessorTest, DirectedChordGraph13AllZeroColors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {0U, 1U}, {1U, 2U}, {2U, 3U}, {3U, 4U}, {1U, 3U}};
    const std::vector<uint32_t> zero = {0U, 0U, 0U, 0U, 0U};
    const ColoredGraph graph(5U, edges, zero, true);
    for (const std::pair<const __int128_t, uint32_t>& entry : run(graph))
    {
        EXPECT_EQ(entry.first & COLOR_PORTION_MASK, static_cast<__int128_t>(0));
    }
}

TEST(PathProcessorTest, UndirectedChordGraph14AllZeroColors)
{
    // Chain 0-1-2-3-4 plus chord 1-4.
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {0U, 1U}, {1U, 2U}, {2U, 3U}, {3U, 4U}, {1U, 4U}};
    const std::vector<uint32_t> zero = {0U, 0U, 0U, 0U, 0U};
    const ColoredGraph graph(5U, edges, zero, false);
    const std::unordered_map<__int128_t, uint32_t, Int128Hash> result = run(graph);
    ASSERT_FALSE(result.empty());
    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.count(static_cast<__int128_t>(0)), 1U);
}

TEST(PathProcessorTest, DirectedChordGraph14AllZeroColors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {0U, 1U}, {1U, 2U}, {2U, 3U}, {3U, 4U}, {1U, 4U}};
    const std::vector<uint32_t> zero = {0U, 0U, 0U, 0U, 0U};
    const ColoredGraph graph(5U, edges, zero, true);
    for (const std::pair<const __int128_t, uint32_t>& entry : run(graph))
    {
        EXPECT_EQ(entry.first & COLOR_PORTION_MASK, static_cast<__int128_t>(0));
    }
}

TEST(PathProcessorTest, UndirectedChordGraph24AllZeroColors)
{
    // Chain 0-1-2-3-4 plus chord 2-4.
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {0U, 1U}, {1U, 2U}, {2U, 3U}, {3U, 4U}, {2U, 4U}};
    const std::vector<uint32_t> zero = {0U, 0U, 0U, 0U, 0U};
    const ColoredGraph graph(5U, edges, zero, false);
    const std::unordered_map<__int128_t, uint32_t, Int128Hash> result = run(graph);
    ASSERT_FALSE(result.empty());
    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.count(static_cast<__int128_t>(0)), 1U);
}

TEST(PathProcessorTest, DirectedChordGraph24AllZeroColors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {0U, 1U}, {1U, 2U}, {2U, 3U}, {3U, 4U}, {2U, 4U}};
    const std::vector<uint32_t> zero = {0U, 0U, 0U, 0U, 0U};
    const ColoredGraph graph(5U, edges, zero, true);
    for (const std::pair<const __int128_t, uint32_t>& entry : run(graph))
    {
        EXPECT_EQ(entry.first & COLOR_PORTION_MASK, static_cast<__int128_t>(0));
    }
}

TEST(PathProcessorTest, UndirectedCycleC3AllZeroColors)
{
    // 3-cycle has only 3 vertices — no 5-vertex simple path can exist.
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {1U, 2U}, {2U, 0U}};
    const std::vector<uint32_t> zero = {0U, 0U, 0U};
    const ColoredGraph graph(3U, edges, zero, false);
    EXPECT_TRUE(run(graph).empty());
}

TEST(PathProcessorTest, UndirectedCycleC4AllZeroColors)
{
    // 4-cycle has only 4 vertices — no 5-vertex simple path can exist.
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {1U, 2U}, {2U, 3U}, {3U, 0U}};
    const std::vector<uint32_t> zero = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, zero, false);
    EXPECT_TRUE(run(graph).empty());
}

TEST(PathProcessorTest, UndirectedCycleC5AllZeroColors)
{
    // 5-cycle: 0-1-2-3-4-0.
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {0U, 1U}, {1U, 2U}, {2U, 3U}, {3U, 4U}, {4U, 0U}};
    const std::vector<uint32_t> zero = {0U, 0U, 0U, 0U, 0U};
    const ColoredGraph graph(5U, edges, zero, false);
    const std::unordered_map<__int128_t, uint32_t, Int128Hash> result = run(graph);
    ASSERT_FALSE(result.empty());
    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(static_cast<__int128_t>(0)), 5U);
}

TEST(PathProcessorTest, DirectedCycleC5AllZeroColors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {0U, 1U}, {1U, 2U}, {2U, 3U}, {3U, 4U}, {4U, 0U}};
    const std::vector<uint32_t> zero = {0U, 0U, 0U, 0U, 0U};
    const ColoredGraph graph(5U, edges, zero, true);
    for (const std::pair<const __int128_t, uint32_t>& entry : run(graph))
    {
        EXPECT_EQ(entry.first & COLOR_PORTION_MASK, static_cast<__int128_t>(0));
    }
}

TEST(PathProcessorTest, UndirectedCycleC6AllZeroColors)
{
    // 6-cycle: each of the 6 vertices can be excluded to leave a 5-vertex chain.
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {1U, 2U}, {2U, 3U},
                                                        {3U, 4U}, {4U, 5U}, {5U, 0U}};
    const std::vector<uint32_t> zero = {0U, 0U, 0U, 0U, 0U, 0U};
    const ColoredGraph graph(6U, edges, zero, false);
    const std::unordered_map<__int128_t, uint32_t, Int128Hash> result = run(graph);
    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(static_cast<__int128_t>(0)), 6U);
}

TEST(PathProcessorTest, UndirectedCompleteK5AllZeroColors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {0U, 2U}, {0U, 3U}, {0U, 4U},
                                                        {1U, 2U}, {1U, 3U}, {1U, 4U}, {2U, 3U},
                                                        {2U, 4U}, {3U, 4U}};
    const std::vector<uint32_t> zero = {0U, 0U, 0U, 0U, 0U};
    const ColoredGraph graph(5U, edges, zero, false);
    const std::unordered_map<__int128_t, uint32_t, Int128Hash> result = run(graph);
    ASSERT_FALSE(result.empty());
    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.count(static_cast<__int128_t>(0)), 1U);
}

TEST(PathProcessorTest, DirectedCompleteK5AllZeroColors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {0U, 2U}, {0U, 3U}, {0U, 4U},
                                                        {1U, 2U}, {1U, 3U}, {1U, 4U}, {2U, 3U},
                                                        {2U, 4U}, {3U, 4U}};
    const std::vector<uint32_t> zero = {0U, 0U, 0U, 0U, 0U};
    const ColoredGraph graph(5U, edges, zero, true);
    for (const std::pair<const __int128_t, uint32_t>& entry : run(graph))
    {
        EXPECT_EQ(entry.first & COLOR_PORTION_MASK, static_cast<__int128_t>(0));
    }
}

// ── Colored cycle tests: C3, C4, C5, C6 ─────────────────────────────────────

TEST(PathProcessorTest, UndirectedCycleC3Colors123)
{
    // 3 vertices — no 5-vertex path regardless of colors.
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {1U, 2U}, {2U, 0U}};
    const std::vector<uint32_t> colors = {1U, 2U, 3U};
    const ColoredGraph graph(3U, edges, colors, false);
    EXPECT_TRUE(run(graph).empty());
}

TEST(PathProcessorTest, UndirectedCycleC4Colors1234)
{
    // 4 vertices — no 5-vertex path regardless of colors.
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {1U, 2U}, {2U, 3U}, {3U, 0U}};
    const std::vector<uint32_t> colors = {1U, 2U, 3U, 4U};
    const ColoredGraph graph(4U, edges, colors, false);
    EXPECT_TRUE(run(graph).empty());
}

TEST(PathProcessorTest, UndirectedCycleC5Colors12345)
{
    // Each of the 5 Hamiltonian paths in C5 is a distinct rotation of [1,2,3,4,5].
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {0U, 1U}, {1U, 2U}, {2U, 3U}, {3U, 4U}, {4U, 0U}};
    const std::vector<uint32_t> colors = {1U, 2U, 3U, 4U, 5U};
    const ColoredGraph graph(5U, edges, colors, false);
    const std::unordered_map<__int128_t, uint32_t, Int128Hash> result = run(graph);
    ASSERT_EQ(result.size(), 5U);
    for (const std::pair<const __int128_t, uint32_t>& entry : result)
    {
        EXPECT_EQ(entry.second, 1U);
    }
    const std::vector<std::vector<uint32_t>> expected_paths = {{1U, 2U, 3U, 4U, 5U},
                                                               {2U, 3U, 4U, 5U, 1U},
                                                               {3U, 4U, 5U, 1U, 2U},
                                                               {4U, 5U, 1U, 2U, 3U},
                                                               {5U, 1U, 2U, 3U, 4U}};
    for (const std::vector<uint32_t>& path : expected_paths)
    {
        EXPECT_EQ(result.count(undirected_motif(path)), 1U);
    }
}

TEST(PathProcessorTest, UndirectedCycleC6Colors123456)
{
    // C6 has 6 simple 5-vertex paths (one per excluded vertex), all distinct.
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {1U, 2U}, {2U, 3U},
                                                        {3U, 4U}, {4U, 5U}, {5U, 0U}};
    const std::vector<uint32_t> colors = {1U, 2U, 3U, 4U, 5U, 6U};
    const ColoredGraph graph(6U, edges, colors, false);
    const std::unordered_map<__int128_t, uint32_t, Int128Hash> result = run(graph);
    ASSERT_EQ(result.size(), 6U);
    for (const std::pair<const __int128_t, uint32_t>& entry : result)
    {
        EXPECT_EQ(entry.second, 1U);
    }
    const std::vector<std::vector<uint32_t>> expected_paths = {
        {2U, 3U, 4U, 5U, 6U}, {3U, 4U, 5U, 6U, 1U}, {2U, 1U, 6U, 5U, 4U},
        {3U, 2U, 1U, 6U, 5U}, {4U, 3U, 2U, 1U, 6U}, {1U, 2U, 3U, 4U, 5U}};
    for (const std::vector<uint32_t>& path : expected_paths)
    {
        EXPECT_EQ(result.count(undirected_motif(path)), 1U);
    }
}
