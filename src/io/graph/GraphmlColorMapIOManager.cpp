#include "GraphmlColorMapIOManager.h"

#include "GraphConstructionException.h"
#include "IOUtils.h"
#include "LogLevel.h"
#include "SgfPathExistsException.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace sgf
{

GraphmlColorMapIOManager::GraphmlColorMapIOManager(std::string folder, LoggerHandler logger)
    : m_folder(std::move(folder))
    , m_logger(std::move(logger))
{
}

std::string GraphmlColorMapIOManager::build_full_path(const std::string& base_filename) const
{
    const std::filesystem::path full_path =
        std::filesystem::path(m_folder) / (base_filename + ".csv");
    return full_path.string();
}

void GraphmlColorMapIOManager::write_to_file(const std::map<std::string, uint32_t>& color_map,
                                             const std::string& full_path)
{
    std::ofstream file(full_path);
    if (!file.is_open())
    {
        throw SgfPathExistsException("Cannot open color map file for writing: '" + full_path + "'");
    }
    file << CSV_COLUMN_LABEL << "," << CSV_COLUMN_ID << "\n";
    for (const auto& [label, color_id] : color_map)
    {
        file << label << "," << color_id << "\n";
    }
    if (file.fail())
    {
        throw SgfPathExistsException("Write error on color map file: '" + full_path + "'");
    }
}

std::map<std::string, uint32_t> GraphmlColorMapIOManager::parse_rows(std::ifstream& file)
{
    std::map<std::string, uint32_t> result;
    std::string line;
    while (std::getline(file, line))
    {
        std::istringstream stream(line);
        std::string label;
        std::string id_str;
        std::getline(stream, label, ',');
        std::getline(stream, id_str);
        if (label.empty() || id_str.empty())
        {
            throw GraphConstructionException("Malformed row in color map CSV: '" + line + "'");
        }
        result[label] = static_cast<uint32_t>(std::stoul(id_str));
    }
    return result;
}

std::map<std::string, uint32_t>
GraphmlColorMapIOManager::load(const std::string& file_path) const
{
    std::ifstream file(file_path);
    if (!file.is_open())
    {
        throw SgfPathExistsException("Cannot open color map file for reading: '" + file_path + "'");
    }
    m_logger.log(LogLevel::INFO, "Loading GraphML color map from '" + file_path + "'");
    std::string header;
    std::getline(file, header);
    try
    {
        const std::map<std::string, uint32_t> result = parse_rows(file);
        m_logger.log(LogLevel::INFO,
                     "Loaded " + std::to_string(result.size()) + " colour entries from '" +
                         file_path + "'");
        return result;
    }
    catch (const std::invalid_argument& ex)
    {
        throw GraphConstructionException("Malformed color id in '" + file_path + "': " + ex.what());
    }
    catch (const std::out_of_range& ex)
    {
        throw GraphConstructionException("Color id out of range in '" + file_path + "': " +
                                         ex.what());
    }
}

void GraphmlColorMapIOManager::save(const std::string& base_filename,
                                    const std::map<std::string, uint32_t>& color_map) const
{
    IOUtils::create_directory_if_needed(m_folder);
    const std::string full_path = build_full_path(base_filename);
    m_logger.log(LogLevel::INFO, "Saving GraphML color map to '" + full_path + "'");
    write_to_file(color_map, full_path);
    m_logger.log(LogLevel::INFO,
                 "Saved " + std::to_string(color_map.size()) + " colour entries to '" + full_path +
                     "'");
}

}  // namespace sgf
