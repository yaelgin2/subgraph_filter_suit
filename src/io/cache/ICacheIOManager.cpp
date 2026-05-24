#include "ICacheIOManager.h"

#include "EnumerationPreprocessManager.h"
#include "IOUtils.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sgf
{

ICacheIOManager::ICacheIOManager(std::string folder)
    : m_folder(std::move(folder))
{
}

std::string ICacheIOManager::build_full_path(const std::string& base_filename) const
{
    const std::filesystem::path full_path =
        std::filesystem::path(m_folder) / (base_filename + "." + get_extension());
    return full_path.string();
}

void ICacheIOManager::write(std::string base_filename, const EnumerationResultVector& data,
                            const std::vector<std::string>& graph_names) const
{
    IOUtils::create_directory_if_needed(m_folder);
    write_to_file(data, graph_names, build_full_path(base_filename));
}

std::unordered_map<std::string, EnumerationResult>
ICacheIOManager::read(const std::string& base_filename) const
{
    return read_from_file(build_full_path(base_filename));
}

}  // namespace sgf
