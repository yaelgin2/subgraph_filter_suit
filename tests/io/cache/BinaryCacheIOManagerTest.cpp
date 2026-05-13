#include "BinaryCacheIOManager.h"

#include "IGraphPreprocessor.h"
#include "Int128.h"

#include <cstdint>
#include <filesystem>
#include <gtest/gtest.h>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

using namespace sgf;

namespace
{

/**
 * @brief Generates a random 64-bit identifier using a non-deterministic seed.
 * @return A random uint64_t value.
 */
uint64_t random_id()
{
    std::random_device device;
    std::mt19937_64 engine{device()};
    std::uniform_int_distribution<uint64_t> dist;
    return dist(engine);
}

/**
 * @brief RAII wrapper that owns a temp .bin file path and deletes it on destruction.
 *
 * The file is placed in the system temporary directory under a random name derived
 * from the given prefix so concurrent test runs do not collide.
 */
struct TempCacheFile
{
    /**
     * @brief Constructs a temp file handle with a random base name.
     * @param prefix Prepended to a random numeric suffix.
     */
    explicit TempCacheFile(const std::string& prefix)
        : m_folder(std::filesystem::temp_directory_path().string())
        , m_base_name(prefix + "_" + std::to_string(random_id()))
    {
    }

    /**
     * @brief Removes the .bin file from the filesystem if it exists.
     */
    ~TempCacheFile()
    {
        std::filesystem::remove(std::filesystem::path(m_folder) / (m_base_name + ".bin"));
    }

    TempCacheFile(const TempCacheFile&)            = delete;
    TempCacheFile& operator=(const TempCacheFile&) = delete;
    TempCacheFile(TempCacheFile&&)                 = delete;
    TempCacheFile& operator=(TempCacheFile&&)      = delete;

    std::string m_folder;    ///< Directory in which the file is created.
    std::string m_base_name; ///< Base filename without extension.
};

}  // namespace

// ── Empty vector ──────────────────────────────────────────────────────────────

TEST(BinaryCacheIOManagerTest, empty_data_roundtrip)
{
    TempCacheFile temp{"empty_data"};
    const BinaryCacheIOManager manager{temp.m_folder, temp.m_base_name};

    const EnumerationData data{};
    manager.write(data);

    const EnumerationData result = manager.read();
    EXPECT_TRUE(result.empty());
}

// ── One graph, empty map ──────────────────────────────────────────────────────

TEST(BinaryCacheIOManagerTest, single_graph_empty_map_roundtrip)
{
    TempCacheFile temp{"single_empty_map"};
    const BinaryCacheIOManager manager{temp.m_folder, temp.m_base_name};

    const EnumerationData data{EnumerationResult{}};
    manager.write(data);

    const EnumerationData result = manager.read();
    ASSERT_EQ(result.size(), 1U);
    EXPECT_TRUE(result[0].empty());
}

// ── Two graphs: first empty, second non-empty ─────────────────────────────────

TEST(BinaryCacheIOManagerTest, two_graphs_first_empty_second_nonempty_roundtrip)
{
    TempCacheFile temp{"two_graphs_first_empty"};
    const BinaryCacheIOManager manager{temp.m_folder, temp.m_base_name};

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

TEST(BinaryCacheIOManagerTest, two_graphs_both_nonempty_roundtrip)
{
    TempCacheFile temp{"two_graphs_both_nonempty"};
    const BinaryCacheIOManager manager{temp.m_folder, temp.m_base_name};

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
