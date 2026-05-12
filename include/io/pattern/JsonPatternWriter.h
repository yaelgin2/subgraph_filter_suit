#pragma once

#include "BoostGraph.h"
#include "ColoredGraph.h"
#include "IPatternWriter.h"

#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <string>

namespace sgf
{

/**
 * @class JsonPatternWriter
 * @brief Writes pattern graphs in JSON format.
 *
 * The JSON format is:
 * @code
 * {
 *   "nodes": [{"id": 0, "color": 0}, ...],
 *   "links": [{"source": 0, "target": 1, "color": 0}, ...]
 * }
 * @endcode
 *
 * All edges are always serialized with their color value. The graph
 * is treated as directed to match the BoostGraph definition (boost::directedS).
 * Vertex indices in the BoostGraph become the "id" values in the output JSON.
 */
class JsonPatternWriter : public IPatternWriter
{
public:
    /**
     * @brief Default constructor.
     */
    JsonPatternWriter() = default;

    /**
     * @brief Writes a BoostGraph to a JSON file.
     *
     * Serializes all vertices with their colors and all directed edges with
     * their colors. Vertex indices in the BoostGraph become the "id" values
     * in the output JSON.
     *
     * @param graph The pattern graph to serialize.
     * @param path Destination file path.
     * @throws SgfPathDoesntExistException if the file cannot be opened for writing.
     */
    void write(const BoostGraph& graph, const std::string& path) const override;

private:
    /**
     * @brief Builds a JSON array of node objects from a BoostGraph.
     *
     * Each element has the form {"id": <vertex_index>, "color": <vertex_color>}.
     *
     * @param graph The source graph.
     * @return JSON array of node objects.
     */
    static boost::json::array build_nodes_array(const BoostGraph& graph);

    /**
     * @brief Builds a JSON array of link objects from a BoostGraph.
     *
     * Each element has the form {"source": <src>, "target": <dst>, "color": <edge_color>}.
     *
     * @param graph The source graph.
     * @return JSON array of link objects.
     */
    static boost::json::array build_links_array(const BoostGraph& graph);

    /**
     * @brief Serializes a JSON root object and writes it to a file.
     *
     * @param root The JSON root object to serialize.
     * @param path Destination file path.
     * @throws SgfPathDoesntExistException if the file cannot be opened for writing.
     */
    static void write_to_file(const boost::json::object& root, const std::string& path);
};

}  // namespace sgf
