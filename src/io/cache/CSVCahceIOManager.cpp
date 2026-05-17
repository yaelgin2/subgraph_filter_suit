#include "CSVCacheIOManager.h"
#include "EnumerationPreprocessManager.h"
#include "GraphConstructionException.h"
#include "ICacheIOManagment.h"
#include "Int128.h"
#include "SgfPathDoesntExistException.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace sgf
{

CSVIOManager::CSVIOManager(std::string folder, std::string base_filename)
    : ICacheIOManagment(std::move(folder), std::move(base_filename))
{
}

std::string CSVIOManager::get_extension() const
{
    return "csv";
}

std::string CSVIOManager::uint128_to_decimal(UInt128 value)
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

UInt128 CSVIOManager::decimal_to_uint128(const std::string& decimal_str)
{
    UInt128 result;
    for (const auto& digit_char : decimal_str)
    {
        const UInt128 digit{static_cast<uint32_t>(digit_char - '0')};
        result = (result << 3U) + (result << 1U) + digit;
    }
    return result;
}

void CSVIOManager::write_header(std::ofstream& file)
{
    file << CSV_COLUMN_GRAPH_INDEX << "," << CSV_COLUMN_MOTIF_NUMBER << ","
         << CSV_COLUMN_APPEARANCES << "\n";
}

void CSVIOManager::write_rows(const EnumerationData& data, std::ofstream& file)
{
    for (size_t graph_index = 0U; graph_index < data.size(); ++graph_index)
    {
        for (const auto& entry : data[graph_index])
        {
            file << graph_index << "," << uint128_to_decimal(entry.first) << "," << entry.second
                 << "\n";
        }
    }
}

void CSVIOManager::write_to_file(const EnumerationData& data, const std::string& full_path) const
{
    std::ofstream file(full_path);
    if (!file.is_open())
    {
        throw SgfPathDoesntExistException("Cannot open file for writing: '" + full_path + "'");
    }
    write_header(file);
    write_rows(data, file);
}

void CSVIOManager::insert_row(const std::string& line, EnumerationData& data)
{
    std::istringstream stream(line);
    std::string graph_idx_str, motif_str, appearances_str;
    std::getline(stream, graph_idx_str, ',');
    std::getline(stream, motif_str, ',');
    std::getline(stream, appearances_str);
    const size_t graph_index = std::stoull(graph_idx_str);
    const UInt128 motif_key = decimal_to_uint128(motif_str);
    const uint32_t appearances = static_cast<uint32_t>(std::stoul(appearances_str));
    if (data.size() <= graph_index)
    {
        data.resize(graph_index + 1U);
    }
    data[graph_index][motif_key] = appearances;
}

EnumerationData CSVIOManager::parse_file(std::ifstream& file)
{
    EnumerationData result;
    std::string line;
    std::getline(file, line);  // discard header row
    while (std::getline(file, line))
    {
        insert_row(line, result);
    }
    return result;
}

EnumerationData CSVIOManager::read_from_file(const std::string& full_path) const
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
