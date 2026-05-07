// GCC false-positive: Boost's adjacency_list copy_impl and write_graphml trigger
// -Wmaybe-uninitialized on internal edge iterator optional members. The issue
// is in Boost's template code, not in this file.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"

#include "GraphmlPatternWriter.h"

#include "BoostGraph.h"
#include "GraphmlIOUtils.h"
#include "IOConstants.h"

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graphml.hpp>
#include <boost/property_map/dynamic_property_map.hpp>
#include <cstdint>
#include <fstream>
#include <string>

namespace sgf
{

void GraphmlPatternWriter::write(const BoostGraph& graph, const std::string& path) const
{
    std::ofstream file = GraphmlUtils::open_output_file(path);

    // Build a string-property copy so write_graphml declares attr.type="string".
    // GraphmlGraphReader binds std::string properties; reading attr.type="int" fails
    // with boost::bad_any_cast, so the color values must be serialized as strings.
    IOConstants::GraphmlDirectedBoostGraph string_graph;
    const uint32_t vertex_count = static_cast<uint32_t>(boost::num_vertices(graph));
    for (uint32_t index = 0; index < vertex_count; ++index)
    {
        const IOConstants::GraphmlDirectedBoostGraph::vertex_descriptor v =
            boost::add_vertex(string_graph);
        string_graph[v].m_color = std::to_string(graph[index].m_color);
    }
    for (const BoostGraph::edge_descriptor& edge :
         boost::make_iterator_range(boost::edges(graph)))
    {
        const BoostGraph::vertex_descriptor src = boost::source(edge, graph);
        const BoostGraph::vertex_descriptor dst = boost::target(edge, graph);
        const IOConstants::GraphmlDirectedBoostGraph::edge_descriptor str_edge =
            boost::add_edge(src, dst, string_graph).first;
        string_graph[str_edge].m_color = std::to_string(graph[edge].m_color);
    }

    boost::dynamic_properties dynamic_props;
    dynamic_props.property(
        "color", boost::get(&IOConstants::GraphmlVertexProperties::m_color, string_graph));
    dynamic_props.property(
        "color", boost::get(&IOConstants::GraphmlEdgeProperties::m_color, string_graph));
    boost::write_graphml(file, string_graph, dynamic_props);
}

}  // namespace sgf

#pragma GCC diagnostic pop
