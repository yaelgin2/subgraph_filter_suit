#include "CSVFilterIOManager.h"

#include "FilteringUtils.h"
#include "GraphConstructionException.h"
#include "IFilterIOManager.h"
#include "LoggerHandler.h"
#include "SgfPathExistsException.h"

#include <cstddef>
#include <fstream>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sgf
{

CSVFilterIOManager::CSVFilterIOManager(std::string folder, LoggerHandler logger)
    : IFilterIOManager(std::move(folder), std::move(logger))
{
}

std::string CSVFilterIOManager::get_extension() const
{
    return "csv";
}

void CSVFilterIOManager::write_to_file(const std::vector<std::string>& filenames,
                                       const FilterResult& results,
                                       const std::string& full_path) const
{
    std::ofstream file(full_path);
    if (!file.is_open())
    {
        throw SgfPathExistsException("Cannot open file for writing: '" + full_path + "'");
    }
    for (size_t idx = 0U; idx < filenames.size(); ++idx)
    {
        file << filenames[idx] << "," << results[idx] << "\n";
    }
    if (file.fail())
    {
        throw SgfPathExistsException("Write error on file: '" + full_path + "'");
    }
}

void CSVFilterIOManager::insert_row(const std::string& line,
                                    std::unordered_map<std::string, bool>& result)
{
    std::istringstream stream(line);
    std::string filename;
    std::string result_str;
    std::getline(stream, filename, ',');
    std::getline(stream, result_str);
    result[filename] = (std::stoi(result_str) != 0);
}

std::unordered_map<std::string, bool> CSVFilterIOManager::parse_file(std::ifstream& file)
{
    std::unordered_map<std::string, bool> result;
    std::string line;
    while (std::getline(file, line))
    {
        insert_row(line, result);
    }
    return result;
}

std::unordered_map<std::string, bool>
CSVFilterIOManager::read_from_file(const std::string& full_path) const
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
        throw GraphConstructionException(
            "Memory allocation failed reading CSV filter results: '" + full_path + "'");
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
