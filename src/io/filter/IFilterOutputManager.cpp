#include "IFilterOutputManager.h"

#include "IOUtils.h"

#include <filesystem>
#include <string>
#include <utility>

namespace sgf
{

IFilterOutputManager::IFilterOutputManager(std::string folder, std::string base_filename)
    : m_folder(std::move(folder))
    , m_base_filename(std::move(base_filename))
{
}

std::string IFilterOutputManager::build_full_path() const
{
    const std::filesystem::path full_path =
        std::filesystem::path(m_folder) / (m_base_filename + "." + get_extension());
    return full_path.string();
}

void IFilterOutputManager::write(const std::vector<std::string>& filenames,
                                 const FilterResult& results) const
{
    IOUtils::create_directory_if_needed(m_folder);
    write_to_file(filenames, results, build_full_path());
}

}  // namespace sgf
