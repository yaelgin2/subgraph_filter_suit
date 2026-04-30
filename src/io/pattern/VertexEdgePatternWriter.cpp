#include "VertexEdgePatternWriter.h"

#include "VertexEdgeUtils.h"
#include "IOConstants.h"

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_traits.hpp>
#include <cstdint>
#include <fstream>
#include <string>

namespace sgf
{

void VertexEdgePatternWriter::write_node_labels(const BoostGraph& graph,
                                                const std::string& base_path)
{
    std::ofstream file = VertexEdgeUtils::open_file_for_writing(base_path + IOConstants::NODE_LABELS_SUFFIX);
    const std::pair<boost::graph_traits<BoostGraph>::vertex_iterator,
                    boost::graph_traits<BoostGraph>::vertex_iterator>
        vertex_range = boost::vertices(graph);
    for (boost::graph_traits<BoostGraph>::vertex_iterator it = vertex_range.first;
         it != vertex_range.second; ++it)
    {
        file << static_cast<uint32_t>(*it) << ' ' << graph[*it].m_color << '\n';
    }
}

void VertexEdgePatternWriter::write_edge_file(const BoostGraph& graph,
                                              const std::string& base_path)
{
    std::ofstream file = VertexEdgeUtils::open_file_for_writing(base_path + IOConstants::EDGE_SUFFIX);
    const std::pair<boost::graph_traits<BoostGraph>::edge_iterator,
                    boost::graph_traits<BoostGraph>::edge_iterator>
        edge_range = boost::edges(graph);
    for (boost::graph_traits<BoostGraph>::edge_iterator it = edge_range.first;
         it != edge_range.second; ++it)
    {
        file << static_cast<uint32_t>(boost::source(*it, graph)) << ' '
             << static_cast<uint32_t>(boost::target(*it, graph)) << ' ' << graph[*it].m_color
             << '\n';
    }
}

void VertexEdgePatternWriter::write(const BoostGraph& graph, const std::string& path) const
{
    write_node_labels(graph, path);
    write_edge_file(graph, path);
}

}  // namespace sgf
