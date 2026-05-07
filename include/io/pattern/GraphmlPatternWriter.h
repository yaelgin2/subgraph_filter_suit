#pragma once

#include "BoostGraph.h"
#include "IPatternWriter.h"

#include <string>

namespace sgf
{

/**
 * @brief Writes pattern graphs in GraphML format.
 *
 * Implements IPatternWriter for the GraphML file format using Boost.Graph.
 * write() serializes a directed BoostGraph to a GraphML file on disk.
 */
class GraphmlPatternWriter : public IPatternWriter
{
public:
    /**
     * @brief Writes a BoostGraph to a GraphML file.
     *
     * Vertex and edge color properties are serialized as "color" XML attributes.
     *
     * @param graph The pattern graph to serialize.
     * @param path Destination file path.
     * @throws SgfPathDoesntExistException if the file cannot be opened for writing.
     */
    void write(const BoostGraph& graph, const std::string& path) const override;
};

}  // namespace sgf
