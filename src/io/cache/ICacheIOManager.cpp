#include "ICacheIOManager.h"

#include "EnumerationPreprocessManager.h"
#include "IOUtils.h"

#include <filesystem>
#include <string>
#include <utility>

namespace sgf
{

ICacheIOManager::ICacheIOManager(std::string folder, std::string base_filename)
    : m_folder(std::move(folder))
    , m_base_filename(std::move(base_filename))
{
}

std::string ICacheIOManager::build_full_path() const
{
    const std::filesystem::path full_path =
        std::filesystem::path(m_folder) / (m_base_filename + "." + get_extension());
    return full_path.string();
}

void ICacheIOManager::write(const EnumerationData& data) const
{
    IOUtils::create_directory_if_needed(m_folder);
    write_to_file(data, build_full_path());
}

EnumerationData ICacheIOManager::read() const
{
    return read_from_file(build_full_path());
}

}  // namespace sgf
