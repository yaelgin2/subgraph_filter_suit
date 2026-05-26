#pragma once

#include <string>
#include <vector>

namespace sgf
{

/**
 * @brief Utility functions for filesystem I/O operations.
 */
class IOUtils
{
public:
    /**
     * @brief Creates @p folder_path and any missing parent directories.
     *
     * A no-op if the directory already exists.
     *
     * @param folder_path Path of the directory to create.
     * @throws SgfDirectoryCreationException if the directory cannot be created.
     */
    static void create_directory_if_needed(const std::string& folder_path);

    /**
     * @brief Return the paths of all regular files directly inside @p directory_path.
     *
     * Only immediate children are returned (non-recursive). Subdirectories,
     * symlinks, and other non-regular entries are skipped.
     *
     * @param directory_path Path of the directory to scan.
     * @return Absolute paths of every regular file found, in filesystem order.
     * @throws SgfPathDoesntExistException if @p directory_path does not exist or is not a
     * directory.
     */
    static std::vector<std::string> get_files_in_directory(const std::string& directory_path);
};

}  // namespace sgf
