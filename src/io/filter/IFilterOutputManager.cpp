#include "IFilterOutputManager.h"

#include "FilteringUtils.h"
#include "IOUtils.h"

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace sgf
{

IFilterOutputManager::IFilterOutputManager(std::string folder)
    : m_folder(std::move(folder))
{
}

std::string IFilterOutputManager::build_full_path(const std::string& base_filename) const
{
    const std::filesystem::path full_path =
        std::filesystem::path(m_folder) / (base_filename + "." + get_extension());
    return full_path.string();
}

void IFilterOutputManager::write(std::string base_filename, const std::vector<std::string>& filenames,
                                 const FilterResult& results) const
{
    IOUtils::create_directory_if_needed(m_folder);
    write_to_file(filenames, results, build_full_path(base_filename));
}

}  // namespace sgf
