#pragma once

#include "IColoredGraphReader.h"

#include <exception>
#include <fstream>
#include <string>

namespace sgf
{

/**
 * @brief Reads a ColoredGraph from a GraphML file using Boost.Graph.
 *
 * Supports directed and undirected GraphML files. Directedness is inferred
 * from the @c edgedefault attribute. Vertex and edge "color" properties are
 * mapped to ColoredGraph labels. All Boost and I/O exceptions are re-wrapped
 * as GraphConstructionException.
 */
class GraphmlGraphReader : public IColoredGraphReader
{
public:
    /**
     * @brief Reads a ColoredGraph from a GraphML file.
     *
     * @param path Path to the .graphml file.
     * @return The parsed ColoredGraph.
     * @throws SgfPathDoesntExistException if the file cannot be opened.
     * @throws GraphConstructionException if the file is malformed or contains
     *         invalid property values.
     * @throws InvalidArgumentException if the graph structure is invalid
     *         (e.g. conflicting edge colors for the same endpoint pair).
     */
    ColoredGraph read(const std::string& path) const override;

private:
    /**
     * @brief Opens a file for reading.
     * @param path Path to the file.
     * @return An open input stream.
     * @throws SgfPathDoesntExistException if the file cannot be opened.
     */
    static std::ifstream open_file(const std::string& path);

    /**
     * @brief Wraps @p exc in a GraphConstructionException and throws it.
     * @param path The file path associated with the failure.
     * @param exc The original exception.
     */
    [[noreturn]] static void rethrow_as_construction_error(const std::string& path,
                                                           const std::exception& exc);

    /**
     * @brief Detects whether a GraphML file declares directed edges.
     *
     * Scans the file for the @c edgedefault attribute. If not found, defaults
     * to directed.
     *
     * @param path Path to the GraphML file.
     * @return True if the graph is directed or no @c edgedefault was found.
     * @throws GraphConstructionException if the file cannot be opened.
     */
    static bool detect_is_directed(const std::string& path);

    /**
     * @brief Opens a GraphML file and reads it into a ColoredGraph.
     *
     * Does not handle Boost exceptions — callers are responsible for wrapping
     * boost::parse_error and boost::property_tree::xml_parser::xml_parser_error.
     *
     * @param path Path to the GraphML file.
     * @param is_directed Whether to build a directed ColoredGraph.
     * @return The parsed ColoredGraph.
     */
    static ColoredGraph read_graphml_from_file(const std::string& path, bool is_directed);
};

}  // namespace sgf
