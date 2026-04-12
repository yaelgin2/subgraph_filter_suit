#pragma once

#include "BoostGraph.h"
#include "ColoredGraph.h"

#include <cstdint>

namespace sgf
{

class GraphUtils
{
public:
    /**
     * @brief Converts a Boost graph into a ColoredGraph.
     * @param boost_graph The source Boost graph.
     * @param is_directed Whether to build a directed ColoredGraph.
     * @return The resulting ColoredGraph.
     * @throws GraphConstructionException if any vertex color is out of range.
     */
    static ColoredGraph convert_boost_graph_to_colored_graph(const BoostGraph& graph,
                                                             bool is_directed);

private:
    /**
     * @brief Extracts vertex colors from a Boost graph into an int32_t vector.
     *
     * @param boost_graph The source Boost graph.
     * @return Per-vertex color labels.
     * @throws GraphConstructionException if any color value exceeds MAX_VERTEX_COLOR.
     */
    static std::vector<int32_t> extract_vertex_colors(const BoostGraph& boost_graph);

    /**
     * @brief Extracts edges as (source, destination) pairs.
     * @param boost_graph The source Boost graph.
     * @return Uncolored edge pairs.
     */
    static std::vector<std::pair<uint32_t, uint32_t>>
    extract_uncolored_edges(const BoostGraph& boost_graph);

    /**
     * @brief Extracts edges as (source, destination, color) tuples.
     * @param boost_graph The source Boost graph.
     * @return Colored edge tuples.
     */
    static std::vector<std::tuple<uint32_t, uint32_t, uint32_t>>
    extract_colored_edges(const BoostGraph& boost_graph);

    /**
     * @brief Returns true if any edge in the graph has a non-zero color.
     * @param boost_graph The graph to inspect.
     * @return True if at least one edge color is non-zero.
     */
    static bool has_edge_colors(const BoostGraph& boost_graph);
};

}  // namespace sgf
