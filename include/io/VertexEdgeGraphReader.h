#pragma once

#include "IColoredGraphReader.h"

#include <cstdint>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sgf
{

/**
 * @brief Reads a ColoredGraph from a pair of plain-text files.
 *
 * The format uses two files that share the same base path:
 *
 * **@p path.vertex_indices** — one vertex per line:
 * @code
 * <index> <vertex_id> <color>
 * @endcode
 * - @c index      : zero-based line index (ignored; only @c vertex_id matters).
 * - @c vertex_id  : original node identifier (may be non-consecutive).
 * - @c color      : signed integer color label for the vertex.
 *
 * **@p path.edges** — one edge per line:
 * @code
 * <src_vertex_id> <dst_vertex_id> [color]
 * @endcode
 * - @c src_vertex_id / @c dst_vertex_id : original node identifiers.
 * - @c color (optional)                 : unsigned integer edge color label.
 *
 * Node IDs may be non-consecutive; they are remapped to dense consecutive
 * indices sorted by original ID. Edge colors follow an all-or-nothing rule:
 * every edge must carry a color or none may — a mix throws
 * GraphConstructionException. Duplicate vertex IDs and unknown edge endpoint
 * IDs also throw GraphConstructionException.
 */
class VertexEdgeGraphReader : public IColoredGraphReader
{
public:
    /**
     * @brief Reads a ColoredGraph from two files derived from @p path.
     *
     * Reads @p path.vertex_indices for vertex data and @p path.edges for edge
     * data, then constructs a ColoredGraph.
     *
     * @param path Base path; suffixes @c .vertex_indices and @c .edges are appended.
     * @param is_directed Whether to build a directed ColoredGraph.
     * @param logger Optional logger for diagnostics. May be expired.
     * @return The parsed ColoredGraph.
     * @throws SgfPathDoesntExistException if either file cannot be opened.
     * @throws GraphConstructionException if either file is malformed, a vertex
     *         ID is duplicated, edge colors are mixed, or a link references an
     *         unknown vertex ID.
     */
    ColoredGraph read(const std::string& path, const bool is_directed,
                      const std::weak_ptr<ILogger> logger) const override;

private:
    /**
     * @brief Opens a file for reading, throwing on failure.
     * @param file_path Path to the file.
     * @return An open input stream.
     * @throws SgfPathDoesntExistException if the file cannot be opened.
     */
    static std::ifstream open_file(const std::string& file_path);

    /**
     * @brief Wraps @p exc in a GraphConstructionException and throws it.
     * @param file_path The file path associated with the failure.
     * @param exc The original exception.
     */
    [[noreturn]] static void rethrow_as_construction_error(const std::string& file_path,
                                                           const std::out_of_range& exc);

    /**
     * @brief Parses the .vertex_indices file into a color-by-original-ID map.
     *
     * Each line must contain three whitespace-separated tokens: index, vertex_id,
     * color. Duplicate vertex IDs throw GraphConstructionException.
     *
     * @param vertices_path Path to the .vertex_indices file.
     * @return Map from original vertex ID to its signed color label.
     * @throws SgfPathDoesntExistException if the file cannot be opened.
     * @throws GraphConstructionException if any line is malformed or a vertex
     *         ID appears more than once.
     */
    static std::unordered_map<uint32_t, int32_t> parse_vertex_file(
        const std::string& vertices_path);

    /**
     * @brief Builds a remapping from original vertex IDs to consecutive indices.
     *
     * IDs are sorted ascending before assigning consecutive indices so the
     * mapping is deterministic regardless of insertion order.
     *
     * @param vertex_color_by_original_id Map from original ID to color.
     * @return Map from original ID to consecutive zero-based index.
     */
    static std::unordered_map<uint32_t, uint32_t> build_consecutive_index_map(
        const std::unordered_map<uint32_t, int32_t>& vertex_color_by_original_id);

    /**
     * @brief Extracts per-vertex colors in consecutive-index order.
     *
     * @param vertex_color_by_original_id Map from original ID to color.
     * @param consecutive_index_by_original_id Map from original ID to consecutive index.
     * @return Vector of color labels indexed by consecutive vertex index.
     */
    static std::vector<int32_t> build_vertex_colors(
        const std::unordered_map<uint32_t, int32_t>& vertex_color_by_original_id,
        const std::unordered_map<uint32_t, uint32_t>& consecutive_index_by_original_id);

    /**
     * @brief Parses one edge line into an (src, dst) pair and optional color.
     *
     * @param line The raw line text.
     * @param consecutive_index_by_original_id Map from original ID to consecutive index.
     * @param[out] out_src  Consecutive source vertex index.
     * @param[out] out_dst  Consecutive destination vertex index.
     * @param[out] out_color Edge color if present; undefined if not present.
     * @return True if the line carries a color token, false if it has only two tokens.
     * @throws GraphConstructionException if the line is malformed or references
     *         an unknown vertex ID.
     */
    static bool parse_edge_line(
        const std::string& line,
        const std::unordered_map<uint32_t, uint32_t>& consecutive_index_by_original_id,
        uint32_t& out_src, uint32_t& out_dst, uint32_t& out_color);

    /**
     * @brief Parses the .edges file, enforcing the all-or-nothing color rule.
     *
     * Returns either a colored or uncolored edge list via output parameters.
     * Exactly one of the two output vectors will be populated on return.
     *
     * @param edges_path Path to the .edges file.
     * @param consecutive_index_by_original_id Map from original vertex ID to consecutive index.
     * @param[out] colored_edges   Populated when every edge carries a color.
     * @param[out] uncolored_edges Populated when no edge carries a color.
     * @throws SgfPathDoesntExistException if the file cannot be opened.
     * @throws GraphConstructionException if any line is malformed, references
     *         an unknown vertex ID, or edge colors are mixed.
     */
    static void parse_edge_file(
        const std::string& edges_path,
        const std::unordered_map<uint32_t, uint32_t>& consecutive_index_by_original_id,
        std::vector<std::tuple<uint32_t, uint32_t, uint32_t>>& colored_edges,
        std::vector<std::pair<uint32_t, uint32_t>>& uncolored_edges);

    /**
     * @brief Logs a successful read at INFO level.
     * @param logger The logger to write to. No-op if null.
     * @param path The base path used in the log message.
     */
    static void log_read_result(const std::shared_ptr<ILogger>& logger,
                                const std::string& path);
};

}  // namespace sgf
