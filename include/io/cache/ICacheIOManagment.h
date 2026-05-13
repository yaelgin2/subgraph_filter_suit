#pragma once

#include "EnumerationPreprocessManager.h"

#include <string>

namespace sgf
{

/**
 * @brief Interface for writing enumeration frequency data to a cache file.
 *
 * Subclasses implement format-specific serialization (e.g. CSV, binary).
 * The base class owns path construction and directory creation so that concrete
 * writers only need to handle the byte-level encoding.
 */
class ICacheIOManagment
{
public:
    /**
     * @brief Constructs an ICacheIOManagment targeting a specific directory and base filename.
     *
     * @param folder        Directory where the cache file will be written.
     * @param base_filename File name without extension (e.g. "motif_cache").
     */
    ICacheIOManagment(std::string folder, std::string base_filename);

    /**
     * @brief Default virtual destructor.
     */
    virtual ~ICacheIOManagment() = default;

    ICacheIOManagment(const ICacheIOManagment&) = default;
    ICacheIOManagment& operator=(const ICacheIOManagment&) = default;
    ICacheIOManagment(ICacheIOManagment&&) = default;
    ICacheIOManagment& operator=(ICacheIOManagment&&) = default;

    /**
     * @brief Creates the target directory if absent, then writes @p data to the cache file.
     *
     * The full output path is: @c folder / @c base_filename + "." + get_extension().
     * Directory creation is delegated to IOUtils::create_directory_if_needed; format-specific
     * serialization is delegated to write_to_file().
     *
     * @param data Enumeration data to persist.
     * @throws SgfPathDoesntExistException if directory creation or file writing fails.
     */
    void write(const EnumerationData& data) const;

     /**
     * @brief Read data from the cache file.
     *
     * The full output path is: @c folder / @c base_filename + "." + get_extension().
     * Deserialization is delegated to read_from_file().
     *
     * @throws SgfPathDoesntExistException if directory creation or file writing fails.
     */
    EnumerationData read() const;

protected:
    /**
     * @brief Serializes @p data to @p full_path in the concrete format.
     *
     * @param data      Enumeration data to write.
     * @param full_path Destination file path including the format extension.
     * @throws SgfPathDoesntExistException if the file cannot be opened or written.
     */
    virtual void write_to_file(const EnumerationData& data, const std::string& full_path) const = 0;

    /**
     * @brief Deseerializes data from @p full_path in the concrete format.
     *
     * @param full_path Source file path including the format extension.
     * @throws SgfPathDoesntExistException if the file cannot be opened or read from.
     */
    virtual EnumerationData read_from_file(const std::string& full_path) const = 0;

    /**
     * @brief Returns the file extension used by this format, without a leading dot.
     *
     * @return Extension string (e.g. "csv" or "bin").
     */
    [[nodiscard]] virtual std::string get_extension() const = 0;

private:
    std::string m_folder;
    std::string m_base_filename;

    /**
     * @brief Builds the full output path from folder, base filename, and extension.
     *
     * @return Full path string.
     */
    [[nodiscard]] std::string build_full_path() const;
};

}  // namespace sgf
