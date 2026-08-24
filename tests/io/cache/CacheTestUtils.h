#pragma once

#include <cstdint>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

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
        std::filesystem::remove(std::filesystem::path(m_folder) /
                                (m_base_name + "." + m_extension));
    }

    TempCacheFile(const TempCacheFile&) = delete;
    TempCacheFile& operator=(const TempCacheFile&) = delete;
    TempCacheFile(TempCacheFile&&) = delete;
    TempCacheFile& operator=(TempCacheFile&&) = delete;

    std::string m_folder;     ///< Directory in which the file is created.
    std::string m_base_name;  ///< Base filename without extension.
    std::string m_extension;  ///< File extension without leading dot.
};

/**
 * @brief RAII wrapper for a per-graph cache category spanning multiple files.
 *
 * Mirrors how preprocessing writes one file per graph under a shared category prefix
 * (e.g. "motif_cache_lib_1", "motif_cache_lib_2"). Tracks every graph name written under
 * it so its destructor can remove each corresponding file.
 */
struct TempCacheCategory
{
    /**
     * @brief Constructs a temp category handle with a random prefix.
     * @param prefix    Prepended to a random numeric suffix.
     * @param extension File extension without leading dot (e.g. "bin" or "csv").
     */
    TempCacheCategory(const std::string& prefix, std::string extension)
        : m_folder(std::filesystem::temp_directory_path().string())
        , m_category(prefix + "_" + std::to_string(random_id()))
        , m_extension(std::move(extension))
    {
    }

    /**
     * @brief Removes every tracked per-graph file from the filesystem if it exists.
     */
    ~TempCacheCategory()
    {
        for (const std::string& graph_name : m_written_graph_names)
        {
            std::filesystem::remove(std::filesystem::path(m_folder) /
                                    (base_filename(graph_name) + "." + m_extension));
        }
    }

    TempCacheCategory(const TempCacheCategory&) = delete;
    TempCacheCategory& operator=(const TempCacheCategory&) = delete;
    TempCacheCategory(TempCacheCategory&&) = delete;
    TempCacheCategory& operator=(TempCacheCategory&&) = delete;

    /**
     * @brief Builds the per-graph base filename for @p graph_name under this category.
     * @param graph_name Name of the graph.
     * @return Combined base filename, without extension.
     */
    [[nodiscard]] std::string base_filename(const std::string& graph_name) const
    {
        return m_category + "_" + graph_name;
    }

    /**
     * @brief Records that @p graph_name's file was written, so it gets cleaned up.
     * @param graph_name Name of the graph whose file was written.
     */
    void track(const std::string& graph_name)
    {
        m_written_graph_names.push_back(graph_name);
    }

    std::string m_folder;                            ///< Directory in which files are created.
    std::string m_category;                          ///< Random cache-category prefix.
    std::string m_extension;                         ///< File extension without leading dot.
    std::vector<std::string> m_written_graph_names;  ///< Graphs written under this category.
};

}  // namespace
