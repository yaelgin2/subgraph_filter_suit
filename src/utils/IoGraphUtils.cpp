#include "IoGraphUtils.h"

#include "GraphConstructionException.h"
#include "SgfPathDoesntExistException.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace sgf
{

std::ifstream IoGraphUtils::open_file(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        throw SgfPathDoesntExistException("cannot open file: " + path);
    }
    return file;
}

std::unordered_map<uint32_t, uint32_t>
IoGraphUtils::build_consecutive_index_map(const std::unordered_map<uint32_t, uint32_t>& color_by_id)
{
    std::vector<uint32_t> sorted_ids;
    sorted_ids.reserve(color_by_id.size());
    for (const auto& entry : color_by_id)
    {
        sorted_ids.push_back(entry.first);
    }
    std::sort(sorted_ids.begin(), sorted_ids.end());
    std::unordered_map<uint32_t, uint32_t> consecutive_index_by_original_id;
    consecutive_index_by_original_id.reserve(sorted_ids.size());
    uint32_t consecutive_index = 0;
    for (const uint32_t original_id : sorted_ids)
    {
        if (consecutive_index == std::numeric_limits<uint32_t>::max())
        {
            throw GraphConstructionException("vertex count exceeded maximum of " +
                                             std::to_string(std::numeric_limits<uint32_t>::max()));
        }
        consecutive_index_by_original_id.emplace(original_id, consecutive_index);
        ++consecutive_index;
    }
    return consecutive_index_by_original_id;
}

}  // namespace sgf
