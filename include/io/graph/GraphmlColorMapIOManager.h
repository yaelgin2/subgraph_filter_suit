#pragma once

#include "LoggerHandler.h"

#include <fstream>
#include <map>
#include <string>

namespace sgf
{

/**
 * @brief Reads and writes the GraphML string→uint color map to/from CSV files.
 *
 * The CSV format has two columns: @c color_label and @c color_id.
 * The caller is responsible for supplying a timestamped base filename so that
 * successive saves never overwrite each other; this class only appends ".csv"
 * and manages the folder path.
 *
 * Typical workflow:
 * 1. Optionally call @c load to pre-populate the reader from a prior run.
 * 2. Pass the loaded map to @c GraphmlGraphReader at construction.
 * 3. After reading the library, call @c save with a timestamped base name and
 *    the reader's @c get_color_map() result to persist the accumulated map.
 */
class GraphmlColorMapIOManager
{
public:
    /**
     * @brief Constructs the manager targeting a specific output directory.
     *
     * @param folder  Directory where CSV files will be written.
     * @param logger  Optional logger for diagnostics.
     */
    explicit GraphmlColorMapIOManager(std::string folder,
                                      LoggerHandler logger = LoggerHandler::null());

    /**
     * @brief Loads a color map from an existing CSV file.
     *
     * The file must have been produced by @c save (header + one row per colour).
     *
     * @param file_path Full path to the CSV file to read.
     * @return The parsed string→uint color map.
     * @throws SgfPathExistsException if the file cannot be opened.
     * @throws GraphConstructionException if any row is malformed.
     */
    std::map<std::string, uint32_t> load(const std::string& file_path) const;

    /**
     * @brief Saves a color map to a CSV file in @c m_folder.
     *
     * The target directory is created if it does not already exist.
     * The caller must supply @p base_filename with a timestamp already embedded
     * (e.g. @c "graphml_color_map_20260615_143022"); this manager appends ".csv".
     *
     * @param base_filename  Base file name without extension, typically including a timestamp.
     * @param color_map      The string→uint mapping to persist.
     * @throws SgfPathExistsException if the file cannot be opened for writing.
     * @throws SgfDirectoryCreationException if the output directory cannot be created.
     */
    void save(const std::string& base_filename,
              const std::map<std::string, uint32_t>& color_map) const;

private:
    static constexpr const char* CSV_COLUMN_LABEL = "color_label";
    static constexpr const char* CSV_COLUMN_ID = "color_id";

    std::string m_folder;
    LoggerHandler m_logger;

    /**
     * @brief Builds the full output path from folder, base filename, and ".csv".
     * @param base_filename Base file name without extension.
     * @return Full path string.
     */
    [[nodiscard]] std::string build_full_path(const std::string& base_filename) const;

    /**
     * @brief Writes @p color_map to the CSV file at @p full_path.
     *
     * @param color_map  Map to write.
     * @param full_path  Destination path including the .csv extension.
     * @throws SgfPathExistsException if the file cannot be opened.
     */
    static void write_to_file(const std::map<std::string, uint32_t>& color_map,
                              const std::string& full_path);

    /**
     * @brief Parses all data rows from @p file into a color map.
     *
     * Assumes the header row has already been consumed by the caller.
     *
     * @param file Opened input stream positioned after the header.
     * @return Parsed string→uint color map.
     * @throws GraphConstructionException if any row is malformed.
     */
    static std::map<std::string, uint32_t> parse_rows(std::ifstream& file);
};

}  // namespace sgf
