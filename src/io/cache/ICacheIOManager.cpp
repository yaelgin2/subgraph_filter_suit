#include "ICacheIOManager.h"

#include "IGraphPreprocessor.h"
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

ICacheIOManager::ICacheIOManager(std::string folder, LoggerHandler logger)
    : m_folder(std::move(folder))
    , m_logger(std::move(logger))
{
}

std::string ICacheIOManager::build_full_path(const std::string& base_filename) const
{
    const std::filesystem::path full_path =
        std::filesystem::path(m_folder) / (base_filename + "." + get_extension());
    return full_path.string();
}

void ICacheIOManager::write(const std::string& base_filename, const EnumerationResult& data) const
{
    IOUtils::create_directory_if_needed(m_folder);
    const std::string full_path = build_full_path(base_filename);
    m_logger.log(LogLevel::INFO, "Writing cache to '" + full_path + "'");
    write_to_file(data, full_path);
}

std::unordered_map<std::string, EnumerationResult>
ICacheIOManager::read(const std::string& base_filename) const
{
    const std::vector<std::string> files = IOUtils::get_files_in_directory(m_folder);
    const std::string extension = "." + get_extension();
    const std::string category_prefix = base_filename + "_";
    std::unordered_map<std::string, EnumerationResult> merged;
    for (const std::string& file : files)
    {
        const std::filesystem::path file_path(file);
        const std::string stem = file_path.stem().string();
        const bool is_exact_match = stem == base_filename;
        const bool is_category_match = stem.rfind(category_prefix, 0) == 0;
        if (file_path.extension() != extension || (!is_exact_match && !is_category_match))
        {
            continue;
        }
        m_logger.log(LogLevel::INFO, "Reading cache from '" + file + "'");
        const std::string graph_name = is_exact_match ? stem : stem.substr(category_prefix.size());
        merged.emplace(graph_name, read_from_file(file_path.string()));
    }
    return merged;
}

bool ICacheIOManager::exists(const std::string& base_filename) const
{
    return std::filesystem::exists(build_full_path(base_filename));
}

}  // namespace sgf
