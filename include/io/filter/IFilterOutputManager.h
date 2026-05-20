#pragma once

#include "FilteringUtils.h"

#include <string>
#include <vector>

namespace sgf
{

/**
 * @brief Interface for writing filter results paired with library graph filenames.
 *
 * Subclasses implement format-specific serialization (e.g. JSON, CSV).
 * The base class owns path construction and directory creation so that concrete
 * implementations only need to handle the byte-level encoding.
 */
class IFilterOutputManager
{
public:
    /**
     * @brief Constructs an IFilterOutputManager targeting a directory and base filename.
     *
     * @param folder        Directory where the output file will be written.
     * @param base_filename File name without extension (e.g. "filter_results").
     */
    IFilterOutputManager(std::string folder, std::string base_filename);

    /**
     * @brief Default virtual destructor.
     */
    virtual ~IFilterOutputManager() = default;

    IFilterOutputManager(const IFilterOutputManager&) = default;
    IFilterOutputManager& operator=(const IFilterOutputManager&) = default;
    IFilterOutputManager(IFilterOutputManager&&) = default;
    IFilterOutputManager& operator=(IFilterOutputManager&&) = default;

    /**
     * @brief Creates the target directory if absent, then writes filter results to file.
     *
     * The full output path is: @c folder / @c base_filename + "." + get_extension().
     * Each entry pairs @p filenames[i] with @p results[i].
     *
     * @param filenames Library graph filenames, one per library graph.
     * @param results   Filter result per library graph; true = pruned, false = survives.
     * @throws SgfPathDoesntExistException if directory creation or file writing fails.
     */
    void write(const std::vector<std::string>& filenames, const FilterResult& results) const;

protected:
    /**
     * @brief Serializes filter results to @p full_path in the concrete format.
     *
     * @param filenames Library graph filenames.
     * @param results   Filter result per library graph.
     * @param full_path Destination file path including the format extension.
     * @throws SgfPathDoesntExistException if the file cannot be opened or written.
     */
    virtual void write_to_file(const std::vector<std::string>& filenames,
                               const FilterResult& results,
                               const std::string& full_path) const = 0;

    /**
     * @brief Returns the file extension used by this format, without a leading dot.
     *
     * @return Extension string (e.g. "json" or "csv").
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
