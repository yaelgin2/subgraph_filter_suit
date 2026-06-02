#include "IFilterIOManager.h"

#include "FilteringUtils.h"
#include "IOUtils.h"
#include "LogLevel.h"
#include "LoggerHandler.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sgf
{

IFilterIOManager::IFilterIOManager(std::string folder, LoggerHandler logger)
    : m_folder(std::move(folder))
    , m_logger(std::move(logger))
{
}

std::string IFilterIOManager::build_full_path(const std::string& base_filename) const
{
    const std::filesystem::path full_path =
        std::filesystem::path(m_folder) / (base_filename + "." + get_extension());
    return full_path.string();
}

void IFilterIOManager::write(const std::string& base_filename,
                             const std::vector<std::string>& filenames,
                             const FilterResult& results) const
{
    const std::string full_path = build_full_path(base_filename);
    IOUtils::create_directory_if_needed(std::filesystem::path(full_path).parent_path().string());
    m_logger.log(LogLevel::INFO, "Writing filter results to '" + full_path + "'");
    write_to_file(filenames, results, full_path);
}

std::unordered_map<std::string, bool> IFilterIOManager::read(const std::string& base_filename) const
{
    const std::string full_path = build_full_path(base_filename);
    m_logger.log(LogLevel::INFO, "Reading filter results from '" + full_path + "'");
    return read_from_file(full_path);
}

}  // namespace sgf
