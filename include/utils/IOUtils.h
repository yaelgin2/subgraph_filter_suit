#pragma once

#include <string>

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
     * @throws SgfPathDoesntExistException if the directory cannot be created.
     */
    static void create_directory_if_needed(const std::string& folder_path);
};

}  // namespace sgf
