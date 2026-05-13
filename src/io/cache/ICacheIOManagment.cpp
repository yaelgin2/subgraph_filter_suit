#include "ICacheIOManagment.h"

#include "IOUtils.h"

#include <filesystem>
#include <string>
#include <utility>

namespace sgf
{

ICacheIOManagment::ICacheIOManagment(std::string folder, std::string base_filename)
    : m_folder(std::move(folder))
    , m_base_filename(std::move(base_filename))
{
}

std::string ICacheIOManagment::build_full_path() const
{
    const std::filesystem::path full_path =
        std::filesystem::path(m_folder) / (m_base_filename + "." + get_extension());
    return full_path.string();
}

void ICacheIOManagment::write(const EnumerationData& data) const
{
    IOUtils::create_directory_if_needed(m_folder);
    write_to_file(data, build_full_path());
}

EnumerationData ICacheIOManagment::read() const
{
    return read_from_file(build_full_path());
}

}  // namespace sgf
