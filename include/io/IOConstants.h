#pragma once

#include <boost/graph/adjacency_list.hpp>

namespace sgf
{

/**
 * @brief Shared I/O constants used across multiple reader and writer classes.
 */
class IOConstants
{
public:
    /// Suffix for vertex label files.
    static constexpr const char* NODE_LABELS_SUFFIX = ".node_labels";
    /// Suffix for edge files.
    static constexpr const char* EDGE_SUFFIX = ".edges";
    /// Number of tokens on a vertex (or node-label) line: id and color.
    static constexpr uint32_t VERTEX_EDGE_TOKENS_PER_VERTEX_LINE = 2;
    /// Minimum tokens on an edge line: source and destination.
    static constexpr uint32_t VERTEX_EDGE_TOKENS_PER_UNCOLORED_EDGE_LINE = 2;
    /// Tokens on a fully-colored edge line: source, destination, and color.
    static constexpr uint32_t VERTEX_EDGE_TOKENS_PER_COLORED_EDGE_LINE = 3;

    /// JSON key for the nodes array.
    static constexpr const char* JSON_NODES_KEY = "nodes";
    /// JSON key for the links (edges) array.
    static constexpr const char* JSON_LINKS_KEY = "links";
    /// JSON key for a node's identifier field.
    static constexpr const char* JSON_NODE_ID_KEY = "id";
    /// JSON key for a node's or edge's color field.
    static constexpr const char* JSON_COLOR_KEY = "color";
    /// JSON key for an edge's source vertex field.
    static constexpr const char* JSON_SOURCE_KEY = "source";
    /// JSON key for an edge's target vertex field.
    static constexpr const char* JSON_TARGET_KEY = "target";
    /// Whether pattern graphs serialised as JSON are directed.
    static constexpr bool JSON_PATTERN_IS_DIRECTED = true;

    /**
     * @brief Vertex property carrying a string color label for GraphML parsing.
     */
    struct GraphmlVertexProperties
    {
        std::string m_color = "0";
    };

    /**
     * @brief Edge property carrying a string color label for GraphML parsing.
     */
    struct GraphmlEdgeProperties
    {
        std::string m_color = "0";
    };

    /// @brief Directed Boost adjacency list used as intermediate parse target for GraphML files.
    using GraphmlDirectedBoostGraph =
        boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS, GraphmlVertexProperties,
                              GraphmlEdgeProperties>;

    /// @brief Undirected Boost adjacency list used as intermediate parse target for GraphML files.
    using GraphmlUndirectedBoostGraph =
        boost::adjacency_list<boost::vecS, boost::vecS, boost::undirectedS, GraphmlVertexProperties,
                              GraphmlEdgeProperties>;
};

}  // namespace sgf
