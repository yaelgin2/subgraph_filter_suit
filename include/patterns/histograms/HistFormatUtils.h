#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sgf
{

/**
 * @brief Format a flat vector of uint32_t as "[v0 v1 v2 ...]" for logging.
 * @param data The vector to format.
 * @return String representation of the vector.
 */
inline std::string format_uint_vector(const std::vector<uint32_t>& data)
{
    std::string result = "[";
    for (size_t idx = 0; idx < data.size(); ++idx)
    {
        if (idx > 0U)
        {
            result += " ";
        }
        result += std::to_string(data[idx]);
    }
    result += "]";
    return result;
}

/**
 * @brief Format a 2D histogram matrix as "[[d0c0 d0c1], [d1c0 d1c1]]" for logging.
 * @param data The [depth][color] matrix to format.
 * @return String representation of the matrix.
 */
inline std::string format_hist_matrix(const std::vector<std::vector<uint32_t>>& data)
{
    std::string result = "[";
    for (size_t depth = 0; depth < data.size(); ++depth)
    {
        if (depth > 0U)
        {
            result += ", ";
        }
        result += "[";
        for (size_t color = 0; color < data[depth].size(); ++color)
        {
            if (color > 0U)
            {
                result += " ";
            }
            result += std::to_string(data[depth][color]);
        }
        result += "]";
    }
    result += "]";
    return result;
}

}  // namespace sgf
