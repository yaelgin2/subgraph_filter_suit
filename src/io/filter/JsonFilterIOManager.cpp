#include "JsonFilterIOManager.h"

#include "FilteringUtils.h"
#include "GraphConstructionException.h"
#include "IFilterIOManager.h"
#include "LoggerHandler.h"
#include "SgfPathExistsException.h"

#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/value.hpp>
#include <boost/system/system_error.hpp>
#include <cstddef>
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

JsonFilterIOManager::JsonFilterIOManager(std::string folder, LoggerHandler logger)
    : IFilterIOManager(std::move(folder), std::move(logger))
{
}

std::string JsonFilterIOManager::get_extension() const
{
    return "json";
}

boost::json::object
JsonFilterIOManager::build_json_object(const std::vector<std::string>& filenames,
                                       const FilterResult& results)
{
    boost::json::object json_obj;
    for (size_t idx = 0U; idx < filenames.size(); ++idx)
    {
        json_obj[filenames[idx]] = results[idx];
    }
    return json_obj;
}

void JsonFilterIOManager::write_json_to_file(const boost::json::object& json_obj,
                                             const std::string& full_path)
{
    std::ofstream file(full_path);
    if (!file.is_open())
    {
        throw SgfPathExistsException("Cannot open file for writing: '" + full_path + "'");
    }
    file << boost::json::serialize(json_obj);
    if (file.fail())
    {
        throw SgfPathExistsException("Write error on file: '" + full_path + "'");
    }
}

void JsonFilterIOManager::write_to_file(const std::vector<std::string>& filenames,
                                        const FilterResult& results,
                                        const std::string& full_path) const
{
    const boost::json::object json_obj = build_json_object(filenames, results);
    write_json_to_file(json_obj, full_path);
}

std::unordered_map<std::string, bool>
JsonFilterIOManager::parse_json_file(const std::string& full_path)
{
    std::ifstream file(full_path);
    if (!file.is_open())
    {
        throw SgfPathExistsException("Cannot open file for reading: '" + full_path + "'");
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    const boost::json::value parsed = boost::json::parse(buffer.str());
    const boost::json::object& json_obj = parsed.as_object();
    std::unordered_map<std::string, bool> result;
    for (const auto& entry : json_obj)
    {
        result[std::string(entry.key())] = entry.value().as_bool();
    }
    return result;
}

std::unordered_map<std::string, bool>
JsonFilterIOManager::read_from_file(const std::string& full_path) const
{
    try
    {
        return parse_json_file(full_path);
    }
    catch (const SgfPathExistsException&)
    {
        throw;
    }
    catch (const std::bad_alloc&)
    {
        throw GraphConstructionException("Memory allocation failed reading JSON filter results: '" +
                                         full_path + "'");
    }
    catch (const std::invalid_argument& ex)
    {
        throw GraphConstructionException("Malformed JSON in '" + full_path + "': " + ex.what());
    }
    catch (const boost::system::system_error& ex)
    {
        throw GraphConstructionException("Failed to parse JSON in '" + full_path +
                                         "': " + ex.what());
    }
}

}  // namespace sgf
