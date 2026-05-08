#pragma once

#include "BoostGraph.h"
#include "IOConstants.h"
#include "IPatternWriter.h"

#include <string>

namespace sgf
{

/**
 * @brief Writes pattern graphs in GraphML format.
 */
class GraphmlPatternWriter : public IPatternWriter
{
public:
    /**
     * @brief Writes a BoostGraph to a GraphML file.
     *
     * @param graph Input graph.
     * @param path Output file path.
     */
    void write(const BoostGraph& graph, const std::string& path) const override;

private:
    /**
     * @brief Builds vertex entries for GraphML output graph.
     */
    static void build_vertices(const BoostGraph& graph,
                               IOConstants::GraphmlDirectedBoostGraph& out);

    /**
     * @brief Builds edge entries for GraphML output graph.
     */
    static void build_edges(const BoostGraph& graph, IOConstants::GraphmlDirectedBoostGraph& out);
};

}  // namespace sgf
