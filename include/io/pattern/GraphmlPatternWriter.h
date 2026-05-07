#pragma once

#include "BoostGraph.h"
#include "ColoredGraph.h"
#include "IPatternWriter.h"

#include <exception>
#include <fstream>
#include <string>

namespace sgf
{
/**
 * @brief Bundled vertex property carrying a string color label.
 *
 * Intermediate representation when parsing GraphML files, before string
 * colors are mapped to uint32_t IDs by GraphUtils.
 */
struct StringVertexProperties
{
    std::string m_color = "0";  ///< Color label read from the GraphML "color" attribute.
};

/**
 * @brief Bundled edge property carrying a string color label.
 *
 * Intermediate representation when parsing GraphML files, before string
 * colors are mapped to uint32_t IDs by GraphUtils.
 */
struct StringEdgeProperties
{
    std::string m_color = "0";  ///< Color label read from the GraphML "color" attribute.
};

/// @brief Directed Boost adjacency list used as intermediate parse target for GraphML files.
using DirectedStringBoostGraph =
    boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS, StringVertexProperties,
                          StringEdgeProperties>;

/// @brief Undirected Boost adjacency list used as intermediate parse target for GraphML files.
using UndirectedStringBoostGraph =
    boost::adjacency_list<boost::vecS, boost::vecS, boost::undirectedS, StringVertexProperties,
                          StringEdgeProperties>;
                          
/**
 * @brief Reads and writes pattern graphs in GraphML format.
 *
 * Implements IPatternIOManager for the GraphML file format using Boost.Graph.
 * write() serializes a directed BoostGraph to disk; read() deserializes a
 * GraphML file into a ColoredGraph, auto-detecting graph direction from the
 * file's edgedefault attribute.
 */
class GraphmlPatternIOManager : public IPatternWriter
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

private:
    /**
     * @brief Opens a file for writing.
     *
     * @param path Destination file path.
     * @return An open output stream.
     * @throws SgfPathDoesntExistException if the file cannot be opened.
     */
    static std::ofstream open_output_file(const std::string& path);

    /**
     * @brief Detects whether a GraphML file declares directed edges.
     *
     * Scans the file for the edgedefault attribute. Defaults to directed if
     * the attribute is absent.
     *
     * @param path Path to the GraphML file.
     * @return True if the graph is directed or no edgedefault was found.
     * @throws SgfPathDoesntExistException if the file cannot be opened.
     */
    static bool detect_is_directed(const std::string& path);

    /**
     * @brief Wraps @p exc in a GraphConstructionException and throws it.
     *
     * @param path The file path associated with the failure.
     * @param exc The original exception.
     */
    [[noreturn]] static void rethrow_as_construction_error(const std::string& path,
                                                           const std::exception& exc);
};

}  // namespace sgf
