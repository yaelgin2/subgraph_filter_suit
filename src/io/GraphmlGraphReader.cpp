#include "GraphmlGraphReader.h"

#include "BoostGraph.h"
#include "ColoredGraph.h"
#include "GraphConstructionException.h"
#include "GraphUtils.h"
#include "SgfPathDoesntExistException.h"

#include <boost/graph/graphml.hpp>
#include <boost/property_map/dynamic_property_map.hpp>
#include <boost/property_tree/detail/xml_parser_error.hpp>
#include <exception>
#include <fstream>
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
    throw GraphConstructionException("failed to read graphml '" + path + "': " + exc.what());
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

ColoredGraph GraphmlGraphReader::read_graphml_from_file(const std::string& path,
                                                        const bool is_directed)
{
    std::ifstream file = open_file(path);
    BoostGraph boost_graph;
    boost::dynamic_properties dynamic_props(boost::ignore_other_properties);
    dynamic_props.property("color", boost::get(&VertexProperties::m_color, boost_graph));
    dynamic_props.property("color", boost::get(&EdgeProperties::m_color, boost_graph));
    boost::read_graphml(file, boost_graph, dynamic_props);
    return GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, is_directed);
}

ColoredGraph GraphmlGraphReader::read(const std::string& path) const
{
    try
    {
        const bool is_directed = detect_is_directed(path);
        return read_graphml_from_file(path, is_directed);
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
