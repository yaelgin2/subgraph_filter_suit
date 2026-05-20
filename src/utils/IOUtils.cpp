#include "IOUtils.h"

#include "SgfDirectoryCreationException.h"

#include <filesystem>
#include <string>
#include <system_error>

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

}  // namespace sgf
