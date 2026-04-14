#include "GraphUtils.h"

#include "Constants.h"
#include "GraphConstructionException.h"

#include <cstdint>
#include <map>
#include <string>

namespace sgf
{

uint32_t GraphUtils::map_color_string(const std::string& color_str,
                                      std::map<std::string, uint32_t>& color_map)
{
    const std::map<std::string, uint32_t>::iterator color_map_iterator = color_map.find(color_str);
    if (color_map_iterator != color_map.end())
    {
        return color_map_iterator->second;
    }
    const uint32_t new_id = static_cast<uint32_t>(color_map.size());
    if (new_id > SgfConstants::MAX_VERTEX_COLOR)
    {
        throw GraphConstructionException("too many distinct colors: exceeds maximum of " +
                                         std::to_string(SgfConstants::MAX_VERTEX_COLOR));
    }
    color_map.emplace(color_str, new_id);
    return new_id;
}

uint32_t GraphUtils::extract_color(const uint32_t color,
                                   std::map<std::string, uint32_t>& /*color_map*/)
{
    return color;
}

uint32_t GraphUtils::extract_color(const std::string& color_str,
                                   std::map<std::string, uint32_t>& color_map)
{
    return map_color_string(color_str, color_map);
}

}  // namespace sgf
