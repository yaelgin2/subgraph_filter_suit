#include "VertexEdgePatternWriter.h"

#include "BoostGraph.h"
#include "IOConstants.h"
#include "VertexEdgeUtils.h"

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/range/iterator_range.hpp>
#include <cstdint>
#include <fstream>
#include <string>

namespace sgf

{

void VertexEdgePatternWriter::write_node_labels(const BoostGraph& graph,
                                                const std::string& base_path)
{
    std::ofstream file =
        VertexEdgeUtils::open_file_for_writing(base_path + IOConstants::NODE_LABELS_SUFFIX);
    for (const boost::graph_traits<BoostGraph>::vertex_descriptor& vertex :
         boost::make_iterator_range(boost::vertices(graph)))
    {
        file << static_cast<uint32_t>(vertex) << ' ' << graph[vertex].m_color << '\n';
    }
}

void VertexEdgePatternWriter::write_edge_file(const BoostGraph& graph, const std::string& base_path)
{
    std::ofstream file =
        VertexEdgeUtils::open_file_for_writing(base_path + IOConstants::EDGE_SUFFIX);
    for (const boost::graph_traits<BoostGraph>::edge_descriptor& edge :
         boost::make_iterator_range(boost::edges(graph)))
    {
        file << static_cast<uint32_t>(boost::source(edge, graph)) << ' '
             << static_cast<uint32_t>(boost::target(edge, graph)) << ' ' << graph[edge].m_color
             << '\n';
    }
}

void VertexEdgePatternWriter::write(const BoostGraph& graph, const std::string& path) const
{
    write_node_labels(graph, path);
    write_edge_file(graph, path);
}

}  // namespace sgf
