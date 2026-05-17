#pragma once

#include <cstdint>
#include <filesystem>
#include <random>
#include <string>

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
 * @brief RAII wrapper that owns a temp cache file path and deletes it on destruction.
 *
 * The file is placed in the system temporary directory under a random name derived
 * from the given prefix so concurrent test runs do not collide.
 */
struct TempCacheFile
{
    /**
     * @brief Constructs a temp file handle with a random base name.
     * @param prefix    Prepended to a random numeric suffix.
     * @param extension File extension without leading dot (e.g. "bin" or "csv").
     */
    TempCacheFile(const std::string& prefix, std::string extension)
        : m_folder(std::filesystem::temp_directory_path().string())
        , m_base_name(prefix + "_" + std::to_string(random_id()))
        , m_extension(std::move(extension))
    {
    }

    /**
     * @brief Removes the temp file from the filesystem if it exists.
     */
    ~TempCacheFile()
    {
        std::filesystem::remove(
            std::filesystem::path(m_folder) / (m_base_name + "." + m_extension));
    }

    TempCacheFile(const TempCacheFile&) = delete;
    TempCacheFile& operator=(const TempCacheFile&) = delete;
    TempCacheFile(TempCacheFile&&) = delete;
    TempCacheFile& operator=(TempCacheFile&&) = delete;

    std::string m_folder;     ///< Directory in which the file is created.
    std::string m_base_name;  ///< Base filename without extension.
    std::string m_extension;  ///< File extension without leading dot.
};

}  // namespace
