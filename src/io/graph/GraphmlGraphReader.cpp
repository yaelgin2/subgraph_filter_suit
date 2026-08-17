#include "GraphmlGraphReader.h"

#include "ColoredGraph.h"
#include "GraphConstructionException.h"
#include "GraphUtils.h"
#include "GraphmlIOUtils.h"
#include "IOConstants.h"
#include "IoGraphUtils.h"
#include "LogLevel.h"
#include "LoggerHandler.h"

#include <boost/any/bad_any_cast.hpp>
#include <boost/graph/graphml.hpp>
#include <boost/property_map/dynamic_property_map.hpp>
#include <boost/property_tree/detail/xml_parser_error.hpp>
#include <boost/property_tree/exceptions.hpp>
#include <cstdint>
#include <exception>
#include <fstream>
#include <map>
#include <string>
#include <utility>

namespace sgf
{

GraphmlGraphReader::GraphmlGraphReader(std::map<std::string, uint32_t> initial_color_map)
    : m_color_map(std::move(initial_color_map))
{
}

const std::map<std::string, uint32_t>& GraphmlGraphReader::get_color_map() const
{
    return m_color_map;
}

template <typename GraphType>
void GraphmlGraphReader::read_graphml_from_file_into_boost_graph(const std::string& path,
                                                                 GraphType& boost_graph)
{
    std::ifstream file = IoGraphUtils::open_file(path);
    boost::dynamic_properties dynamic_props(boost::ignore_other_properties);
    dynamic_props.property("color",
                           boost::get(&IOConstants::GraphmlVertexProperties::m_color, boost_graph));
    dynamic_props.property("color",
                           boost::get(&IOConstants::GraphmlEdgeProperties::m_color, boost_graph));
    boost::read_graphml(file, boost_graph, dynamic_props);
}

ColoredGraph GraphmlGraphReader::read_graphml_from_file(const std::string& path,
                                                        const bool file_is_directed,
                                                        const bool is_directed,
                                                        std::map<std::string, uint32_t>& color_map,
                                                        const LoggerHandler& logger)
{
    if (file_is_directed)
    {
        IOConstants::GraphmlDirectedBoostGraph boost_graph;
        read_graphml_from_file_into_boost_graph(path, boost_graph);
        return GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, is_directed, color_map,
                                                                logger);
    }
    IOConstants::GraphmlUndirectedBoostGraph boost_graph;
    read_graphml_from_file_into_boost_graph(path, boost_graph);
    return GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, is_directed, color_map,
                                                            logger);
}

void GraphmlGraphReader::log_read_result(const LoggerHandler& logger, const std::string& path,
                                         const bool file_is_directed, const bool is_directed,
                                         const std::map<std::string, uint32_t>& color_map)
{
    if (logger.is_null())
    {
        return;
    }
    if (file_is_directed != is_directed)
    {
        const std::string file_type = file_is_directed ? "directed" : "undirected";
        const std::string param_type = is_directed ? "directed" : "undirected";
        logger.log(LogLevel::WARNING, "graphml file '" + path + "' declares " + file_type +
                                          " but caller requested " + param_type +
                                          "; using caller parameter");
    }
    std::string color_log = "color map for '" + path + "':";
    for (const auto& [color_str, color_id] : color_map)
    {
        color_log += " '" + color_str + "'=" + std::to_string(color_id);
    }
    logger.log(LogLevel::INFO, color_log);
}

ColoredGraph GraphmlGraphReader::read(const std::string& path, const bool is_directed,
                                      const LoggerHandler& logger) const
{
    try
    {
        const bool file_is_directed = GraphmlUtils::detect_is_directed(path);
        if (!file_is_directed && is_directed)
        {
            throw GraphConstructionException(
                "Failed to read graphml - requested a directed graph when the graphml is "
                "undirected.");
        }
        const ColoredGraph graph =
            read_graphml_from_file(path, file_is_directed, is_directed, m_color_map, logger);
        log_read_result(logger, path, file_is_directed, is_directed, m_color_map);
        return graph;
    }
    catch (const boost::bad_any_cast& exc)
    {
        GraphmlUtils::rethrow_as_construction_error(path, exc);
    }
    catch (const boost::property_tree::ptree_bad_path& exc)
    {
        GraphmlUtils::rethrow_as_construction_error(path, exc);
    }
    catch (const boost::parse_error& exc)
    {
        GraphmlUtils::rethrow_as_construction_error(path, exc);
    }
    catch (const boost::property_tree::xml_parser::xml_parser_error& exc)
    {
        GraphmlUtils::rethrow_as_construction_error(path, exc);
    }
    // Unreachable: every catch arm calls [[noreturn]] rethrow_as_construction_error.
    // std::terminate() satisfies compilers that do not propagate [[noreturn]] across
    // indirect calls (e.g. MSVC), preventing undefined behavior from a missing return.
    std::terminate();
}

}  // namespace sgf
