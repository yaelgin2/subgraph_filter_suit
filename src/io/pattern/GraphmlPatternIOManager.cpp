// GCC false-positive: Boost's adjacency_list copy_impl and write_graphml trigger
// -Wmaybe-uninitialized on internal edge iterator optional members. The issue
// is in Boost's template code, not in this file.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"

#include "GraphmlPatternIOManager.h"

#include "BoostGraph.h"
#include "ColoredGraph.h"
#include "GraphConstructionException.h"
#include "GraphUtils.h"
#include "SgfPathDoesntExistException.h"

#include <boost/any/bad_any_cast.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graphml.hpp>
#include <boost/property_map/dynamic_property_map.hpp>
#include <boost/property_tree/detail/xml_parser_error.hpp>
#include <boost/property_tree/exceptions.hpp>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <string>

namespace sgf
{

namespace
{

/**
 * @brief Reads a GraphML file into a Boost graph using the "color" attribute.
 *
 * @tparam GraphType A Boost adjacency_list type with StringVertexProperties and
 *         StringEdgeProperties bundles.
 * @param path Path to the GraphML file.
 * @param boost_graph Graph to populate.
 * @throws SgfPathDoesntExistException if the file cannot be opened.
 */
template <typename GraphType>
void read_into_boost_graph(const std::string& path, GraphType& boost_graph)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        throw SgfPathDoesntExistException("cannot open file: " + path);
    }
    boost::dynamic_properties dynamic_props(boost::ignore_other_properties);
    dynamic_props.property("color", boost::get(&StringVertexProperties::m_color, boost_graph));
    dynamic_props.property("color", boost::get(&StringEdgeProperties::m_color, boost_graph));
    boost::read_graphml(file, boost_graph, dynamic_props);
}

}  // namespace

std::ofstream GraphmlPatternIOManager::open_output_file(const std::string& path)
{
    std::ofstream file(path);
    if (!file.is_open())
    {
        throw SgfPathDoesntExistException("cannot open file for writing: " + path);
    }
    return file;
}

bool GraphmlPatternIOManager::detect_is_directed(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        throw SgfPathDoesntExistException("cannot open file: " + path);
    }
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

[[noreturn]] void GraphmlPatternIOManager::rethrow_as_construction_error(
    const std::string& path, const std::exception& exc)
{
    throw GraphConstructionException("Failed to read graphml '" + path + "': " + exc.what());
}

void GraphmlPatternIOManager::write(const BoostGraph& graph, const std::string& path) const
{
    std::ofstream file = open_output_file(path);
    // Boost's dynamic_properties::property() compiles the put() path unconditionally,
    // requiring a mutable graph even when write_graphml only reads properties.
    BoostGraph graph_copy = graph;
    boost::dynamic_properties dynamic_props;
    dynamic_props.property("color", boost::get(&VertexProperties::m_color, graph_copy));
    dynamic_props.property("color", boost::get(&EdgeProperties::m_color, graph_copy));
    boost::write_graphml(file, graph_copy, dynamic_props);
}

ColoredGraph GraphmlPatternIOManager::read(const std::string& path) const
{
    // SgfPathDoesntExistException from detect_is_directed / read_into_boost_graph
    // propagates directly to the caller as documented in IPatternIOManager::read().
    // Only Boost parse and cast errors are re-wrapped as GraphConstructionException.
    try
    {
        const bool is_directed = detect_is_directed(path);
        if (is_directed)
        {
            DirectedStringBoostGraph boost_graph;
            read_into_boost_graph(path, boost_graph);
            return GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, true);
        }
        UndirectedStringBoostGraph boost_graph;
        read_into_boost_graph(path, boost_graph);
        return GraphUtils::convert_boost_graph_to_colored_graph(boost_graph, false);
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
    // Unreachable: every catch arm calls [[noreturn]] rethrow_as_construction_error.
    // std::terminate() satisfies compilers that do not propagate [[noreturn]] across
    // indirect calls (e.g. MSVC), preventing undefined behavior from a missing return.
    std::terminate();
}

}  // namespace sgf
