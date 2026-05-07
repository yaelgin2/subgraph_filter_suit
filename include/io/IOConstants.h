#pragma once

#include <boost/graph/adjacency_list.hpp>
#include <string>

namespace sgf
{

class IOConstants
{
public:
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
