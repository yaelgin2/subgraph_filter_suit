#pragma once

#include "EnumerationPreprocessManager.h"
#include "IGraphPreprocessor.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace sgf
{

/**
 * @brief Interface for reading and writing enumeration frequency data to a cache file.
 *
 * Subclasses implement format-specific serialization (e.g. CSV, binary).
 * The base class owns path construction and directory creation so that concrete
 * implementations only need to handle the byte-level encoding.
 */
class ICacheIOManager
{
public:
    /**
     * @brief Constructs an ICacheIOManager targeting a specific directory and base filename.
     *
     * @param folder        Directory where the cache file will be written.
     * @param base_filename File name without extension (e.g. "motif_cache").
     */
    ICacheIOManager(std::string folder);

    /**
     * @brief Default virtual destructor.
     */
    virtual ~ICacheIOManager() = default;

    ICacheIOManager(const ICacheIOManager&) = default;
    ICacheIOManager& operator=(const ICacheIOManager&) = default;
    ICacheIOManager(ICacheIOManager&&) = default;
    ICacheIOManager& operator=(ICacheIOManager&&) = default;

    /**
     * @brief Creates the target directory if absent, then writes @p data to the cache file.
     *
     * Each entry is stored under the corresponding name from @p graph_names:
     * the key written to disk for @p data[i] is @p graph_names[i].
     * @p data and @p graph_names must have the same size.
     *
     * @param data        Enumeration results indexed by library position.
     * @param graph_names Names aligned with @p data (same size).
     * @throws SgfPathDoesntExistException if directory creation or file writing fails.
     */
    void write(std::string base_filename, const EnumerationResultVector& data, const std::vector<std::string>& graph_names) const;

    /**
     * @brief Reads data from the cache file.
     *
     * @param base_filename File name without extension (e.g. "motif_cache").
     * @return Map from graph name to its enumeration result.
     * @throws SgfPathDoesntExistException if the file cannot be opened for reading.
     */
    std::unordered_map<std::string, EnumerationResult> read(const std::string& base_filename) const;

protected:
    /**
     * @brief Serializes @p data (paired with @p graph_names) to @p full_path.
     *
     * @param data        Enumeration data to serialize.
     * @param graph_names Names aligned with @p data.
     * @param full_path   Destination file path including the format extension.
     */
    virtual void write_to_file(const EnumerationResultVector& data,
                               const std::vector<std::string>& graph_names,
                               const std::string& full_path) const = 0;

    /**
     * @brief Deserializes a name-keyed result map from @p full_path.
     *
     * @param full_path Source file path including the format extension.
     * @return Map from graph name to enumeration result.
     */
    virtual std::unordered_map<std::string, EnumerationResult>
    read_from_file(const std::string& full_path) const = 0;

    /**
     * @brief Returns the file extension used by this format, without a leading dot.
     *
     * @return Extension string (e.g. "csv" or "bin").
     */
    [[nodiscard]] virtual std::string get_extension() const = 0;

private:
    std::string m_folder;

    /**
     * @brief Builds the full output path from folder, base filename, and extension.
     *
     * @return Full path string.
     */
    [[nodiscard]] std::string build_full_path(const std::string& base_filename) const;
};

}  // namespace sgf
