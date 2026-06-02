#include "IOUtils.h"

#include "SgfDirectoryCreationException.h"
#include "SgfPathDoesntExistException.h"

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace sgf
{

void IOUtils::create_directory_if_needed(const std::string& folder_path)
{
    if (folder_path.empty())
    {
        return;
    }
    std::error_code error_code;
    std::filesystem::create_directories(folder_path, error_code);
    if (error_code)
    {
        throw SgfDirectoryCreationException("Failed to create directory '" + folder_path +
                                            "': " + error_code.message());
    }
}

std::vector<std::string> IOUtils::get_files_in_directory(const std::string& directory_path)
{
    const std::filesystem::path dir_path(directory_path);
    if (!std::filesystem::is_directory(dir_path))
    {
        throw SgfPathDoesntExistException("Not a directory: '" + directory_path + "'");
    }

    std::vector<std::string> file_paths;
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(dir_path))
    {
        if (entry.is_regular_file())
        {
            file_paths.push_back(entry.path().string());
        }
    }
    return file_paths;
}

}  // namespace sgf
