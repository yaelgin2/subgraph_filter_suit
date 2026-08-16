#ifdef SGF_CUDA_ENABLED
#include "MotifPreprocessor.h"

#include "ColoredGraph.h"
#include "Constants.h"
#include "FileLogger.h"
#include "ILogger.h"
#include "LoggerHandler.h"

#include <cstdint>
#include <cuda_runtime.h>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using namespace sgf;

namespace
{

class GpuMotifPreprocessorTest : public ::testing::Test
{
protected:
    /**
     * @brief Skip test if no CUDA-capable GPU is present on this machine.
     */
    void SetUp() override
    {
        int32_t device_count = 0;
        if (cudaGetDeviceCount(&device_count) != cudaSuccess || device_count == 0)
        {
            GTEST_SKIP() << "No CUDA-capable GPU found";
        }
    }

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
    static UInt128 zero_color_key(const uint32_t structure_id)
    {
        return UInt128{static_cast<uint64_t>(structure_id)}
               << static_cast<uint32_t>(SgfConstants::MOTIF_SIZE * SgfConstants::BITS_PER_COLOR);
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
    static UInt128 colored_key(const uint32_t structure_id, const uint32_t c0, const uint32_t c1,
                               const uint32_t c2, const uint32_t c3)
    {
        const UInt128 color_part = UInt128{static_cast<uint64_t>(c0)} |
                                   (UInt128{static_cast<uint64_t>(c1)}
                                    << static_cast<uint32_t>(SgfConstants::BITS_PER_COLOR)) |
                                   (UInt128{static_cast<uint64_t>(c2)}
                                    << static_cast<uint32_t>(2U * SgfConstants::BITS_PER_COLOR)) |
                                   (UInt128{static_cast<uint64_t>(c3)}
                                    << static_cast<uint32_t>(3U * SgfConstants::BITS_PER_COLOR));
        return zero_color_key(structure_id) | color_part;
    }
};

// ── Group 1: Edge cases — fewer than 4 vertices ───────────────────────────────

TEST_F(GpuMotifPreprocessorTest, empty_graph_returns_empty_map)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    const std::vector<uint32_t> colors;
    const ColoredGraph graph(0U, edges, colors, false);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    EXPECT_TRUE(result.empty());
}

TEST_F(GpuMotifPreprocessorTest, single_vertex_no_edges_returns_empty_map)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    const std::vector<uint32_t> colors = {0U};
    const ColoredGraph graph(1U, edges, colors, false);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    EXPECT_TRUE(result.empty());
}

TEST_F(GpuMotifPreprocessorTest, two_vertices_one_edge_returns_empty_map)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}};
    const std::vector<uint32_t> colors = {0U, 0U};
    const ColoredGraph graph(2U, edges, colors, false);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    EXPECT_TRUE(result.empty());
}

TEST_F(GpuMotifPreprocessorTest, three_vertices_triangle_returns_empty_map)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {0U, 2U}, {1U, 2U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U};
    const ColoredGraph graph(3U, edges, colors, false);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    EXPECT_TRUE(result.empty());
}

TEST_F(GpuMotifPreprocessorTest, five_vertices_no_edges_returns_empty_map)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U, 0U};
    const ColoredGraph graph(5U, edges, colors, false);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    EXPECT_TRUE(result.empty());
}

TEST_F(GpuMotifPreprocessorTest, six_vertices_one_edge_returns_empty_map)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U, 0U, 0U};
    const ColoredGraph graph(6U, edges, colors, false);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    EXPECT_TRUE(result.empty());
}

// ── Group 2: All 6 motif types, 4-vertex graphs, all-zero colors ──────────────

TEST_F(GpuMotifPreprocessorTest, four_vertex_star_k13_all_zero_colors)
{
    // K1,3: center=0, leaves=1,2,3. Canonical structure ID = 11.
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {0U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, false);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(11U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, four_vertex_path_p4_all_zero_colors)
{
    // P4: linear path 0-1-2-3. Canonical structure ID = 13.
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {1U, 2U}, {2U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, false);

    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(13U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, four_vertex_paw_all_zero_colors)
{
    // Paw (K3+pendant): triangle {0,1,2} with pendant 3-0. Canonical structure ID = 15.
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {0U, 2U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, false);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(15U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, four_vertex_cycle_c4_all_zero_colors)
{
    // C4: 4-cycle 0-1-2-3-0. Canonical structure ID = 30.
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {1U, 2U}, {2U, 3U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, false);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(30U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, four_vertex_diamond_all_zero_colors)
{
    // Diamond (K4 minus edge 0-1): Canonical structure ID = 31.
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {0U, 2U}, {0U, 3U}, {1U, 2U}, {1U, 3U}, {2U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, false);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(31U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, four_vertex_complete_k4_all_zero_colors)
{
    // K4: all 6 edges. Canonical structure ID = 63.
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {0U, 2U}, {0U, 3U},
                                                        {1U, 2U}, {1U, 3U}, {2U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, false);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(63U)), 1U);
}

// ── Group 3: 5-vertex graphs, all-zero colors ─────────────────────────────────

TEST_F(GpuMotifPreprocessorTest, five_vertex_star_k14_all_zero_colors)
{
    // K1,4: center=0, leaves=1,2,3,4. Each 3-leaf subset induces K1,3. Count = C(4,3) = 4.
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {0U, 2U}, {0U, 3U}, {0U, 4U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U, 0U};
    const ColoredGraph graph(5U, edges, colors, false);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(11U)), 4U);
}

TEST_F(GpuMotifPreprocessorTest, five_vertex_path_p5_all_zero_colors)
{
    // P5: 0-1-2-3-4. Exactly 2 consecutive 4-vertex sub-paths: {0,1,2,3} and {1,2,3,4}.
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {1U, 2U}, {2U, 3U}, {3U, 4U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U, 0U};
    const ColoredGraph graph(5U, edges, colors, false);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(13U)), 2U);
}

TEST_F(GpuMotifPreprocessorTest, five_vertex_complete_k5_all_zero_colors)
{
    // K5: all 10 edges. Every 4-vertex subset induces K4. Count = C(5,4) = 5.
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {0U, 2U}, {0U, 3U}, {0U, 4U},
                                                        {1U, 2U}, {1U, 3U}, {1U, 4U}, {2U, 3U},
                                                        {2U, 4U}, {3U, 4U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U, 0U};
    const ColoredGraph graph(5U, edges, colors, false);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(63U)), 5U);
}

TEST_F(GpuMotifPreprocessorTest, five_vertex_cycle_c5_all_zero_colors)
{
    // C5: 0-1-2-3-4-0. Every 4-vertex induced subgraph is a P4. Count = 5.
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {0U, 1U}, {1U, 2U}, {2U, 3U}, {3U, 4U}, {4U, 0U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U, 0U};
    const ColoredGraph graph(5U, edges, colors, false);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(13U)), 5U);
}

TEST_F(GpuMotifPreprocessorTest, five_vertex_k4_plus_pendant_all_zero_colors)
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

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

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

TEST_F(GpuMotifPreprocessorTest, colored_k4_key_encodes_color_not_just_structure)
{
    // K4 with one vertex colored 1 and the rest 0.
    // Canonical minimum packs the unique color 1 into the least-significant slot (c0),
    // giving colored_key(63, 1, 0, 0, 0). Must NOT equal the zero-color key.
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {0U, 2U}, {0U, 3U},
                                                        {1U, 2U}, {1U, 3U}, {2U, 3U}};
    const std::vector<uint32_t> colors = {1U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, false);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    const UInt128 expected_key = colored_key(63U, 1U, 0U, 0U, 0U);
    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(expected_key), 1U);
    EXPECT_EQ(result.count(zero_color_key(63U)), 0U);
}

TEST_F(GpuMotifPreprocessorTest, colored_k4_automorphic_assignment_same_key)
{
    // K4 has 24 automorphisms — any permutation of vertex colors gives the same canonical key.
    // Graph A: colors {1,2,3,4}. Graph B: colors {2,1,3,4} (v0 and v1 swapped).
    // Canonical minimum packs in descending order into slots (c0 largest, c3 smallest),
    // so {1,2,3,4} → colored_key(63, 4, 3, 2, 1).
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

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result_a = preprocessor_a.calculate();
    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result_b = preprocessor_b.calculate();

    const UInt128 expected_key = colored_key(63U, 4U, 3U, 2U, 1U);
    ASSERT_EQ(result_a.size(), 1U);
    ASSERT_EQ(result_b.size(), 1U);
    EXPECT_EQ(result_a.at(expected_key), 1U);
    EXPECT_EQ(result_b.at(expected_key), 1U);
}

TEST_F(GpuMotifPreprocessorTest, colored_k13_center_vs_leaf_distinct_keys)
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

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result_a = preprocessor_a.calculate();
    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result_b = preprocessor_b.calculate();

    const UInt128 key_a = colored_key(11U, 0U, 0U, 0U, 5U);
    const UInt128 key_b = colored_key(11U, 5U, 5U, 5U, 0U);

    ASSERT_EQ(result_a.size(), 1U);
    ASSERT_EQ(result_b.size(), 1U);
    EXPECT_TRUE(result_a.find(key_a) != result_a.end());
    EXPECT_TRUE(result_b.find(key_b) != result_b.end());
    EXPECT_EQ(result_a.at(key_a), 1U);
    EXPECT_EQ(result_b.at(key_b), 1U);
    EXPECT_NE(key_a, key_b);
}

TEST_F(GpuMotifPreprocessorTest, colored_p4_reflection_automorphism_same_key)
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

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result_a = preprocessor_a.calculate();
    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result_b = preprocessor_b.calculate();

    const UInt128 expected_key = colored_key(13U, 1U, 4U, 3U, 2U);
    ASSERT_EQ(result_a.size(), 1U);
    ASSERT_EQ(result_b.size(), 1U);
    EXPECT_EQ(result_a.at(expected_key), 1U);
    EXPECT_EQ(result_b.at(expected_key), 1U);
}

TEST_F(GpuMotifPreprocessorTest, colored_p4_non_equivalent_colors_different_keys)
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

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result_a = preprocessor_a.calculate();
    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result_b = preprocessor_b.calculate();

    const UInt128 key_a = colored_key(13U, 1U, 4U, 3U, 2U);
    const UInt128 key_b = colored_key(13U, 1U, 5U, 3U, 2U);
    ASSERT_EQ(result_a.size(), 1U);
    ASSERT_EQ(result_b.size(), 1U);
    EXPECT_EQ(result_a.at(key_a), 1U);
    EXPECT_EQ(result_b.at(key_b), 1U);
    EXPECT_NE(key_a, key_b);
}

// ── Group 5: Directed graphs — empty-map (fewer than 4 vertices) ────────────────

TEST_F(GpuMotifPreprocessorTest, directed_single_vertex_returns_empty_map)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    const std::vector<uint32_t> colors = {0U};
    const ColoredGraph graph(1U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    EXPECT_TRUE(result.empty());
}

TEST_F(GpuMotifPreprocessorTest, directed_two_vertices_one_edge_returns_empty_map)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}};
    const std::vector<uint32_t> colors = {0U, 0U};
    const ColoredGraph graph(2U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    EXPECT_TRUE(result.empty());
}

TEST_F(GpuMotifPreprocessorTest, directed_three_vertices_cycle_returns_empty_map)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {1U, 2U}, {2U, 0U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U};
    const ColoredGraph graph(3U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    EXPECT_TRUE(result.empty());
}

// ── Group 6: All 185 findable 4-vertex directed motifs, all-zero colors ──────────

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_7_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(7U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_14_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 1U}, {3U, 0U}, {2U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(14U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_15_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(15U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_21_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 0U}, {2U, 1U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(21U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_23_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 1U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(23U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_29_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 0U}, {2U, 3U}, {2U, 1U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(29U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_30_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 1U}, {3U, 0U}, {2U, 3U}, {2U, 1U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(30U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_31_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 3U}, {2U, 1U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(31U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_55_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 1U}, {2U, 0U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(55U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_63_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U},
                                                        {2U, 3U}, {2U, 1U}, {2U, 0U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(63U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_77_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 0U}, {2U, 3U}, {1U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(77U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_79_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 3U}, {1U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(79U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_84_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 0U}, {2U, 1U}, {1U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(84U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_85_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 0U}, {2U, 1U}, {1U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(85U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_86_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 1U}, {3U, 0U}, {2U, 1U}, {1U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(86U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_87_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 1U}, {1U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(87U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_92_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 0U}, {2U, 3U}, {2U, 1U}, {1U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(92U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_93_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 2U}, {3U, 0U}, {2U, 3U}, {2U, 1U}, {1U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(93U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_94_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 1U}, {3U, 0U}, {2U, 3U}, {2U, 1U}, {1U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(94U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_95_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U},
                                                        {2U, 3U}, {2U, 1U}, {1U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(95U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_99_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {2U, 0U}, {1U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(99U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_101_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 0U}, {2U, 0U}, {1U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(101U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_103_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 0U}, {1U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(103U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_106_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 1U}, {2U, 3U}, {2U, 0U}, {1U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(106U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_107_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 2U}, {3U, 1U}, {2U, 3U}, {2U, 0U}, {1U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(107U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_109_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 2U}, {3U, 0U}, {2U, 3U}, {2U, 0U}, {1U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(109U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_110_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 1U}, {3U, 0U}, {2U, 3U}, {2U, 0U}, {1U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(110U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_111_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U},
                                                        {2U, 3U}, {2U, 0U}, {1U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(111U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_115_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 2U}, {3U, 1U}, {2U, 1U}, {2U, 0U}, {1U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(115U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_116_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 0U}, {2U, 1U}, {2U, 0U}, {1U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(116U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_117_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 2U}, {3U, 0U}, {2U, 1U}, {2U, 0U}, {1U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(117U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_118_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 1U}, {3U, 0U}, {2U, 1U}, {2U, 0U}, {1U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(118U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_119_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U},
                                                        {2U, 1U}, {2U, 0U}, {1U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(119U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_122_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 1U}, {2U, 3U}, {2U, 1U}, {2U, 0U}, {1U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(122U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_123_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {2U, 3U},
                                                        {2U, 1U}, {2U, 0U}, {1U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(123U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_124_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 0U}, {2U, 3U}, {2U, 1U}, {2U, 0U}, {1U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(124U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_125_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 0U}, {2U, 3U},
                                                        {2U, 1U}, {2U, 0U}, {1U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(125U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_126_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 1U}, {3U, 0U}, {2U, 3U},
                                                        {2U, 1U}, {2U, 0U}, {1U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(126U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_127_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 3U},
                                                        {2U, 1U}, {2U, 0U}, {1U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(127U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_220_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 0U}, {2U, 3U}, {2U, 1U}, {1U, 3U}, {1U, 2U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(220U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_221_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 0U}, {2U, 3U},
                                                        {2U, 1U}, {1U, 3U}, {1U, 2U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(221U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_223_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 3U},
                                                        {2U, 1U}, {1U, 3U}, {1U, 2U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(223U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_228_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 0U}, {2U, 0U}, {1U, 3U}, {1U, 2U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(228U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_229_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 2U}, {3U, 0U}, {2U, 0U}, {1U, 3U}, {1U, 2U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(229U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_230_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 1U}, {3U, 0U}, {2U, 0U}, {1U, 3U}, {1U, 2U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(230U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_231_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U},
                                                        {2U, 0U}, {1U, 3U}, {1U, 2U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(231U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_237_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 0U}, {2U, 3U},
                                                        {2U, 0U}, {1U, 3U}, {1U, 2U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(237U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_238_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 1U}, {3U, 0U}, {2U, 3U},
                                                        {2U, 0U}, {1U, 3U}, {1U, 2U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(238U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_239_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 3U},
                                                        {2U, 0U}, {1U, 3U}, {1U, 2U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(239U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_246_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 1U}, {3U, 0U}, {2U, 1U},
                                                        {2U, 0U}, {1U, 3U}, {1U, 2U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(246U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_247_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 1U},
                                                        {2U, 0U}, {1U, 3U}, {1U, 2U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(247U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_255_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 3U},
                                                        {2U, 1U}, {2U, 0U}, {1U, 3U}, {1U, 2U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(255U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_295_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 0U}, {1U, 0U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(295U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_302_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 1U}, {3U, 0U}, {2U, 3U}, {2U, 0U}, {1U, 0U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(302U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_303_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U},
                                                        {2U, 3U}, {2U, 0U}, {1U, 0U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(303U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_311_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U},
                                                        {2U, 1U}, {2U, 0U}, {1U, 0U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(311U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_319_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 3U},
                                                        {2U, 1U}, {2U, 0U}, {1U, 0U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(319U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_365_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 0U}, {2U, 3U},
                                                        {2U, 0U}, {1U, 3U}, {1U, 0U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(365U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_367_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 3U},
                                                        {2U, 0U}, {1U, 3U}, {1U, 0U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(367U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_373_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 0U}, {2U, 1U},
                                                        {2U, 0U}, {1U, 3U}, {1U, 0U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(373U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_375_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 1U},
                                                        {2U, 0U}, {1U, 3U}, {1U, 0U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(375U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_382_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 1U}, {3U, 0U}, {2U, 3U}, {2U, 1U},
                                                        {2U, 0U}, {1U, 3U}, {1U, 0U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(382U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_383_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 3U},
                                                        {2U, 1U}, {2U, 0U}, {1U, 3U}, {1U, 0U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(383U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_511_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 3U}, {2U, 1U}, {2U, 0U}, {1U, 3U}, {1U, 2U}, {1U, 0U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(511U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_587_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 2U}, {3U, 1U}, {2U, 3U}, {1U, 3U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(587U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_591_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U},
                                                        {2U, 3U}, {1U, 3U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(591U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_593_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {2U, 1U}, {1U, 3U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(593U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_595_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 2U}, {3U, 1U}, {2U, 1U}, {1U, 3U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(595U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_596_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 0U}, {2U, 1U}, {1U, 3U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(596U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_597_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 2U}, {3U, 0U}, {2U, 1U}, {1U, 3U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(597U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_598_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 1U}, {3U, 0U}, {2U, 1U}, {1U, 3U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(598U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_599_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U},
                                                        {2U, 1U}, {1U, 3U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(599U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_601_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 2U}, {2U, 3U}, {2U, 1U}, {1U, 3U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(601U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_603_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {2U, 3U},
                                                        {2U, 1U}, {1U, 3U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(603U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_604_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 0U}, {2U, 3U}, {2U, 1U}, {1U, 3U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(604U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_605_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 0U}, {2U, 3U},
                                                        {2U, 1U}, {1U, 3U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(605U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_606_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 1U}, {3U, 0U}, {2U, 3U},
                                                        {2U, 1U}, {1U, 3U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(606U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_607_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 3U},
                                                        {2U, 1U}, {1U, 3U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(607U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_625_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 2U}, {2U, 1U}, {2U, 0U}, {1U, 3U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(625U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_626_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 1U}, {2U, 1U}, {2U, 0U}, {1U, 3U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(626U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_627_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {2U, 1U},
                                                        {2U, 0U}, {1U, 3U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(627U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_630_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 1U}, {3U, 0U}, {2U, 1U},
                                                        {2U, 0U}, {1U, 3U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(630U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_631_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 1U},
                                                        {2U, 0U}, {1U, 3U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(631U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_633_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {2U, 3U}, {2U, 1U},
                                                        {2U, 0U}, {1U, 3U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(633U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_634_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 1U}, {2U, 3U}, {2U, 1U},
                                                        {2U, 0U}, {1U, 3U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(634U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_635_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {2U, 3U}, {2U, 1U},
                                                        {2U, 0U}, {1U, 3U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(635U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_638_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 1U}, {3U, 0U}, {2U, 3U}, {2U, 1U},
                                                        {2U, 0U}, {1U, 3U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(638U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_639_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 3U},
                                                        {2U, 1U}, {2U, 0U}, {1U, 3U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(639U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_659_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 2U}, {3U, 1U}, {2U, 1U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(659U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_661_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 2U}, {3U, 0U}, {2U, 1U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(661U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_663_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U},
                                                        {2U, 1U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(663U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_666_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 1U}, {2U, 3U}, {2U, 1U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(666U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_667_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {2U, 3U},
                                                        {2U, 1U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(667U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_669_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 0U}, {2U, 3U},
                                                        {2U, 1U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(669U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_670_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 1U}, {3U, 0U}, {2U, 3U},
                                                        {2U, 1U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(670U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_671_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 3U},
                                                        {2U, 1U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(671U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_674_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 1U}, {2U, 0U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(674U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_675_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 2U}, {3U, 1U}, {2U, 0U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(675U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_678_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 1U}, {3U, 0U}, {2U, 0U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(678U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_679_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U},
                                                        {2U, 0U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(679U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_683_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {2U, 3U},
                                                        {2U, 0U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(683U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_686_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 1U}, {3U, 0U}, {2U, 3U},
                                                        {2U, 0U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(686U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_687_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 3U},
                                                        {2U, 0U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(687U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_694_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 1U}, {3U, 0U}, {2U, 1U},
                                                        {2U, 0U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(694U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_695_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 1U},
                                                        {2U, 0U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(695U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_703_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 3U},
                                                        {2U, 1U}, {2U, 0U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(703U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_729_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {2U, 3U}, {2U, 1U},
                                                        {1U, 3U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(729U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_731_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {2U, 3U}, {2U, 1U},
                                                        {1U, 3U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(731U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_732_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 0U}, {2U, 3U}, {2U, 1U},
                                                        {1U, 3U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(732U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_733_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 0U}, {2U, 3U}, {2U, 1U},
                                                        {1U, 3U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(733U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_735_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 3U},
                                                        {2U, 1U}, {1U, 3U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(735U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_737_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 2U}, {2U, 0U}, {1U, 3U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(737U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_739_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {2U, 0U},
                                                        {1U, 3U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(739U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_741_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 0U}, {2U, 0U},
                                                        {1U, 3U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(741U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_742_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 1U}, {3U, 0U}, {2U, 0U},
                                                        {1U, 3U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(742U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_743_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 0U},
                                                        {1U, 3U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(743U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_745_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {2U, 3U}, {2U, 0U},
                                                        {1U, 3U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(745U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_746_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 1U}, {2U, 3U}, {2U, 0U},
                                                        {1U, 3U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(746U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_747_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {2U, 3U}, {2U, 0U},
                                                        {1U, 3U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(747U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_748_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 0U}, {2U, 3U}, {2U, 0U},
                                                        {1U, 3U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(748U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_749_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 0U}, {2U, 3U}, {2U, 0U},
                                                        {1U, 3U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(749U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_750_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 1U}, {3U, 0U}, {2U, 3U}, {2U, 0U},
                                                        {1U, 3U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(750U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_751_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 3U},
                                                        {2U, 0U}, {1U, 3U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(751U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_753_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {2U, 1U}, {2U, 0U},
                                                        {1U, 3U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(753U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_755_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {2U, 1U}, {2U, 0U},
                                                        {1U, 3U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(755U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_756_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 0U}, {2U, 1U}, {2U, 0U},
                                                        {1U, 3U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(756U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_757_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 0U}, {2U, 1U}, {2U, 0U},
                                                        {1U, 3U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(757U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_758_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 1U}, {3U, 0U}, {2U, 1U}, {2U, 0U},
                                                        {1U, 3U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(758U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_759_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 1U},
                                                        {2U, 0U}, {1U, 3U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(759U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_761_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {2U, 3U}, {2U, 1U}, {2U, 0U},
                                                        {1U, 3U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(761U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_762_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 1U}, {2U, 3U}, {2U, 1U}, {2U, 0U},
                                                        {1U, 3U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(762U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_763_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {2U, 3U}, {2U, 1U},
                                                        {2U, 0U}, {1U, 3U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(763U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_764_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 0U}, {2U, 3U}, {2U, 1U}, {2U, 0U},
                                                        {1U, 3U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(764U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_765_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 0U}, {2U, 3U}, {2U, 1U},
                                                        {2U, 0U}, {1U, 3U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(765U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_766_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 1U}, {3U, 0U}, {2U, 3U}, {2U, 1U},
                                                        {2U, 0U}, {1U, 3U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(766U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_767_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 3U}, {2U, 1U}, {2U, 0U}, {1U, 3U}, {1U, 2U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(767U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_819_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {2U, 1U},
                                                        {2U, 0U}, {1U, 0U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(819U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_822_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 1U}, {3U, 0U}, {2U, 1U},
                                                        {2U, 0U}, {1U, 0U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(822U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_823_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 1U},
                                                        {2U, 0U}, {1U, 0U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(823U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_826_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 1U}, {2U, 3U}, {2U, 1U},
                                                        {2U, 0U}, {1U, 0U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(826U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_827_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {2U, 3U}, {2U, 1U},
                                                        {2U, 0U}, {1U, 0U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(827U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_830_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 1U}, {3U, 0U}, {2U, 3U}, {2U, 1U},
                                                        {2U, 0U}, {1U, 0U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(830U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_831_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 3U},
                                                        {2U, 1U}, {2U, 0U}, {1U, 0U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(831U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_875_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {2U, 3U}, {2U, 0U},
                                                        {1U, 3U}, {1U, 0U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(875U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_877_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 0U}, {2U, 3U}, {2U, 0U},
                                                        {1U, 3U}, {1U, 0U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(877U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_879_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 3U},
                                                        {2U, 0U}, {1U, 3U}, {1U, 0U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(879U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_883_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {2U, 1U}, {2U, 0U},
                                                        {1U, 3U}, {1U, 0U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(883U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_885_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 0U}, {2U, 1U}, {2U, 0U},
                                                        {1U, 3U}, {1U, 0U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(885U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_886_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 1U}, {3U, 0U}, {2U, 1U}, {2U, 0U},
                                                        {1U, 3U}, {1U, 0U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(886U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_887_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 1U},
                                                        {2U, 0U}, {1U, 3U}, {1U, 0U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(887U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_891_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {2U, 3U}, {2U, 1U},
                                                        {2U, 0U}, {1U, 3U}, {1U, 0U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(891U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_892_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 0U}, {2U, 3U}, {2U, 1U}, {2U, 0U},
                                                        {1U, 3U}, {1U, 0U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(892U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_893_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 0U}, {2U, 3U}, {2U, 1U},
                                                        {2U, 0U}, {1U, 3U}, {1U, 0U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(893U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_894_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 1U}, {3U, 0U}, {2U, 3U}, {2U, 1U},
                                                        {2U, 0U}, {1U, 3U}, {1U, 0U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(894U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_895_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 3U}, {2U, 1U}, {2U, 0U}, {1U, 3U}, {1U, 0U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(895U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_947_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {2U, 1U}, {2U, 0U},
                                                        {1U, 2U}, {1U, 0U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(947U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_949_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 0U}, {2U, 1U}, {2U, 0U},
                                                        {1U, 2U}, {1U, 0U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(949U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_951_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 1U},
                                                        {2U, 0U}, {1U, 2U}, {1U, 0U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(951U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_955_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {2U, 3U}, {2U, 1U},
                                                        {2U, 0U}, {1U, 2U}, {1U, 0U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(955U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_957_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 0U}, {2U, 3U}, {2U, 1U},
                                                        {2U, 0U}, {1U, 2U}, {1U, 0U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(957U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_958_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 1U}, {3U, 0U}, {2U, 3U}, {2U, 1U},
                                                        {2U, 0U}, {1U, 2U}, {1U, 0U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(958U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_959_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 3U}, {2U, 1U}, {2U, 0U}, {1U, 2U}, {1U, 0U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(959U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_1019_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 2U}, {3U, 1U}, {2U, 3U}, {2U, 1U}, {2U, 0U}, {1U, 3U}, {1U, 2U}, {1U, 0U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(1019U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_1020_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 0U}, {2U, 3U}, {2U, 1U}, {2U, 0U},
                                                        {1U, 3U}, {1U, 2U}, {1U, 0U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(1020U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_1021_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 2U}, {3U, 0U}, {2U, 3U}, {2U, 1U}, {2U, 0U}, {1U, 3U}, {1U, 2U}, {1U, 0U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(1021U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_1023_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 3U},
                                                        {2U, 1U}, {2U, 0U}, {1U, 3U}, {1U, 2U},
                                                        {1U, 0U}, {0U, 3U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(1023U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_1755_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {2U, 3U}, {2U, 1U},
                                                        {1U, 3U}, {1U, 2U}, {0U, 3U}, {0U, 2U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(1755U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_1757_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 0U}, {2U, 3U}, {2U, 1U},
                                                        {1U, 3U}, {1U, 2U}, {0U, 3U}, {0U, 2U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(1757U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_1758_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 1U}, {3U, 0U}, {2U, 3U}, {2U, 1U},
                                                        {1U, 3U}, {1U, 2U}, {0U, 3U}, {0U, 2U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(1758U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_1759_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 3U}, {2U, 1U}, {1U, 3U}, {1U, 2U}, {0U, 3U}, {0U, 2U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(1759U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_1782_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 1U}, {3U, 0U}, {2U, 1U}, {2U, 0U},
                                                        {1U, 3U}, {1U, 2U}, {0U, 3U}, {0U, 2U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(1782U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_1783_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 1U}, {2U, 0U}, {1U, 3U}, {1U, 2U}, {0U, 3U}, {0U, 2U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(1783U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_1791_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 3U},
                                                        {2U, 1U}, {2U, 0U}, {1U, 3U}, {1U, 2U},
                                                        {0U, 3U}, {0U, 2U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(1791U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_1883_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {2U, 3U}, {2U, 1U},
                                                        {1U, 3U}, {1U, 0U}, {0U, 3U}, {0U, 2U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(1883U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_1887_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 3U}, {2U, 1U}, {1U, 3U}, {1U, 0U}, {0U, 3U}, {0U, 2U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(1887U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_1907_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {2U, 1U}, {2U, 0U},
                                                        {1U, 3U}, {1U, 0U}, {0U, 3U}, {0U, 2U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(1907U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_1911_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 1U}, {2U, 0U}, {1U, 3U}, {1U, 0U}, {0U, 3U}, {0U, 2U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(1911U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_1917_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 2U}, {3U, 0U}, {2U, 3U}, {2U, 1U}, {2U, 0U}, {1U, 3U}, {1U, 0U}, {0U, 3U}, {0U, 2U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(1917U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_1918_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 1U}, {3U, 0U}, {2U, 3U}, {2U, 1U}, {2U, 0U}, {1U, 3U}, {1U, 0U}, {0U, 3U}, {0U, 2U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(1918U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_1919_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 3U},
                                                        {2U, 1U}, {2U, 0U}, {1U, 3U}, {1U, 0U},
                                                        {0U, 3U}, {0U, 2U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(1919U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_2029_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {3U, 2U}, {3U, 0U}, {2U, 3U}, {2U, 0U}, {1U, 3U}, {1U, 2U}, {1U, 0U}, {0U, 3U}, {0U, 2U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(2029U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_2031_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 3U},
                                                        {2U, 0U}, {1U, 3U}, {1U, 2U}, {1U, 0U},
                                                        {0U, 3U}, {0U, 2U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(2031U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_2039_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 1U},
                                                        {2U, 0U}, {1U, 3U}, {1U, 2U}, {1U, 0U},
                                                        {0U, 3U}, {0U, 2U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(2039U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_2047_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 3U},
                                                        {2U, 1U}, {2U, 0U}, {1U, 3U}, {1U, 2U},
                                                        {1U, 0U}, {0U, 3U}, {0U, 2U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(2047U)), 1U);
}

TEST_F(GpuMotifPreprocessorTest, directed_four_vertex_motif_4095_all_zero_colors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{3U, 2U}, {3U, 1U}, {3U, 0U}, {2U, 3U},
                                                        {2U, 1U}, {2U, 0U}, {1U, 3U}, {1U, 2U},
                                                        {1U, 0U}, {0U, 3U}, {0U, 2U}, {0U, 1U}};
    const std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    const ColoredGraph graph(4U, edges, colors, true);
    MotifPreprocessor preprocessor(graph, null_logger());

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = preprocessor.calculate(true);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(zero_color_key(4095U)), 1U);
}
}  // namespace

#endif // SGF_CUDA_ENABLED
