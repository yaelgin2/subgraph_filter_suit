#include "IFilterOutputManager.h"

#include "FilteringUtils.h"
#include "IOUtils.h"
#include "LogLevel.h"

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace sgf
{

IFilterOutputManager::IFilterOutputManager(std::string folder, LoggerHandler logger)
    : m_folder(std::move(folder)), m_logger(std::move(logger))
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
    const std::string full_path = build_full_path(base_filename);
    m_logger.log(LogLevel::INFO, "Writing filter results to '" + full_path + "'");
    write_to_file(filenames, results, full_path);
}

}  // namespace sgf
