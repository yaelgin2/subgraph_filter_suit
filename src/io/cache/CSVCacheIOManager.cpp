#include "CSVCacheIOManager.h"

#include "EnumerationPreprocessManager.h"
#include "GraphConstructionException.h"
#include "ICacheIOManager.h"
#include "Int128.h"
#include "SgfPathDoesntExistException.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
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

CSVCacheIOManager::CSVCacheIOManager(std::string folder)
    : ICacheIOManager(std::move(folder))
{
}

std::string CSVCacheIOManager::get_extension() const
{
    return "csv";
}

std::string CSVCacheIOManager::uint128_to_decimal(UInt128 value)
{
    if (!value)
    {
        return "0";
    }
    std::string digits;
    while (value)
    {
        const uint32_t remainder = value.divmod_uint32(10U);
        digits.push_back(static_cast<char>('0' + remainder));
    }
    std::reverse(digits.begin(), digits.end());
    return digits;
}

UInt128 CSVCacheIOManager::decimal_to_uint128(const std::string& decimal_str)
{
    UInt128 result;
    for (const auto& digit_char : decimal_str)
    {
        const UInt128 digit{static_cast<uint32_t>(digit_char - '0')};
        result = (result << 3U) + (result << 1U) + digit;
    }
    return result;
}

void CSVCacheIOManager::write_header(std::ofstream& file)
{
    file << CSV_COLUMN_GRAPH_NAME << "," << CSV_COLUMN_MOTIF_NUMBER << ","
         << CSV_COLUMN_APPEARANCES << "\n";
}

void CSVCacheIOManager::write_rows(const EnumerationData& data,
                                   const std::vector<std::string>& graph_names,
                                   std::ofstream& file)
{
    for (size_t graph_index = 0U; graph_index < data.size(); ++graph_index)
    {
        for (const auto& entry : data[graph_index])
        {
            file << graph_names[graph_index] << "," << uint128_to_decimal(entry.first) << ","
                 << entry.second << "\n";
        }
    }
}

void CSVCacheIOManager::write_to_file(const EnumerationData& data,
                                      const std::vector<std::string>& graph_names,
                                      const std::string& full_path) const
{
    std::ofstream file(full_path);
    if (!file.is_open())
    {
        throw SgfPathDoesntExistException("Cannot open file for writing: '" + full_path + "'");
    }
    write_header(file);
    write_rows(data, graph_names, file);
    if (file.fail())
    {
        throw SgfPathDoesntExistException("Write error on file: '" + full_path + "'");
    }
}

void CSVCacheIOManager::insert_row(const std::string& line,
                                   std::unordered_map<std::string, EnumerationResult>& data)
{
    std::istringstream stream(line);
    std::string graph_name;
    std::string motif_str;
    std::string appearances_str;
    std::getline(stream, graph_name, ',');
    std::getline(stream, motif_str, ',');
    std::getline(stream, appearances_str);
    const UInt128 motif_key = decimal_to_uint128(motif_str);
    const uint32_t appearances = static_cast<uint32_t>(std::stoul(appearances_str));
    data[graph_name][motif_key] = appearances;
}

std::unordered_map<std::string, EnumerationResult>
CSVCacheIOManager::parse_file(std::ifstream& file)
{
    std::unordered_map<std::string, EnumerationResult> result;
    std::string line;
    std::getline(file, line);  // discard header row
    while (std::getline(file, line))
    {
        insert_row(line, result);
    }
    return result;
}

std::unordered_map<std::string, EnumerationResult>
CSVCacheIOManager::read_from_file(const std::string& full_path) const
{
    std::ifstream file(full_path);
    if (!file.is_open())
    {
        throw SgfPathDoesntExistException("Cannot open file for reading: '" + full_path + "'");
    }
    try
    {
        return parse_file(file);
    }
    catch (const std::bad_alloc&)
    {
        throw GraphConstructionException("Memory allocation failed reading CSV cache file: '" +
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
