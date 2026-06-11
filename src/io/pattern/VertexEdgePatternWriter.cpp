#include "VertexEdgePatternWriter.h"

#include "BoostGraph.h"
#include "IOConstants.h"
#include "VertexEdgeUtils.h"

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/range/iterator_range_core.hpp>
#include <cstdint>
#include <fstream>
#include <string>
#include <tuple>

namespace sgf
{

std::string VertexEdgePatternWriter::get_file_extension() const
{
    return "";
}

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

void VertexEdgePatternWriter::write_edge_file(const BoostGraph& graph,
                                              const std::string& base_path,
                                              const bool is_directed)
{
    std::ofstream file =
        VertexEdgeUtils::open_file_for_writing(base_path + IOConstants::EDGE_SUFFIX);
    const boost::graph_traits<BoostGraph>::vertices_size_type vertex_count =
        boost::num_vertices(graph);
    for (uint32_t src = 0; src < static_cast<uint32_t>(vertex_count); ++src)
    {
        boost::graph_traits<BoostGraph>::out_edge_iterator first;
        boost::graph_traits<BoostGraph>::out_edge_iterator last;
        std::tie(first, last) = boost::out_edges(src, graph);
        for (boost::graph_traits<BoostGraph>::out_edge_iterator it = first; it != last; ++it)
        {
            const boost::graph_traits<BoostGraph>::edge_descriptor edge = *it;
            const uint32_t dst = static_cast<uint32_t>(boost::target(edge, graph));
            if (!is_directed && src > dst && boost::edge(dst, src, graph).second)
            {
                continue;
            }
            file << src << ' ' << dst << ' ' << graph[edge].m_color << '\n';
        }
    }
}

void VertexEdgePatternWriter::do_write(const BoostGraph& graph, const std::string& path,
                                       const bool is_directed) const
{
    write_node_labels(graph, path);
    write_edge_file(graph, path, is_directed);
}

}  // namespace sgf
