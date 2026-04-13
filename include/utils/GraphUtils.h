#pragma once

#include "ColoredGraph.h"
#include "Constants.h"
#include "GraphConstructionException.h"
#include "GraphmlGraphReader.h"

#include <algorithm>
#include <boost/graph/graph_traits.hpp>
#include <boost/range/iterator_range_core.hpp>
#include <cstdint>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace sgf
{

class GraphUtils
{
public:
    /**
     * @brief Converts a Boost graph into a ColoredGraph, populating @p color_map.
     *
     * Supports both @c uint32_t and @c std::string bundled color properties.
     * String colors are mapped to sequential uint32_t IDs in order of first
     * appearance; uint32_t colors pass through unchanged. Throws if the number
     * of distinct string colors exceeds @c MAX_VERTEX_COLOR.
     *
     * @tparam GraphType A Boost adjacency_list type.
     * @tparam VertexPropertyType Bundled vertex property struct. Defaults to the
     *         vertex bundle of @p GraphType.
     * @tparam EdgePropertyType Bundled edge property struct. Defaults to the
     *         edge bundle of @p GraphType.
     * @param boost_graph The source Boost graph.
     * @param is_directed Whether to build a directed ColoredGraph.
     * @param color_map Accumulates the string→uint mapping built during conversion.
     * @return The resulting ColoredGraph.
     * @throws GraphConstructionException if too many distinct colors or a
     *         uint32_t color exceeds @c MAX_VERTEX_COLOR.
     */
    template <typename GraphType,
              typename VertexPropertyType = typename boost::vertex_bundle_type<GraphType>::type,
              typename EdgePropertyType = typename boost::edge_bundle_type<GraphType>::type>
    static ColoredGraph
    convert_boost_graph_to_colored_graph(const GraphType& boost_graph, bool is_directed,
                                         std::map<std::string, uint32_t>& color_map);

    /**
     * @brief Converts a Boost graph into a ColoredGraph (no color map output).
     *
     * Convenience overload for callers that do not need the color map.
     * String colors are still mapped to sequential IDs; the map is discarded.
     *
     * @tparam GraphType A Boost adjacency_list type.
     * @tparam VertexPropertyType Bundled vertex property struct.
     * @tparam EdgePropertyType Bundled edge property struct.
     * @param boost_graph The source Boost graph.
     * @param is_directed Whether to build a directed ColoredGraph.
     * @return The resulting ColoredGraph.
     * @throws GraphConstructionException if too many distinct colors or a
     *         uint32_t color exceeds @c MAX_VERTEX_COLOR.
     */
    template <typename GraphType,
              typename VertexPropertyType = typename boost::vertex_bundle_type<GraphType>::type,
              typename EdgePropertyType = typename boost::edge_bundle_type<GraphType>::type>
    static ColoredGraph convert_boost_graph_to_colored_graph(const GraphType& boost_graph,
                                                             bool is_directed);

private:
    /**
     * @brief Maps a color string to a sequential uint32_t ID.
     *
     * On first encounter the string is inserted with the next available ID.
     * Subsequent encounters return the same ID. Throws if inserting would
     * exceed @c MAX_VERTEX_COLOR.
     *
     * @param color_str The color string (any value).
     * @param color_map The string→ID registry to look up or update.
     * @return The uint32_t ID assigned to @p color_str.
     * @throws GraphConstructionException if the number of distinct colors
     *         would exceed @c MAX_VERTEX_COLOR.
     */
    static uint32_t map_color_string(const std::string& color_str,
                                     std::map<std::string, uint32_t>& color_map);

    /**
     * @brief Identity overload — uint32_t color properties pass through unchanged.
     * @param color The color value.
     * @param color_map Unused; present for uniform call syntax in templates.
     * @return @p color unchanged.
     */
    static uint32_t extract_color(uint32_t color, std::map<std::string, uint32_t>& color_map);

    /**
     * @brief String overload — delegates to map_color_string.
     * @param color_str The color string.
     * @param color_map The registry to look up or update.
     * @return The uint32_t ID for @p color_str.
     * @throws GraphConstructionException if too many distinct colors.
     */
    static uint32_t extract_color(const std::string& color_str,
                                  std::map<std::string, uint32_t>& color_map);

    /**
     * @brief Extracts edges as (source, destination) pairs from any Boost graph type.
     * @tparam GraphType A Boost adjacency_list type.
     * @param boost_graph The source graph.
     * @return Uncolored edge pairs.
     */
    template <typename GraphType>
    static std::vector<std::pair<uint32_t, uint32_t>>
    extract_uncolored_edges(const GraphType& boost_graph);

    /**
     * @brief Extracts vertex colors using a caller-supplied color accessor.
     * @tparam GraphType A Boost adjacency_list type.
     * @tparam GetColor Callable: vertex_descriptor → uint32_t.
     * @param boost_graph The source graph.
     * @param get_color Returns the uint32_t color for a given vertex descriptor.
     * @return Per-vertex color labels.
     * @throws GraphConstructionException if any color value exceeds MAX_VERTEX_COLOR.
     */
    template <typename GraphType, typename GetColor>
    static std::vector<int32_t> extract_vertex_colors(const GraphType& boost_graph,
                                                      const GetColor& get_color);

    /**
     * @brief Extracts colored edges using a caller-supplied color accessor.
     * @tparam GraphType A Boost adjacency_list type.
     * @tparam GetColor Callable: edge_descriptor → uint32_t.
     * @param boost_graph The source graph.
     * @param get_color Returns the uint32_t color for a given edge descriptor.
     * @return Colored edge tuples.
     */
    template <typename GraphType, typename GetColor>
    static std::vector<std::tuple<uint32_t, uint32_t, uint32_t>>
    extract_colored_edges(const GraphType& boost_graph, const GetColor& get_color);

    /**
     * @brief Returns true if any edge has a non-zero color via the supplied accessor.
     * @tparam GraphType A Boost adjacency_list type.
     * @tparam GetColor Callable: edge_descriptor → uint32_t.
     * @param boost_graph The graph to inspect.
     * @param get_color Returns the uint32_t color for a given edge descriptor.
     * @return True if at least one edge color is non-zero.
     */
    template <typename GraphType, typename GetColor>
    static bool has_edge_colors(const GraphType& boost_graph, const GetColor& get_color);
};

template <typename GraphType, typename VertexPropertyType, typename EdgePropertyType>
ColoredGraph
GraphUtils::convert_boost_graph_to_colored_graph(const GraphType& boost_graph,
                                                 const bool is_directed,
                                                 std::map<std::string, uint32_t>& color_map)
{
    const uint32_t num_vertices = static_cast<uint32_t>(boost::num_vertices(boost_graph));
    const std::vector<int32_t> vertex_colors = extract_vertex_colors(
        boost_graph,
        [&](const typename boost::graph_traits<GraphType>::vertex_descriptor& vertex_desc)
        {
            return extract_color(boost::get(&VertexPropertyType::m_color, boost_graph, vertex_desc),
                                 color_map);
        });
    const std::function<uint32_t(const typename boost::graph_traits<GraphType>::edge_descriptor&)>
        get_edge_color =
            [&](const typename boost::graph_traits<GraphType>::edge_descriptor& edge_desc)
    {
        return extract_color(boost::get(&EdgePropertyType::m_color, boost_graph, edge_desc),
                             color_map);
    };
    if (has_edge_colors(boost_graph, get_edge_color))
    {
        std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> colored_edges =
            extract_colored_edges(boost_graph, get_edge_color);
        return {num_vertices, colored_edges, vertex_colors, is_directed};
    }
    std::vector<std::pair<uint32_t, uint32_t>> edges = extract_uncolored_edges(boost_graph);
    return {num_vertices, edges, vertex_colors, is_directed};
}

template <typename GraphType, typename VertexPropertyType, typename EdgePropertyType>
ColoredGraph GraphUtils::convert_boost_graph_to_colored_graph(const GraphType& boost_graph,
                                                              const bool is_directed)
{
    std::map<std::string, uint32_t> color_map;
    return convert_boost_graph_to_colored_graph<GraphType, VertexPropertyType, EdgePropertyType>(
        boost_graph, is_directed, color_map);
}

template <typename GraphType>
std::vector<std::pair<uint32_t, uint32_t>>
GraphUtils::extract_uncolored_edges(const GraphType& boost_graph)
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

template <typename GraphType, typename GetColor>
std::vector<int32_t> GraphUtils::extract_vertex_colors(const GraphType& boost_graph,
                                                       const GetColor& get_color)
{
    std::vector<int32_t> vertex_colors;
    vertex_colors.reserve(boost::num_vertices(boost_graph));
    for (const auto& vertex_desc : boost::make_iterator_range(boost::vertices(boost_graph)))
    {
        const uint32_t color = get_color(vertex_desc);
        if (color > SgfConstants::MAX_VERTEX_COLOR)
        {
            throw GraphConstructionException("vertex color " + std::to_string(color) +
                                             " exceeds maximum allowed value");
        }
        vertex_colors.push_back(static_cast<int32_t>(color));
    }
    return vertex_colors;
}

template <typename GraphType, typename GetColor>
std::vector<std::tuple<uint32_t, uint32_t, uint32_t>>
GraphUtils::extract_colored_edges(const GraphType& boost_graph, const GetColor& get_color)
{
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> edges;
    edges.reserve(boost::num_edges(boost_graph));
    for (const auto& edge_desc : boost::make_iterator_range(boost::edges(boost_graph)))
    {
        edges.emplace_back(static_cast<uint32_t>(boost::source(edge_desc, boost_graph)),
                           static_cast<uint32_t>(boost::target(edge_desc, boost_graph)),
                           get_color(edge_desc));
    }
    return edges;
}

template <typename GraphType, typename GetColor>
bool GraphUtils::has_edge_colors(const GraphType& boost_graph, const GetColor& get_color)
{
    const std::pair<typename boost::graph_traits<GraphType>::edge_iterator,
                    typename boost::graph_traits<GraphType>::edge_iterator>
        edge_range = boost::edges(boost_graph);
    return std::any_of(
        edge_range.first, edge_range.second,
        [&](const typename boost::graph_traits<GraphType>::edge_descriptor& edge_desc)
        {
            return get_color(edge_desc) != 0U;
        });
}

}  // namespace sgf
