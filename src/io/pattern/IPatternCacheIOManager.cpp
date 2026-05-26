#include "IPatternCacheIOManager.h"

#include "IOUtils.h"
#include "IPatternPreprocessor.h"
#include "IPatternWriter.h"
#include "LogLevel.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace sgf
{

IPatternCacheIOManager::IPatternCacheIOManager(std::string folder, LoggerHandler logger,
                                               std::unique_ptr<IPatternWriter> writer)
    : m_folder(std::move(folder))
    , m_logger(std::move(logger))
    , m_writer(std::move(writer))
{
}

std::string IPatternCacheIOManager::build_pattern_base_name(const uint32_t index,
                                                             const std::string& timestamp) const
{
    return "pattern_" + std::to_string(index) + "_" + timestamp;
}

std::string IPatternCacheIOManager::build_pattern_full_path(const uint32_t index,
                                                             const std::string& timestamp) const
{
    const std::string base_name = build_pattern_base_name(index, timestamp);
    const std::string ext = m_writer->get_file_extension();
    const std::string filename = ext.empty() ? base_name : base_name + "." + ext;
    return (std::filesystem::path(m_folder) / filename).string();
}

std::string IPatternCacheIOManager::build_mapping_path(const std::string& timestamp) const
{
    const std::string filename = "pattern_index_" + timestamp + "." + get_mapping_extension();
    return (std::filesystem::path(m_folder) / filename).string();
}

void IPatternCacheIOManager::write_single_pattern(const PatternPreprocessorResult& result,
                                                   const uint32_t index,
                                                   const std::string& timestamp,
                                                   PatternMapping& mapping) const
{
    const std::string pattern_path = build_pattern_full_path(index, timestamp);
    m_writer->write(result.first, pattern_path, m_logger);
    const std::string base_name = build_pattern_base_name(index, timestamp);
    mapping.emplace(base_name, result.second);
}

void IPatternCacheIOManager::write(const PatternOutput& patterns,
                                   const std::string& timestamp) const
{
    IOUtils::create_directory_if_needed(m_folder);
    PatternMapping mapping;
    uint32_t pattern_idx = 0U;
    for (const PatternPreprocessorResult& result : patterns)
    {
        write_single_pattern(result, pattern_idx, timestamp, mapping);
        ++pattern_idx;
    }
    const std::string mapping_path = build_mapping_path(timestamp);
    m_logger.log(LogLevel::INFO, "Writing pattern index to '" + mapping_path + "'");
    write_mapping_to_file(mapping, mapping_path);
}

PatternMapping IPatternCacheIOManager::read(const std::string& timestamp) const
{
    const std::string mapping_path = build_mapping_path(timestamp);
    m_logger.log(LogLevel::INFO, "Reading pattern index from '" + mapping_path + "'");
    return read_mapping_from_file(mapping_path);
}

}  // namespace sgf
