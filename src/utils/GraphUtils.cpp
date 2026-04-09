#include "GraphUtils.h"

#include "BoostGraph.h"
#include "ColoredGraph.h"
#include "Constants.h"
#include "GraphConstructionException.h"

#include <algorithm>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/range/iterator_range_core.hpp>
#include <cstdint>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace sgf
{

ColoredGraph GraphUtils::convert_boost_graph_to_colored_graph(const BoostGraph& boost_graph,
                                                              bool is_directed)
{
    const uint32_t num_vertices = static_cast<uint32_t>(boost::num_vertices(boost_graph));
    const std::vector<int32_t> vertex_colors = extract_vertex_colors(boost_graph);
    if (has_edge_colors(boost_graph))
    {
        std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> colored_edges =
            extract_colored_edges(boost_graph);
        return {num_vertices, colored_edges, vertex_colors, is_directed};
    }
    std::vector<std::pair<uint32_t, uint32_t>> edges = extract_uncolored_edges(boost_graph);
    return {num_vertices, edges, vertex_colors, is_directed};
}

std::vector<int32_t> GraphUtils::extract_vertex_colors(const BoostGraph& boost_graph)
{
    std::vector<int32_t> vertex_colors;
    vertex_colors.reserve(boost::num_vertices(boost_graph));
    for (const auto& vertex_desc : boost::make_iterator_range(boost::vertices(boost_graph)))
    {
        const uint32_t color = boost::get(&VertexProperties::m_color, boost_graph, vertex_desc);
        if (color > SgfConstants::MAX_VERTEX_COLOR)
        {
            throw GraphConstructionException("vertex color " + std::to_string(color) +
                                             " exceeds maximum allowed value");
        }
        vertex_colors.push_back(static_cast<int32_t>(color));
    }
    return vertex_colors;
}

std::vector<std::pair<uint32_t, uint32_t>>
GraphUtils::extract_uncolored_edges(const BoostGraph& boost_graph)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    edges.reserve(boost::num_edges(boost_graph));
    for (const auto& edge_desc : boost::make_iterator_range(boost::edges(boost_graph)))
    {
        edges.emplace_back(static_cast<uint32_t>(boost::source(edge_desc, boost_graph)),
                           static_cast<uint32_t>(boost::target(edge_desc, boost_graph)));
    }
    return edges;
}

std::vector<std::tuple<uint32_t, uint32_t, uint32_t>>
GraphUtils::extract_colored_edges(const BoostGraph& boost_graph)
{
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> edges;
    edges.reserve(boost::num_edges(boost_graph));
    for (const auto& edge_desc : boost::make_iterator_range(boost::edges(boost_graph)))
    {
        edges.emplace_back(static_cast<uint32_t>(boost::source(edge_desc, boost_graph)),
                           static_cast<uint32_t>(boost::target(edge_desc, boost_graph)),
                           boost::get(&EdgeProperties::m_color, boost_graph, edge_desc));
    }
    return edges;
}

bool GraphUtils::has_edge_colors(const BoostGraph& boost_graph)
{
    const std::pair<boost::graph_traits<BoostGraph>::edge_iterator,
                    boost::graph_traits<BoostGraph>::edge_iterator>
        edge_range = boost::edges(boost_graph);
    return std::any_of(edge_range.first, edge_range.second,
                       [&](const boost::graph_traits<BoostGraph>::edge_descriptor& edge_desc)
                       {
                           return boost::get(&EdgeProperties::m_color, boost_graph, edge_desc) !=
                                  0U;
                       });
}

}  // namespace sgf
