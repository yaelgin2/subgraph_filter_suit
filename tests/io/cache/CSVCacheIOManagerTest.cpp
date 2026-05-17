#include "CSVCacheIOManager.h"

#include "CacheTestUtils.h"
#include "IGraphPreprocessor.h"
#include "Int128.h"

#include <cstdint>
#include <gtest/gtest.h>
#include <string>
#include <unordered_map>
#include <vector>

using namespace sgf;

// ── Empty vector ──────────────────────────────────────────────────────────────

TEST(CSVCacheIOManagerTest, empty_data_roundtrip)
{
    TempCacheFile temp{"empty_data", "csv"};
    const CSVCacheIOManager manager{temp.m_folder, temp.m_base_name};

    const EnumerationData data{};
    manager.write(data);

    const EnumerationData result = manager.read();
    EXPECT_TRUE(result.empty());
}

// ── One graph, empty map ──────────────────────────────────────────────────────
//
// CSV stores only (graph, motif, count) triples; a graph with no motifs writes
// no rows, so reading back yields an empty EnumerationData rather than a
// one-element vector containing an empty map.

TEST(CSVCacheIOManagerTest, single_graph_empty_map_reads_back_as_empty)
{
    TempCacheFile temp{"single_empty_map", "csv"};
    const CSVCacheIOManager manager{temp.m_folder, temp.m_base_name};

    const EnumerationData data{EnumerationResult{}};
    manager.write(data);

    const EnumerationData result = manager.read();
    EXPECT_TRUE(result.empty());
}

// ── Two graphs: first empty, second non-empty ─────────────────────────────────

TEST(CSVCacheIOManagerTest, two_graphs_first_empty_second_nonempty_roundtrip)
{
    TempCacheFile temp{"two_graphs_first_empty", "csv"};
    const CSVCacheIOManager manager{temp.m_folder, temp.m_base_name};

    const UInt128 key{0xDEADBEEF00000000ULL, 0x00000000CAFEBABEULL};
    const uint32_t value = 77U;

    const EnumerationData data{EnumerationResult{}, EnumerationResult{{key, value}}};
    manager.write(data);

    const EnumerationData result = manager.read();
    ASSERT_EQ(result.size(), 2U);
    EXPECT_TRUE(result[0].empty());
    ASSERT_EQ(result[1].size(), 1U);
    EXPECT_EQ(result[1].at(key), value);
}

// ── Two graphs: both non-empty ────────────────────────────────────────────────

TEST(CSVCacheIOManagerTest, two_graphs_both_nonempty_roundtrip)
{
    TempCacheFile temp{"two_graphs_both_nonempty", "csv"};
    const CSVCacheIOManager manager{temp.m_folder, temp.m_base_name};

    const UInt128 key_a{0x1111111111111111ULL, 0x2222222222222222ULL};
    const uint32_t val_a = 10U;
    const UInt128 key_b{0x3333333333333333ULL, 0x4444444444444444ULL};
    const uint32_t val_b = 20U;
    const UInt128 key_c{0x5555555555555555ULL, 0x6666666666666666ULL};
    const uint32_t val_c = 30U;

    const EnumerationData data{
        EnumerationResult{{key_a, val_a}, {key_b, val_b}},
        EnumerationResult{{key_c, val_c}},
    };
    manager.write(data);

    const EnumerationData result = manager.read();
    ASSERT_EQ(result.size(), 2U);
    ASSERT_EQ(result[0].size(), 2U);
    EXPECT_EQ(result[0].at(key_a), val_a);
    EXPECT_EQ(result[0].at(key_b), val_b);
    ASSERT_EQ(result[1].size(), 1U);
    EXPECT_EQ(result[1].at(key_c), val_c);
}
