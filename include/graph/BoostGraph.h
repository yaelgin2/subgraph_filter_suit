#pragma once

#include <boost/graph/adjacency_list.hpp>

namespace sgf
{

/**
 * @brief Bundled vertex property carrying a color label for Boost.Graph.
 */
struct VertexProperties
{
    uint32_t m_color = 0;
};

/**
 * @brief Bundled edge property carrying a color label for Boost.Graph.
 */
struct EdgeProperties
{
    uint32_t m_color = 0;
};

using BoostGraph = boost::adjacency_list<boost::vecS,       // edge container
                                         boost::vecS,       // vertex container (indexable)
                                         boost::directedS,  // or boost::directedS if needed
                                         VertexProperties,  // vertex properties
                                         EdgeProperties     // edge properties
                                         >;

}  // namespace sgf
