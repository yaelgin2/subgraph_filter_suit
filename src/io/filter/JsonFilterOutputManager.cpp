#include "JsonFilterOutputManager.h"

#include "FilteringUtils.h"
#include "IFilterOutputManager.h"
#include "SgfPathDoesntExistException.h"

#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <cstddef>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace sgf
{

JsonFilterOutputManager::JsonFilterOutputManager(std::string folder, std::string base_filename)
    : IFilterOutputManager(std::move(folder), std::move(base_filename))
{
}

std::string JsonFilterOutputManager::get_extension() const
{
    return "json";
}

void JsonFilterOutputManager::write_to_file(const std::vector<std::string>& filenames,
                                            const FilterResult& results,
                                            const std::string& full_path) const
{
    boost::json::object json_obj;
    for (size_t idx = 0U; idx < filenames.size(); ++idx)
    {
        json_obj[filenames[idx]] = results[idx];
    }

    std::ofstream file(full_path);
    if (!file.is_open())
    {
        throw SgfPathDoesntExistException("Cannot open file for writing: '" + full_path + "'");
    }
    file << boost::json::serialize(json_obj);
    if (file.fail())
    {
        throw SgfPathDoesntExistException("Write error on file: '" + full_path + "'");
    }
}

}  // namespace sgf
