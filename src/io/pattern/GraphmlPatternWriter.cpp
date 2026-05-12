#include "GraphmlPatternWriter.h"

#include "BoostGraph.h"
#include "GraphmlIOUtils.h"
#include "IOConstants.h"

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/property_map/dynamic_property_map.hpp>
#include <cstdint>
#include <fstream>
#include <string>
#include <tuple>

// IMPORTANT: isolate Boost GraphML to avoid GCC 14 false-positive
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#include <boost/graph/graphml.hpp>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

namespace sgf
{

void GraphmlPatternWriter::build_vertices(const BoostGraph& graph,
                                          IOConstants::GraphmlDirectedBoostGraph& out)
{
    const boost::graph_traits<BoostGraph>::vertices_size_type vertex_count =
        boost::num_vertices(graph);

    for (uint32_t i = 0; i < static_cast<uint32_t>(vertex_count); ++i)
    {
        const boost::graph_traits<IOConstants::GraphmlDirectedBoostGraph>::vertex_descriptor
            new_vertex = boost::add_vertex(out);
        out[new_vertex].m_color = std::to_string(graph[i].m_color);
    }
}

void GraphmlPatternWriter::build_edges(const BoostGraph& graph,
                                       IOConstants::GraphmlDirectedBoostGraph& out)
{
    const boost::graph_traits<BoostGraph>::vertices_size_type vertex_count =
        boost::num_vertices(graph);

    for (uint32_t src = 0; src < static_cast<uint32_t>(vertex_count); ++src)
    {
        boost::graph_traits<BoostGraph>::out_edge_iterator first;
        boost::graph_traits<BoostGraph>::out_edge_iterator second;
        std::tie(first, second) = boost::out_edges(src, graph);

        for (boost::graph_traits<BoostGraph>::out_edge_iterator it = first; it != second; ++it)
        {
            const boost::graph_traits<BoostGraph>::edge_descriptor edge = *it;
            const boost::graph_traits<BoostGraph>::vertex_descriptor dst =
                boost::target(edge, graph);

            boost::graph_traits<IOConstants::GraphmlDirectedBoostGraph>::edge_descriptor new_edge;
            bool edge_added = false;
            std::tie(new_edge, edge_added) = boost::add_edge(src, dst, out);
            static_cast<void>(edge_added);

            out[new_edge].m_color = std::to_string(graph[edge].m_color);
        }
    }
}

void GraphmlPatternWriter::write(const BoostGraph& graph, const std::string& path) const
{
    std::ofstream file = GraphmlUtils::open_output_file(path);

    IOConstants::GraphmlDirectedBoostGraph string_graph;

    build_vertices(graph, string_graph);
    build_edges(graph, string_graph);

    boost::dynamic_properties props;

    props.property("color",
                   boost::get(&IOConstants::GraphmlVertexProperties::m_color, string_graph));

    props.property("color", boost::get(&IOConstants::GraphmlEdgeProperties::m_color, string_graph));

    boost::write_graphml(file, string_graph, props);
}

}  // namespace sgf
