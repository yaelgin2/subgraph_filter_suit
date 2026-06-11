#include "IPatternCacheIOManager.h"

#include "IOUtils.h"
#include "IPatternPreprocessor.h"
#include "IPatternWriter.h"
#include "LogLevel.h"
#include "LoggerHandler.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>

namespace sgf
{

IPatternCacheIOManager::IPatternCacheIOManager(std::string folder, LoggerHandler logger)
    : m_folder(std::move(folder))
    , m_logger(std::move(logger))
{
}

std::string IPatternCacheIOManager::build_pattern_base_name(const uint32_t index,
                                                            const std::string& timestamp)
{
    return "pattern_" + std::to_string(index) + "_" + timestamp;
}

std::string IPatternCacheIOManager::build_pattern_full_path(const uint32_t index,
                                                            const std::string& timestamp,
                                                            const IPatternWriter& writer) const
{
    const std::string base_name = build_pattern_base_name(index, timestamp);
    const std::string ext = writer.get_file_extension();
    const std::string filename = ext.empty() ? base_name : base_name + "." + ext;
    return (std::filesystem::path(m_folder) / filename).string();
}

std::string IPatternCacheIOManager::build_mapping_path(const std::string& timestamp) const
{
    const std::string filename = "pattern_index_" + timestamp + "." + get_mapping_extension();
    return (std::filesystem::path(m_folder) / filename).string();
}

void IPatternCacheIOManager::write_single_pattern(
    const PatternPreprocessorResult& result, const uint32_t index, const std::string& timestamp,
    const IPatternWriter& writer, PatternMapping& mapping, const bool is_directed) const
{
    const std::string pattern_path = build_pattern_full_path(index, timestamp, writer);
    writer.write(result.first, pattern_path, m_logger, is_directed);
    const std::string filename = std::filesystem::path(pattern_path).filename().string();
    mapping.emplace(filename, result.second);
}

void IPatternCacheIOManager::write(const PatternOutput& patterns, const std::string& timestamp,
                                   const std::shared_ptr<IPatternWriter>& writer,
                                   const bool is_directed) const
{
    IOUtils::create_directory_if_needed(m_folder);
    PatternMapping mapping;
    uint32_t pattern_idx = 0U;
    for (const PatternPreprocessorResult& result : patterns)
    {
        write_single_pattern(result, pattern_idx, timestamp, *writer, mapping, is_directed);
        ++pattern_idx;
    }
    const std::string mapping_path = build_mapping_path(timestamp);
    m_logger.log(LogLevel::INFO, "Writing pattern index to '" + mapping_path + "'");
    write_mapping_to_file(mapping, mapping_path);
}

PatternMapping IPatternCacheIOManager::read(const std::string& full_path) const
{
    m_logger.log(LogLevel::INFO, "Reading pattern index from '" + full_path + "'");
    return read_mapping_from_file(full_path);
}

}  // namespace sgf
