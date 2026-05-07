#include "GraphmlIOUtils.h"

#include "GraphConstructionException.h"
#include "SgfPathDoesntExistException.h"

#include <exception>
#include <fstream>
#include <string>

namespace sgf
{
namespace graphml_io_utils
{

std::ifstream open_file(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        throw SgfPathDoesntExistException("cannot open file: " + path);
    }
    return file;
}

std::ofstream open_output_file(const std::string& path)
{
    std::ofstream file(path);
    if (!file.is_open())
    {
        throw SgfPathDoesntExistException("cannot open file for writing: " + path);
    }
    return file;
}

bool detect_is_directed(const std::string& path)
{
    std::ifstream file = open_file(path);
    // NOLINTNEXTLINE(misc-const-correctness) -- std::getline writes to line
    std::string line;
    while (std::getline(file, line))
    {
        if (line.find("edgedefault=") != std::string::npos)
        {
            return line.find("edgedefault=\"directed\"") != std::string::npos;
        }
    }
    return true;
}

[[noreturn]] void rethrow_as_construction_error(const std::string& path,
                                                const std::exception& exc)
{
    throw GraphConstructionException("Failed to read graphml '" + path + "': " + exc.what());
}

}  // namespace graphml_io_utils
}  // namespace sgf
