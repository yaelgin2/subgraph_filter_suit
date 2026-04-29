#include "MotifPreprocessor.h"

#include "ColoredGraph.h"
#include "Constants.h"
#include "ILogger.h"
#include "LoggerHandler.h"

#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <unordered_map>
#include <vector>

using namespace sgf;

namespace
{

class MotifPreprocessorTest : public ::testing::Test
{
protected:
    /**
     * @brief Creates a no-op LoggerHandler for use in tests.
     * @return LoggerHandler backed by an expired weak_ptr (all log calls are no-ops).
     */
    static LoggerHandler null_logger()
    {
        return LoggerHandler{std::weak_ptr<ILogger>{}};
    }

    /**
     * @brief Canonical motif key with zero (uncolored) color encoding.
     * @param structure_id Canonical motif structure ID (11, 13, 15, 30, 31, or 63).
     * @return 128-bit key with structure in the high 32 bits and zeros in the low 96 bits.
     */
    static __uint128_t zero_color_key(const uint32_t structure_id)
    {
        return static_cast<__uint128_t>(structure_id)
               << (SgfConstants::MOTIF_SIZE * SgfConstants::BITS_PER_COLOR);
    }

    /**
     * @brief Canonical motif key encoding both structure and canonical colors.
     *
     * @param structure_id Canonical motif structure ID.
     * @param c0 Color packed into bits 0–23.
     * @param c1 Color packed into bits 24–47.
     * @param c2 Color packed into bits 48–71.
     * @param c3 Color packed into bits 72–95.
     * @return 128-bit key with structure in the high 32 bits and packed colors in the low 96 bits.
     */
    static __uint128_t colored_key(const uint32_t structure_id, const uint32_t c0,
                                   const uint32_t c1, const uint32_t c2, const uint32_t c3)
    {
        const __uint128_t color_part =
            static_cast<__uint128_t>(c0) |
            (static_cast<__uint128_t>(c1) << SgfConstants::BITS_PER_COLOR) |
            (static_cast<__uint128_t>(c2) << (2U * SgfConstants::BITS_PER_COLOR)) |
            (static_cast<__uint128_t>(c3) << (3U * SgfConstants::BITS_PER_COLOR));
        return zero_color_key(structure_id) | color_part;
    }
};

// ── Group 1: Edge cases — fewer than 4 vertices ───────────────────────────────

TEST_F(MotifPreprocessorTest, empty_graph_returns_empty_map)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    const std::vector<uint32_t> colors;
    const ColoredGraph graph(0U, edges, colors, false);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<__uint128_t, uint32_t> result = preprocessor.calculate();

    EXPECT_TRUE(result.empty());
}

TEST_F(MotifPreprocessorTest, single_vertex_no_edges_returns_empty_map)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    const std::vector<uint32_t> colors = {0U};
    const ColoredGraph graph(1U, edges, colors, false);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<__uint128_t, uint32_t> result = preprocessor.calculate();

    EXPECT_TRUE(result.empty());
}

TEST_F(MotifPreprocessorTest, two_vertices_one_edge_returns_empty_map)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}};
    const std::vector<uint32_t> colors = {0U, 0U};
    const ColoredGraph graph(2U, edges, colors, false);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<__uint128_t, uint32_t> result = preprocessor.calculate();

    EXPECT_TRUE(result.empty());
}

TEST_F(MotifPreprocessorTest, three_vertices_triangle_returns_empty_map)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {0U, 2U}, {1U, 2U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U};
    const ColoredGraph graph(3U, edges, colors, false);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<__uint128_t, uint32_t> result = preprocessor.calculate();

    EXPECT_TRUE(result.empty());
}

// ── Group 2: All 6 motif types, 4-vertex graphs, all-zero colors ──────────────

TEST_F(MotifPreprocessorTest, four_vertex_star_k13_all_zero_colors)
{
    // K1,3: center=0, leaves=1,2,3. Canonical structure ID = 11.
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {0U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, false);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<__uint128_t, uint32_t> result = preprocessor.calculate();

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(11U)), 1U);
}

TEST_F(MotifPreprocessorTest, four_vertex_path_p4_all_zero_colors)
{
    // P4: linear path 0-1-2-3. Canonical structure ID = 13.
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {1U, 2U}, {2U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, false);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<__uint128_t, uint32_t> result = preprocessor.calculate();

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(13U)), 1U);
}

TEST_F(MotifPreprocessorTest, four_vertex_paw_all_zero_colors)
{
    // Paw (K3+pendant): triangle {0,1,2} with pendant 3-0. Canonical structure ID = 15.
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {0U, 2U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, false);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<__uint128_t, uint32_t> result = preprocessor.calculate();

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(15U)), 1U);
}

TEST_F(MotifPreprocessorTest, four_vertex_cycle_c4_all_zero_colors)
{
    // C4: 4-cycle 0-1-2-3-0. Canonical structure ID = 30.
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {1U, 2U}, {2U, 3U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, false);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<__uint128_t, uint32_t> result = preprocessor.calculate();

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(30U)), 1U);
}

TEST_F(MotifPreprocessorTest, four_vertex_diamond_all_zero_colors)
{
    // Diamond (K4 minus edge 0-1): Canonical structure ID = 31.
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {0U, 2U}, {0U, 3U}, {1U, 2U}, {1U, 3U}, {2U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, false);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<__uint128_t, uint32_t> result = preprocessor.calculate();

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(31U)), 1U);
}

TEST_F(MotifPreprocessorTest, four_vertex_complete_k4_all_zero_colors)
{
    // K4: all 6 edges. Canonical structure ID = 63.
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {0U, 2U}, {0U, 3U},
                                                        {1U, 2U}, {1U, 3U}, {2U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, false);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<__uint128_t, uint32_t> result = preprocessor.calculate();

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(63U)), 1U);
}

// ── Group 3: 5-vertex graphs, all-zero colors ─────────────────────────────────

TEST_F(MotifPreprocessorTest, five_vertex_star_k14_all_zero_colors)
{
    // K1,4: center=0, leaves=1,2,3,4. Each 3-leaf subset induces K1,3. Count = C(4,3) = 4.
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {0U, 2U}, {0U, 3U}, {0U, 4U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U, 0U};
    const ColoredGraph graph(5U, edges, colors, false);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<__uint128_t, uint32_t> result = preprocessor.calculate();

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(11U)), 4U);
}

TEST_F(MotifPreprocessorTest, five_vertex_path_p5_all_zero_colors)
{
    // P5: 0-1-2-3-4. Exactly 2 consecutive 4-vertex sub-paths: {0,1,2,3} and {1,2,3,4}.
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {1U, 2U}, {2U, 3U}, {3U, 4U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U, 0U};
    const ColoredGraph graph(5U, edges, colors, false);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<__uint128_t, uint32_t> result = preprocessor.calculate();

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(13U)), 2U);
}

TEST_F(MotifPreprocessorTest, five_vertex_complete_k5_all_zero_colors)
{
    // K5: all 10 edges. Every 4-vertex subset induces K4. Count = C(5,4) = 5.
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {0U, 2U}, {0U, 3U}, {0U, 4U},
                                                        {1U, 2U}, {1U, 3U}, {1U, 4U}, {2U, 3U},
                                                        {2U, 4U}, {3U, 4U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U, 0U};
    const ColoredGraph graph(5U, edges, colors, false);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<__uint128_t, uint32_t> result = preprocessor.calculate();

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(63U)), 5U);
}

TEST_F(MotifPreprocessorTest, five_vertex_cycle_c5_all_zero_colors)
{
    // C5: 0-1-2-3-4-0. Every 4-vertex induced subgraph is a P4. Count = 5.
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {0U, 1U}, {1U, 2U}, {2U, 3U}, {3U, 4U}, {4U, 0U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U, 0U};
    const ColoredGraph graph(5U, edges, colors, false);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<__uint128_t, uint32_t> result = preprocessor.calculate();

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(13U)), 5U);
}

TEST_F(MotifPreprocessorTest, five_vertex_k4_plus_pendant_all_zero_colors)
{
    // K4 on {0,1,2,3} + pendant edge 0-4.
    // {0,1,2,3} → K4 (canonical 63).
    // {0,1,2,4}, {0,1,3,4}, {0,2,3,4} → paw each (canonical 15).
    // {1,2,3,4} → unreachable by Kavosh (4 only connects to 0 which is processed first).
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {0U, 2U}, {0U, 3U}, {1U, 2U},
                                                        {1U, 3U}, {2U, 3U}, {0U, 4U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U, 0U};
    const ColoredGraph graph(5U, edges, colors, false);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<__uint128_t, uint32_t> result = preprocessor.calculate();

    ASSERT_EQ(result.size(), 2U);
    EXPECT_EQ(result.at(zero_color_key(63U)), 1U);
    EXPECT_EQ(result.at(zero_color_key(15U)), 3U);
}

// ── Group 4: Colored graphs ───────────────────────────────────────────────────
//
// K4 automorphism group = all 24 permutations of 4 vertices. The canonical
// color encoding is therefore the ascending sort of the 4 colors.
//
// K1,3 automorphisms permute only the 3 leaves. The center is fixed in the
// canonical encoding. The minimum is computed over the 6 leaf permutations.
//
// P4 automorphisms = identity + reflection. Minimum of forward vs reversed
// encoding is used.

TEST_F(MotifPreprocessorTest, colored_k4_key_encodes_color_not_just_structure)
{
    // K4 with one vertex colored 1 and the rest 0.
    // Canonical (sorted ascending): {0, 0, 0, 1}.
    // Expected key has color info; must NOT equal the zero-color key.
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {0U, 2U}, {0U, 3U},
                                                        {1U, 2U}, {1U, 3U}, {2U, 3U}};
    const std::vector<uint32_t> colors = {1U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, false);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<__uint128_t, uint32_t> result = preprocessor.calculate();

    const __uint128_t expected_key = colored_key(63U, 0U, 0U, 0U, 1U);
    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(expected_key), 1U);
    EXPECT_EQ(result.count(zero_color_key(63U)), 0U);
}

TEST_F(MotifPreprocessorTest, colored_k4_automorphic_assignment_same_key)
{
    // K4 has 24 automorphisms — any permutation of vertex colors gives the same canonical key.
    // Graph A: colors {1,2,3,4}. Graph B: colors {2,1,3,4} (v0 and v1 swapped).
    // Both should canonicalize to sorted {1,2,3,4}.
    std::vector<std::pair<uint32_t, uint32_t>> edges_a = {{0U, 1U}, {0U, 2U}, {0U, 3U},
                                                          {1U, 2U}, {1U, 3U}, {2U, 3U}};
    const std::vector<uint32_t> colors_a = {1U, 2U, 3U, 4U};
    const ColoredGraph graph_a(4U, edges_a, colors_a, false);
    MotifPreprocessor preprocessor_a(graph_a, null_logger());

    std::vector<std::pair<uint32_t, uint32_t>> edges_b = {{0U, 1U}, {0U, 2U}, {0U, 3U},
                                                          {1U, 2U}, {1U, 3U}, {2U, 3U}};
    const std::vector<uint32_t> colors_b = {2U, 1U, 3U, 4U};
    const ColoredGraph graph_b(4U, edges_b, colors_b, false);
    MotifPreprocessor preprocessor_b(graph_b, null_logger());

    const std::unordered_map<__uint128_t, uint32_t> result_a = preprocessor_a.calculate();
    const std::unordered_map<__uint128_t, uint32_t> result_b = preprocessor_b.calculate();

    const __uint128_t expected_key = colored_key(63U, 1U, 2U, 3U, 4U);
    ASSERT_EQ(result_a.size(), 1U);
    ASSERT_EQ(result_b.size(), 1U);
    EXPECT_EQ(result_a.at(expected_key), 1U);
    EXPECT_EQ(result_b.at(expected_key), 1U);
}

TEST_F(MotifPreprocessorTest, colored_k13_center_vs_leaf_distinct_keys)
{
    // K1,3 center=v0=5, leaves=0. Group found as [center, l1, l2, l3].
    // Permutations for entry 11: {0,1,2,3},{0,2,1,3},{1,0,2,3},{1,2,0,3},{2,0,1,3},{2,1,0,3}.
    // With node_colors=[5,0,0,0]:
    //   Perm {0,1,2,3}: 5+0+0+0 = 5  ← minimum (leaves at pos 1-3 give 5<<24 which is larger)
    // Key_A = colored_key(11, 5, 0, 0, 0).
    //
    // K1,3 center=v0=0, leaves=5. node_colors=[0,5,5,5].
    //   Perm {0,1,2,3}: 0+5<<24+5<<48+5<<72  ← minimum (starting with 0 in pos 0 is minimal)
    // Key_B = colored_key(11, 0, 5, 5, 5).
    std::vector<std::pair<uint32_t, uint32_t>> edges_a = {{0U, 1U}, {0U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors_a = {5U, 0U, 0U, 0U};
    const ColoredGraph graph_a(4U, edges_a, colors_a, false);
    MotifPreprocessor preprocessor_a(graph_a, null_logger());

    std::vector<std::pair<uint32_t, uint32_t>> edges_b = {{0U, 1U}, {0U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors_b = {0U, 5U, 5U, 5U};
    const ColoredGraph graph_b(4U, edges_b, colors_b, false);
    MotifPreprocessor preprocessor_b(graph_b, null_logger());

    const std::unordered_map<__uint128_t, uint32_t> result_a = preprocessor_a.calculate();
    const std::unordered_map<__uint128_t, uint32_t> result_b = preprocessor_b.calculate();

    const __uint128_t key_a = colored_key(11U, 5U, 0U, 0U, 0U);
    const __uint128_t key_b = colored_key(11U, 0U, 5U, 5U, 5U);
    ASSERT_EQ(result_a.size(), 1U);
    ASSERT_EQ(result_b.size(), 1U);
    EXPECT_EQ(result_a.at(key_a), 1U);
    EXPECT_EQ(result_b.at(key_b), 1U);
    EXPECT_NE(key_a, key_b);
}

TEST_F(MotifPreprocessorTest, colored_p4_reflection_automorphism_same_key)
{
    // P4's only non-trivial automorphism is reversal. Colors {1,2,3,4} and {4,3,2,1}
    // are related by this automorphism and must canonicalize to the same key.
    //
    // The Kavosh enumeration finds group [root,n1,n11,n2] with raw descriptor 50
    // (entry in UNDIRECTED_MOTIF_CANONICAL_MAP → canonical 13, perms {2,3,1,0},{3,2,0,1}).
    //
    // For vertex_colors={1,2,3,4}, node_colors of the found group = [2,3,1,4]:
    //   Perm {2,3,1,0}: nc[2]+nc[3]<<24+nc[1]<<48+nc[0]<<72 = 1+4<<24+3<<48+2<<72
    //   Perm {3,2,0,1}: nc[3]+nc[2]<<24+nc[0]<<48+nc[1]<<72 = 4+1<<24+2<<48+3<<72
    //   Min = 1+4<<24+3<<48+2<<72  → colored_key(13, 1, 4, 3, 2)
    //
    // For vertex_colors={4,3,2,1}, node_colors = [3,2,4,1], the same two permutations
    // swap their computed values and yield the same minimum → colored_key(13, 1, 4, 3, 2).
    std::vector<std::pair<uint32_t, uint32_t>> edges_a = {{0U, 1U}, {1U, 2U}, {2U, 3U}};
    const std::vector<uint32_t> colors_a = {1U, 2U, 3U, 4U};
    const ColoredGraph graph_a(4U, edges_a, colors_a, false);
    MotifPreprocessor preprocessor_a(graph_a, null_logger());

    std::vector<std::pair<uint32_t, uint32_t>> edges_b = {{0U, 1U}, {1U, 2U}, {2U, 3U}};
    const std::vector<uint32_t> colors_b = {4U, 3U, 2U, 1U};
    const ColoredGraph graph_b(4U, edges_b, colors_b, false);
    MotifPreprocessor preprocessor_b(graph_b, null_logger());

    const std::unordered_map<__uint128_t, uint32_t> result_a = preprocessor_a.calculate();
    const std::unordered_map<__uint128_t, uint32_t> result_b = preprocessor_b.calculate();

    const __uint128_t expected_key = colored_key(13U, 1U, 4U, 3U, 2U);
    ASSERT_EQ(result_a.size(), 1U);
    ASSERT_EQ(result_b.size(), 1U);
    EXPECT_EQ(result_a.at(expected_key), 1U);
    EXPECT_EQ(result_b.at(expected_key), 1U);
}

TEST_F(MotifPreprocessorTest, colored_p4_non_equivalent_colors_different_keys)
{
    // Colors {1,2,3,4} and {1,2,3,5} are NOT related by any P4 automorphism.
    // They must produce different canonical keys.
    //
    // vertex_colors={1,2,3,4} → colored_key(13, 1, 4, 3, 2)  (derived above)
    // vertex_colors={1,2,3,5}: node_colors of found group = [2,3,1,5]:
    //   Perm {2,3,1,0}: 1+5<<24+3<<48+2<<72
    //   Perm {3,2,0,1}: 5+1<<24+2<<48+3<<72
    //   Min = 1+5<<24+3<<48+2<<72  → colored_key(13, 1, 5, 3, 2)
    std::vector<std::pair<uint32_t, uint32_t>> edges_a = {{0U, 1U}, {1U, 2U}, {2U, 3U}};
    const std::vector<uint32_t> colors_a = {1U, 2U, 3U, 4U};
    const ColoredGraph graph_a(4U, edges_a, colors_a, false);
    MotifPreprocessor preprocessor_a(graph_a, null_logger());

    std::vector<std::pair<uint32_t, uint32_t>> edges_b = {{0U, 1U}, {1U, 2U}, {2U, 3U}};
    const std::vector<uint32_t> colors_b = {1U, 2U, 3U, 5U};
    const ColoredGraph graph_b(4U, edges_b, colors_b, false);
    MotifPreprocessor preprocessor_b(graph_b, null_logger());

    const std::unordered_map<__uint128_t, uint32_t> result_a = preprocessor_a.calculate();
    const std::unordered_map<__uint128_t, uint32_t> result_b = preprocessor_b.calculate();

    const __uint128_t key_a = colored_key(13U, 1U, 4U, 3U, 2U);
    const __uint128_t key_b = colored_key(13U, 1U, 5U, 3U, 2U);
    ASSERT_EQ(result_a.size(), 1U);
    ASSERT_EQ(result_b.size(), 1U);
    EXPECT_EQ(result_a.at(key_a), 1U);
    EXPECT_EQ(result_b.at(key_b), 1U);
    EXPECT_NE(key_a, key_b);
}

}  // namespace
