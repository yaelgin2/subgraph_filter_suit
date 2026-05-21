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

TEST(CSVCacheIOManagerTest, empty_data_roundtrip)
{
    TempCacheFile temp{"empty_data", "csv"};
    const CSVCacheIOManager manager{temp.m_folder, temp.m_base_name};

    const EnumerationData data{};
    const std::vector<std::string> names{};
    manager.write(data, names);

    const std::unordered_map<std::string, EnumerationResult> result = manager.read();
    EXPECT_TRUE(result.empty());
}

// CSV stores only (graph, motif, count) triples; a graph with no motifs writes
// no rows, so reading back yields an empty map rather than a one-element map
// containing an empty EnumerationResult.

TEST(CSVCacheIOManagerTest, single_graph_empty_map_reads_back_as_empty)
{
    TempCacheFile temp{"single_empty_map", "csv"};
    const CSVCacheIOManager manager{temp.m_folder, temp.m_base_name};

    const EnumerationData data{EnumerationResult{}};
    const std::vector<std::string> names{"graph_a"};
    manager.write(data, names);

    const std::unordered_map<std::string, EnumerationResult> result = manager.read();
    EXPECT_TRUE(result.empty());
}

TEST(CSVCacheIOManagerTest, two_graphs_first_empty_second_nonempty_roundtrip)
{
    TempCacheFile temp{"two_graphs_first_empty", "csv"};
    const CSVCacheIOManager manager{temp.m_folder, temp.m_base_name};

    const UInt128 key{0xDEADBEEF00000000ULL, 0x00000000CAFEBABEULL};
    const uint32_t value = 77U;

    const EnumerationData data{EnumerationResult{}, EnumerationResult{{key, value}}};
    const std::vector<std::string> names{"graph_a", "graph_b"};
    manager.write(data, names);

    const std::unordered_map<std::string, EnumerationResult> result = manager.read();
    ASSERT_EQ(result.size(), 1U);
    ASSERT_EQ(result.at("graph_b").size(), 1U);
    EXPECT_EQ(result.at("graph_b").at(key), value);
}

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
    const std::vector<std::string> names{"graph_a", "graph_b"};
    manager.write(data, names);

    const std::unordered_map<std::string, EnumerationResult> result = manager.read();
    ASSERT_EQ(result.size(), 2U);
    ASSERT_EQ(result.at("graph_a").size(), 2U);
    EXPECT_EQ(result.at("graph_a").at(key_a), val_a);
    EXPECT_EQ(result.at("graph_a").at(key_b), val_b);
    ASSERT_EQ(result.at("graph_b").size(), 1U);
    EXPECT_EQ(result.at("graph_b").at(key_c), val_c);
}
