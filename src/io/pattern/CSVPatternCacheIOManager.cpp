#include "CSVPatternCacheIOManager.h"

#include "GraphConstructionException.h"
#include "IPatternCacheIOManager.h"
#include "LoggerHandler.h"
#include "SgfPathExistsException.h"

#include <cstdint>
#include <fstream>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace sgf
{

CSVPatternCacheIOManager::CSVPatternCacheIOManager(std::string folder, LoggerHandler logger)
    : IPatternCacheIOManager(std::move(folder), std::move(logger))
{
}

std::string CSVPatternCacheIOManager::get_mapping_extension() const
{
    return "csv";
}

void CSVPatternCacheIOManager::write_header(std::ofstream& file)
{
    file << CSV_COL_PATTERN_NAME << "," << CSV_COL_GRAPH_INDEX << "\n";
}

void CSVPatternCacheIOManager::write_rows(const PatternMapping& mapping, std::ofstream& file)
{
    for (const auto& entry : mapping)
    {
        for (const uint32_t graph_index : entry.second)
        {
            file << entry.first << "," << graph_index << "\n";
        }
    }
}

void CSVPatternCacheIOManager::write_mapping_to_file(const PatternMapping& mapping,
                                                     const std::string& full_path) const
{
    std::ofstream file(full_path);
    if (!file.is_open())
    {
        throw SgfPathExistsException("Cannot open file for writing: '" + full_path + "'");
    }
    write_header(file);
    write_rows(mapping, file);
    if (file.fail())
    {
        throw SgfPathExistsException("Write error on file: '" + full_path + "'");
    }
}

void CSVPatternCacheIOManager::insert_row(const std::string& line, PatternMapping& mapping)
{
    std::istringstream stream(line);
    std::string pattern_name;
    std::string graph_index_str;
    std::getline(stream, pattern_name, ',');
    std::getline(stream, graph_index_str);
    const uint32_t graph_index = static_cast<uint32_t>(std::stoul(graph_index_str));
    mapping[pattern_name].insert(graph_index);
}

PatternMapping CSVPatternCacheIOManager::parse_file(std::ifstream& file)
{
    PatternMapping result;
    std::string line;
    std::getline(file, line);
    while (std::getline(file, line))
    {
        insert_row(line, result);
    }
    return result;
}

PatternMapping CSVPatternCacheIOManager::read_mapping_from_file(const std::string& full_path) const
{
    std::ifstream file(full_path);
    if (!file.is_open())
    {
        throw SgfPathExistsException("Cannot open file for reading: '" + full_path + "'");
    }
    try
    {
        return parse_file(file);
    }
    catch (const std::bad_alloc&)
    {
        throw GraphConstructionException("Memory allocation failed reading CSV pattern index: '" +
                                         full_path + "'");
    }
    catch (const std::invalid_argument& ex)
    {
        throw GraphConstructionException("Malformed value in '" + full_path + "': " + ex.what());
    }
    catch (const std::out_of_range& ex)
    {
        throw GraphConstructionException("Out-of-range value in '" + full_path + "': " + ex.what());
    }
}

}  // namespace sgf
