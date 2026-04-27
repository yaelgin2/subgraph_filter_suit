#pragma once

#include "IColoredGraphReader.h"
#include "IoGraphUtils.h"

#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <cstdint>
#include <fstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sgf
{

/**
 * @brief Reads a ColoredGraph from a JSON file.
 *
 * The expected JSON format is:
 * @code
 * {
 *   "nodes": [{"id": 0, "color": 11}, ...],
 *   "links": [{"source": 0, "target": 1, "color": 2}, ...]
 * }
 * @endcode
 *
 * Node IDs may be non-consecutive; they are sorted by original ID and
 * remapped to dense consecutive indices (0, 1, 2, ...). Vertex and edge
 * colors are kept at their original values without remapping. The "color"
 * field on links follows an all-or-nothing rule: either every link carries
 * a color (producing an edge-colored graph) or no link does (producing an
 * uncolored graph). A mix throws GraphConstructionException. Missing node
 * colors and invalid source/target IDs also throw GraphConstructionException.
 */
class JsonGraphReader : public IColoredGraphReader
{
public:
    /**
     * @brief Reads a ColoredGraph from a JSON file.
     *
     * @param path Path to the JSON file.
     * @param is_directed Whether to build a directed ColoredGraph.
     * @param logger Optional logger for diagnostics. May be expired.
     * @return The parsed ColoredGraph.
     * @throws SgfPathDoesntExistException if the file cannot be opened.
     * @throws GraphConstructionException if the JSON is malformed, a node
     *         lacks a "color" field, edge colors are mixed, or a link
     *         references an unknown node ID.
     */
    ColoredGraph read(const std::string& path, bool is_directed,
                      const LoggerHandler& logger) const override;

private:
    /**
     * @brief Wraps @p caught_exception in a GraphConstructionException and throws it.
     * @param path The file path associated with the failure.
     * @param what_message The what() string of the caught exception.
     */
    [[noreturn]] static void rethrow_as_construction_error(const std::string& path,
                                                           const std::string& what_message);

    /**
     * @brief Reads entire file contents into a single string buffer.
     *
     * Uses seekg/tellg to pre-allocate the exact buffer size, avoiding
     * repeated reallocations from line-by-line reading.
     *
     * @param file An open input stream positioned at the beginning.
     * @return The full file contents as a string.
     */
    static std::string read_file_contents(std::ifstream& file);

    /**
     * @brief Reads and parses the JSON file, returning its root object.
     *
     * @param path Path to the JSON file.
     * @return The root JSON object.
     * @throws SgfPathDoesntExistException if the file cannot be opened.
     * @throws GraphConstructionException if the JSON is malformed or the
     *         root value is not a JSON object.
     */
    static boost::json::object parse_json_object(const std::string& path);

    /**
     * @brief Builds a map from original node IDs to their vertex colors.
     *
     * Every node element must contain integer "id" and "color" fields.
     *
     * @param nodes_array The JSON "nodes" array.
     * @return An unordered map from original node ID to vertex color.
     */
    static std::unordered_map<uint32_t, uint32_t>
    collect_node_colors(const boost::json::array& nodes_array);

    /**
     * @brief Extracts per-vertex colors in consecutive-index order.
     *
     * @param color_by_id Unordered map of original ID to color.
     * @param consecutive_index_by_original_id Mapping from original ID to consecutive index.
     * @return Vector of colors indexed by consecutive vertex index.
     */
    static std::vector<uint32_t> build_vertex_colors(
        const std::unordered_map<uint32_t, uint32_t>& color_by_id,
        const std::unordered_map<uint32_t, uint32_t>& consecutive_index_by_original_id);

    /**
     * @brief Determines whether links carry edge colors, enforcing all-or-nothing.
     *
     * Counts links with a "color" field. Returns true if all links have one,
     * false if none do, and throws if only some do.
     *
     * @param links_array The JSON "links" array.
     * @return True if every link has a "color" field, false if none do.
     * @throws GraphConstructionException if some links have "color" and others do not.
     */
    static bool detect_edge_colors(const boost::json::array& links_array);

    /**
     * @brief Translates a single link's "source" and "target" to consecutive indices.
     *
     * @param link_object A JSON link object with integer "source" and "target" fields.
     * @param consecutive_index_by_original_id Mapping from original node ID to consecutive index.
     * @return Pair of (consecutive source index, consecutive target index).
     * @throws GraphConstructionException (re-wrapped by the caller) if a source or target ID
     *         is absent from @p consecutive_index_by_original_id.
     */
    static std::pair<uint32_t, uint32_t> extract_link_endpoints(
        const boost::json::object& link_object,
        const std::unordered_map<uint32_t, uint32_t>& consecutive_index_by_original_id);

    /**
     * @brief Extracts edge-colored edges from @p links_array.
     *
     * Each link must have integer "source", "target", and "color" fields.
     *
     * @param links_array The JSON "links" array.
     * @param consecutive_index_by_original_id Mapping from original node ID to consecutive index.
     * @return Vector of (source, target, color) tuples using consecutive indices.
     */
    static std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> extract_colored_edges(
        const boost::json::array& links_array,
        const std::unordered_map<uint32_t, uint32_t>& consecutive_index_by_original_id);

    /**
     * @brief Extracts uncolored edges from @p links_array.
     *
     * Each link must have integer "source" and "target" fields.
     *
     * @param links_array The JSON "links" array.
     * @param consecutive_index_by_original_id Mapping from original node ID to consecutive index.
     * @return Vector of (source, target) pairs using consecutive indices.
     */
    static std::vector<std::pair<uint32_t, uint32_t>> extract_uncolored_edges(
        const boost::json::array& links_array,
        const std::unordered_map<uint32_t, uint32_t>& consecutive_index_by_original_id);

    /**
     * @brief Constructs the ColoredGraph from parsed node and link data.
     *
     * Calls detect_edge_colors() to decide between the edge-colored and
     * uncolored ColoredGraph constructors.
     *
     * @param links The JSON "links" array.
     * @param consecutive_index_by_original_id Mapping from original node ID to consecutive index.
     * @param vertex_count Number of vertices.
     * @param vertex_colors Per-vertex color labels.
     * @param is_directed Whether to build a directed graph.
     * @return The constructed ColoredGraph.
     */
    static ColoredGraph
    build_graph(const boost::json::array& links,
                const std::unordered_map<uint32_t, uint32_t>& consecutive_index_by_original_id,
                uint32_t vertex_count, const std::vector<uint32_t>& vertex_colors,
                bool is_directed);
};

}  // namespace sgf
