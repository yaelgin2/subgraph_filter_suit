#include "VertexEdgeUtils.h"

#include "GraphConstructionException.h"
#include "SgfPathDoesntExistException.h"

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>

namespace sgf
{

void VertexEdgeUtils::throw_if_extra_tokens(std::istringstream& stream, const std::string& context,
                                            const std::string& line, const uint32_t expected_count)
{
    uint32_t extra = 0;
    if (stream >> extra)
    {
        throw GraphConstructionException(context + ": '" + line + "' (too many tokens, expected " +
                                         std::to_string(expected_count) + ")");
    }
}

std::ofstream VertexEdgeUtils::open_file_for_writing(const std::string& path)
{
    std::ofstream file(path);
    if (!file.is_open())
    {
        throw SgfPathDoesntExistException("Cannot open for writing: '" + path + "'");
    }
    return file;
}

}  // namespace sgf
