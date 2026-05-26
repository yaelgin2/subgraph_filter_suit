#include "CSVFilterOutputManager.h"

#include "FilteringUtils.h"
#include "IFilterOutputManager.h"
#include "LoggerHandler.h"
#include "SgfPathExistsException.h"

#include <cstddef>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace sgf
{

CSVFilterOutputManager::CSVFilterOutputManager(std::string folder, LoggerHandler logger)
    : IFilterOutputManager(std::move(folder), std::move(logger))
{
}

std::string CSVFilterOutputManager::get_extension() const
{
    return "csv";
}

void CSVFilterOutputManager::write_to_file(const std::vector<std::string>& filenames,
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

}  // namespace sgf
