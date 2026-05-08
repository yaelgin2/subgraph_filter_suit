#pragma once

#include "IColoredGraphReader.h"
#include "IOConstants.h"
#include "LoggerHandler.h"

#include <cstdint>
#include <map>
#include <string>

namespace sgf
{

/**
 * @brief Reads a ColoredGraph from a GraphML file using Boost.Graph.
 *
 * Supports directed and undirected GraphML files. Vertex and edge "color"
 * properties are mapped to ColoredGraph labels using a string→uint registry
 * (any string value is accepted). All Boost and I/O exceptions are re-wrapped
 * as GraphConstructionException.
 */
class GraphmlGraphReader : public IColoredGraphReader
{
public:
    /**
     * @brief Reads a ColoredGraph from a GraphML file.
     *
     * Color strings are mapped to sequential uint IDs in order of first
     * appearance across vertices and edges. The resulting map is logged at
     * INFO level if @p logger is non-null.
     *
     * If @p is_directed is false and the file declares directed edges, the
     * file edges are treated as undirected (caller param wins, warning logged).
     * If @p is_directed is true and the file declares undirected edges, a
     * GraphConstructionException is thrown — direction cannot be invented.
     *
     * @param path Path to the .graphml file.
     * @param is_directed Whether to build a directed ColoredGraph.
     * @param logger Optional logger for warnings and the color map. May be expired.
     * @return The parsed ColoredGraph.
     * @throws SgfPathDoesntExistException if the file cannot be opened.
     * @throws GraphConstructionException if the file is malformed, contains
     *         too many distinct color values, or an undirected file is requested
     *         as directed.
     * @throws InvalidArgumentException if the graph structure is invalid
     *         (e.g. conflicting edge colors for the same endpoint pair).
     */
    ColoredGraph read(const std::string& path, bool is_directed,
                      const LoggerHandler& logger) const override;

private:
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
     * @param boost_graph Graph to populate.
     */
    template <typename GraphType>
    static void read_graphml_from_file_into_boost_graph(const std::string& path,
                                                        GraphType& boost_graph);

    /**
     * @brief Reads a GraphML file into a ColoredGraph using the correct Boost graph type.
     * @param path Path to the GraphML file.
     * @param file_is_directed Whether the file declares directed edges.
     * @param is_directed Whether to build a directed ColoredGraph.
     * @param color_map Receives the string→uint color registry built during parsing.
     * @return The parsed ColoredGraph.
     */
    static ColoredGraph read_graphml_from_file(const std::string& path, bool file_is_directed,
                                               bool is_directed,
                                               std::map<std::string, uint32_t>& color_map);

    /**
     * @brief Logs direction mismatch warning and the color map.
     *
     * No-ops if @p logger is null. Logs WARNING for direction mismatch, then
     * INFO with the full color map.
     *
     * @param logger The logger to write to. May be null.
     * @param path Path used in log messages.
     * @param file_is_directed Whether the file declared directed edges.
     * @param is_directed Whether the caller requested a directed graph.
     * @param color_map The string→uint registry to log.
     */
    static void log_read_result(const LoggerHandler& logger, const std::string& path,
                                bool file_is_directed, bool is_directed,
                                const std::map<std::string, uint32_t>& color_map);
};

}  // namespace sgf
