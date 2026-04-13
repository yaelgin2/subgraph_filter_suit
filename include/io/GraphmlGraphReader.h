#pragma once

#include "IColoredGraphReader.h"

#include <boost/graph/adjacency_list.hpp>
#include <exception>
#include <fstream>
#include <map>
#include <memory>
#include <string>

namespace sgf
{

/**
 * @brief Bundled vertex property carrying a color label for Boost.Graph.
 */
struct GraphmlVertexProperties
{
    std::string m_color = "0";
};

/**
 * @brief Bundled edge property carrying a color label for Boost.Graph.
 */
struct GraphmlEdgeProperties
{
    std::string m_color = "0";
};

/// Boost adjacency list for directed GraphML files.
using GraphmlDirectedBoostGraph =
    boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS, GraphmlVertexProperties,
                          GraphmlEdgeProperties>;

/// Boost adjacency list for undirected GraphML files.
using GraphmlUndirectedBoostGraph =
    boost::adjacency_list<boost::vecS, boost::vecS, boost::undirectedS, GraphmlVertexProperties,
                          GraphmlEdgeProperties>;

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
                      std::weak_ptr<ILogger> logger) const override;

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
     * @throws SgfPathDoesntExistException if the file cannot be opened.
     */
    static bool detect_is_directed(const std::string& path);

    /**
     * @brief Reads a GraphML file into a Boost graph.
     * @tparam GraphType A Boost adjacency_list type.
     * @param path Path to the GraphML file.
     * @param boost_graph BoostGraph to read into.
     */
    template <typename GraphType>
    static void read_graphml_from_file_into_boost_graph(const std::string& path,
                                                        GraphType& boost_graph);

    /**
     * @brief Reads a GraphML file into a ColoredGraph using the correct Boost graph type.
     *
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
    static void log_read_result(const std::shared_ptr<ILogger>& logger, const std::string& path,
                                bool file_is_directed, bool is_directed,
                                const std::map<std::string, uint32_t>& color_map);
};

}  // namespace sgf
