#include "GraphmlGraphReader.h"

#include "ColoredGraph.h"
#include "GraphConstructionException.h"
#include "GraphUtils.h"
#include "ILogger.h"
#include "LogLevel.h"
#include "SgfPathDoesntExistException.h"

#include <boost/any/bad_any_cast.hpp>
#include <boost/graph/graphml.hpp>
#include <boost/property_map/dynamic_property_map.hpp>
#include <boost/property_tree/detail/xml_parser_error.hpp>
#include <boost/property_tree/exceptions.hpp>
#include <cstdint>
#include <exception>
#include <fstream>
#include <map>
#include <memory>
#include <string>

namespace sgf
{

std::ifstream GraphmlGraphReader::open_file(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        throw SgfPathDoesntExistException("cannot open file: " + path);
    }
    return file;
}

[[noreturn]] void GraphmlGraphReader::rethrow_as_construction_error(const std::string& path,
                                                                    const std::exception& exc)
{
    throw GraphConstructionException("Failed to read graphml '" + path + "': " + exc.what());
}

bool GraphmlGraphReader::detect_is_directed(const std::string& path)
{
    std::ifstream file = open_file(path);
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

template <typename GraphType>
void GraphmlGraphReader::read_graphml_from_file_into_boost_graph(const std::string& path,
                                                                 GraphType& boost_graph)
{
    std::ifstream file = open_file(path);
    boost::dynamic_properties dynamic_props(boost::ignore_other_properties);
    dynamic_props.property("color", boost::get(&GraphmlVertexProperties::m_color, boost_graph));
    dynamic_props.property("color", boost::get(&GraphmlEdgeProperties::m_color, boost_graph));
    boost::read_graphml(file, boost_graph, dynamic_props);
}

ColoredGraph GraphmlGraphReader::read_graphml_from_file(const std::string& path,
                                                        const bool file_is_directed,
                                                        const bool is_directed,
                                                        std::map<std::string, uint32_t>& color_map)
{
    if (file_is_directed)
    {
        GraphmlDirectedBoostGraph boost_graph;
        read_graphml_from_file_into_boost_graph(path, boost_graph);
        return GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, is_directed,
                                                                color_map);
    }
    GraphmlUndirectedBoostGraph boost_graph;
    read_graphml_from_file_into_boost_graph(path, boost_graph);
    return GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, is_directed, color_map);
}

void GraphmlGraphReader::log_read_result(const std::shared_ptr<ILogger>& logger,
                                         const std::string& path, const bool file_is_directed,
                                         const bool is_directed,
                                         const std::map<std::string, uint32_t>& color_map)
{
    if (logger == nullptr)
    {
        return;
    }
    if (file_is_directed != is_directed)
    {
        const std::string file_type = file_is_directed ? "directed" : "undirected";
        const std::string param_type = is_directed ? "directed" : "undirected";
        logger->log(LogLevel::WARNING, "graphml file '" + path + "' declares " + file_type +
                                           " but caller requested " + param_type +
                                           "; using caller parameter");
    }
    std::string color_log = "color map for '" + path + "':";
    for (const auto& [color_str, color_id] : color_map)
    {
        color_log += " '" + color_str + "'=" + std::to_string(color_id);
    }
    logger->log(LogLevel::INFO, color_log);
}

ColoredGraph GraphmlGraphReader::read(const std::string& path, const bool is_directed,
                                      const std::weak_ptr<ILogger> logger) const
{
    try
    {
        const bool file_is_directed = detect_is_directed(path);
        if (!file_is_directed && is_directed)
        {
            throw GraphConstructionException(
                "Failed to read graphml - requested a directed graph when the graphml is "
                "undirected.");
        }
        std::map<std::string, uint32_t> color_map;
        const ColoredGraph graph =
            read_graphml_from_file(path, file_is_directed, is_directed, color_map);
        log_read_result(logger.lock(), path, file_is_directed, is_directed, color_map);
        return graph;
    }
    catch (const boost::bad_any_cast& exc)
    {
        rethrow_as_construction_error(path, exc);
    }
    catch (const boost::property_tree::ptree_bad_path& exc)
    {
        rethrow_as_construction_error(path, exc);
    }
    catch (const boost::parse_error& exc)
    {
        rethrow_as_construction_error(path, exc);
    }
    catch (const boost::property_tree::xml_parser::xml_parser_error& exc)
    {
        rethrow_as_construction_error(path, exc);
    }
}

}  // namespace sgf
