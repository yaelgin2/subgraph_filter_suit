#include "CSVCacheWriter.h"

#include "Int128.h"
#include "SgfPathDoesntExistException.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <utility>

namespace sgf
{

CSVCacheWriter::CSVCacheWriter(std::string folder, std::string base_filename)
    : ICacheWriter(std::move(folder), std::move(base_filename))
{
}

std::string CSVCacheWriter::get_extension() const
{
    return "csv";
}

std::string CSVCacheWriter::uint128_to_string(UInt128 value)
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

void CSVCacheWriter::write_header(std::ofstream& file)
{
    file << CSV_COLUMN_GRAPH_INDEX << ","
         << CSV_COLUMN_MOTIF_NUMBER << ","
         << CSV_COLUMN_APPEARANCES << "\n";
}

void CSVCacheWriter::write_rows(const EnumerationData& data, std::ofstream& file)
{
    for (size_t graph_index = 0U; graph_index < data.size(); ++graph_index)
    {
        for (const auto& entry : data[graph_index])
        {
            file << graph_index << ","
                 << uint128_to_string(entry.first) << ","
                 << entry.second << "\n";
        }
    }
}

void CSVCacheWriter::write_to_file(const EnumerationData& data, const std::string& full_path) const
{
    std::ofstream file(full_path);
    if (!file.is_open())
    {
        throw SgfPathDoesntExistException("Cannot open file for writing: '" + full_path + "'");
    }
    write_header(file);
    write_rows(data, file);
}

}  // namespace sgf
