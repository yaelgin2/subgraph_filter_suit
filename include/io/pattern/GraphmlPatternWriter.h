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
     * @brief Returns the GraphML file extension.
     * @return "graphml"
     */
    [[nodiscard]] std::string get_file_extension() const override;

private:
    /**
     * @brief Writes @p graph to @p path in GraphML format.
     *
     * @param graph Input graph.
     * @param path Output file path.
     */
    void do_write(const BoostGraph& graph, const std::string& path) const override;
    /**
     * @brief Copies all vertices and their color properties from @p graph into @p out.
     * @param graph Source BoostGraph.
     * @param out Destination GraphML-compatible Boost graph; must already be empty.
     */
    static void build_vertices(const BoostGraph& graph,
                               IOConstants::GraphmlDirectedBoostGraph& out);

    /**
     * @brief Copies all edges and their color properties from @p graph into @p out.
     * @param graph Source BoostGraph.
     * @param out Destination GraphML-compatible Boost graph; vertices must already be populated.
     */
    static void build_edges(const BoostGraph& graph, IOConstants::GraphmlDirectedBoostGraph& out);
};

}  // namespace sgf
