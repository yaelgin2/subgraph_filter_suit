#include "ICacheIOManager.h"

#include "EnumerationPreprocessManager.h"
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

void ICacheIOManager::write(const std::string& base_filename, const EnumerationResultVector& data,
                            const std::vector<std::string>& graph_names) const
{
    IOUtils::create_directory_if_needed(m_folder);
    const std::string full_path = build_full_path(base_filename);
    m_logger.log(LogLevel::INFO, "Writing cache to '" + full_path + "'");
    write_to_file(data, graph_names, full_path);
}

std::unordered_map<std::string, EnumerationResult>
ICacheIOManager::read(const std::string& base_filename) const
{
    const std::string full_path = build_full_path(base_filename);
    m_logger.log(LogLevel::INFO, "Reading cache from '" + full_path + "'");
    return read_from_file(full_path);
}

}  // namespace sgf
