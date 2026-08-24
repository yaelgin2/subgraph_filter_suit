#include "BinaryCacheIOManager.h"

#include "CacheTestUtils.h"
#include "IGraphPreprocessor.h"
#include "Int128.h"

#include <cstdint>
#include <gtest/gtest.h>
#include <string>
#include <unordered_map>

using namespace sgf;

TEST(BinaryCacheIOManagerTest, no_matching_files_returns_empty)
{
    TempCacheFile temp{"no_match", "bin"};
    const BinaryCacheIOManager manager{temp.m_folder};

    const std::unordered_map<std::string, EnumerationResult> result =
        manager.read(temp.m_base_name);
    EXPECT_TRUE(result.empty());
}

TEST(BinaryCacheIOManagerTest, single_file_empty_result_roundtrip)
{
    TempCacheFile temp{"single_empty", "bin"};
    const BinaryCacheIOManager manager{temp.m_folder};

    manager.write(temp.m_base_name, EnumerationResult{});

    const std::unordered_map<std::string, EnumerationResult> result =
        manager.read(temp.m_base_name);
    ASSERT_EQ(result.size(), 1U);
    EXPECT_TRUE(result.begin()->second.empty());
}

TEST(BinaryCacheIOManagerTest, single_file_nonempty_result_roundtrip)
{
    TempCacheFile temp{"single_nonempty", "bin"};
    const BinaryCacheIOManager manager{temp.m_folder};

    const UInt128 key{0xDEADBEEF00000000ULL, 0x00000000CAFEBABEULL};
    const uint32_t value = 77U;
    manager.write(temp.m_base_name, EnumerationResult{{key, value}});

    const std::unordered_map<std::string, EnumerationResult> result =
        manager.read(temp.m_base_name);
    ASSERT_EQ(result.size(), 1U);
    ASSERT_EQ(result.begin()->second.size(), 1U);
    EXPECT_EQ(result.begin()->second.at(key), value);
}

TEST(BinaryCacheIOManagerTest, category_merges_two_graphs_first_empty_second_nonempty)
{
    TempCacheCategory temp{"two_graphs_first_empty", "bin"};
    const BinaryCacheIOManager manager{temp.m_folder};

    const UInt128 key{0xDEADBEEF00000000ULL, 0x00000000CAFEBABEULL};
    const uint32_t value = 77U;

    manager.write(temp.base_filename("graph_a"), EnumerationResult{});
    temp.track("graph_a");
    manager.write(temp.base_filename("graph_b"), EnumerationResult{{key, value}});
    temp.track("graph_b");

    const std::unordered_map<std::string, EnumerationResult> result = manager.read(temp.m_category);
    ASSERT_EQ(result.size(), 2U);
    EXPECT_TRUE(result.at("graph_a").empty());
    ASSERT_EQ(result.at("graph_b").size(), 1U);
    EXPECT_EQ(result.at("graph_b").at(key), value);
}

TEST(BinaryCacheIOManagerTest, category_merges_two_graphs_both_nonempty)
{
    TempCacheCategory temp{"two_graphs_both_nonempty", "bin"};
    const BinaryCacheIOManager manager{temp.m_folder};

    const UInt128 key_a{0x1111111111111111ULL, 0x2222222222222222ULL};
    const uint32_t val_a = 10U;
    const UInt128 key_b{0x3333333333333333ULL, 0x4444444444444444ULL};
    const uint32_t val_b = 20U;
    const UInt128 key_c{0x5555555555555555ULL, 0x6666666666666666ULL};
    const uint32_t val_c = 30U;

    manager.write(temp.base_filename("graph_a"), EnumerationResult{{key_a, val_a}, {key_b, val_b}});
    temp.track("graph_a");
    manager.write(temp.base_filename("graph_b"), EnumerationResult{{key_c, val_c}});
    temp.track("graph_b");

    const std::unordered_map<std::string, EnumerationResult> result = manager.read(temp.m_category);
    ASSERT_EQ(result.size(), 2U);
    ASSERT_EQ(result.at("graph_a").size(), 2U);
    EXPECT_EQ(result.at("graph_a").at(key_a), val_a);
    EXPECT_EQ(result.at("graph_a").at(key_b), val_b);
    ASSERT_EQ(result.at("graph_b").size(), 1U);
    EXPECT_EQ(result.at("graph_b").at(key_c), val_c);
}
