#pragma once

#include "IColoredGraphReader.h"

#include <cstdint>
#include <fstream>
#include <memory>
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
 * <vertex_id> <color>
 * @endcode
 * - @c vertex_id : original node identifier (may be non-consecutive).
 * - @c color     : unsigned integer color label for the vertex.
 *
 * **@p path.edges** — one edge per line:
 * @code
 * <src_vertex_id> <dst_vertex_id> [color]
 * @endcode
 * - @c src_vertex_id / @c dst_vertex_id : original node identifiers.
 * - @c color (optional)                 : unsigned integer edge color label.
 *
 * Node IDs may be non-consecutive; they are remapped to dense consecutive indices.
 * Edge colors follow an all-or-nothing rule:
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
     * @throws InvalidArgumentException if parallel edges carry different color labels
     *         (propagated from the ColoredGraph constructor).
     */
    ColoredGraph read(const std::string& path, const bool is_directed,
                      const LoggerHandler& logger) const override;

private:
    /**
     * @brief Holds the parsed edge lists from a .edges file.
     *
     * Exactly one of @c colored or @c uncolored will be non-empty on return from
     * parse_edge_file (the all-or-nothing color rule). Both are empty when the
     * file contains no edges.
     */
    struct EdgeData
    {
        std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> colored;
        std::vector<std::pair<uint32_t, uint32_t>> uncolored;
    };

    /**
     * @brief Opens a file for reading, throwing on failure.
     * @param file_path Path to the file.
     * @return An open input stream.
     * @throws SgfPathDoesntExistException if the file cannot be opened.
     */
    static std::ifstream open_file(const std::string& file_path);

    /**
     * @brief Parses one line from a .vertex_indices file.
     *
     * Expects exactly two whitespace-separated tokens: vertex_id and color.
     * Throws on too-few or too-many tokens.
     *
     * @param line Raw line text.
     * @param file_path Path of the file (used in error messages).
     * @return Pair of (vertex_id, color).
     * @throws GraphConstructionException if the line is malformed.
     */
    static std::pair<uint32_t, uint32_t> parse_vertex_line(const std::string& line,
                                                           const std::string& file_path);

    /**
     * @brief Parses the .vertex_indices file into a color-by-original-ID map.
     *
     * Each line must contain exactly two whitespace-separated tokens: vertex_id,
     * color. Extra or missing tokens throw GraphConstructionException.
     * Duplicate vertex IDs throw GraphConstructionException.
     *
     * @param vertices_path Path to the .vertex_indices file.
     * @return Map from original vertex ID to its color label.
     * @throws SgfPathDoesntExistException if the file cannot be opened.
     * @throws GraphConstructionException if any line is malformed or a vertex
     *         ID appears more than once.
     */
    static std::unordered_map<uint32_t, uint32_t>
    parse_vertex_file(const std::string& vertices_path);

    /**
     * @brief Builds a remapping from original vertex IDs to consecutive indices.
     *
     *
     * @param vertex_color_by_original_id Map from original ID to color.
     * @return Map from original ID to consecutive zero-based index.
     */
    static std::unordered_map<uint32_t, uint32_t> build_consecutive_index_map(
        const std::unordered_map<uint32_t, uint32_t>& vertex_color_by_original_id);

    /**
     * @brief Extracts per-vertex colors in consecutive-index order.
     *
     * @param vertex_color_by_original_id Map from original ID to color.
     * @param consecutive_index_by_original_id Map from original ID to consecutive index.
     * @return Vector of color labels indexed by consecutive vertex index.
     * @throws GraphConstructionException if an index lookup fails unexpectedly.
     */
    static std::vector<uint32_t> build_vertex_colors(
        const std::unordered_map<uint32_t, uint32_t>& vertex_color_by_original_id,
        const std::unordered_map<uint32_t, uint32_t>& consecutive_index_by_original_id);

    /**
     * @brief Resolves a raw vertex ID to its consecutive index, throwing if unknown.
     *
     * @param raw_id Original vertex ID from the file.
     * @param consecutive_index_by_original_id Map from original ID to consecutive index.
     * @param role Human-readable role label ("source" or "destination") for error messages.
     * @param line Raw edge line text (used in error messages).
     * @return Consecutive index for @p raw_id.
     * @throws GraphConstructionException if @p raw_id is not in the map.
     */
    static uint32_t resolve_vertex_id(
        const uint32_t raw_id,
        const std::unordered_map<uint32_t, uint32_t>& consecutive_index_by_original_id,
        const std::string& role, const std::string& line);

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
    static bool
    parse_edge_line(const std::string& line,
                    const std::unordered_map<uint32_t, uint32_t>& consecutive_index_by_original_id,
                    uint32_t& out_src, uint32_t& out_dst, uint32_t& out_color);

    /**
     * @brief Parses the .edges file, enforcing the all-or-nothing color rule.
     *
     * @param edges_path Path to the .edges file.
     * @param consecutive_index_by_original_id Map from original vertex ID to consecutive index.
     * @return EdgeData with exactly one of colored or uncolored populated (both empty
     *         if the file has no edges).
     * @throws SgfPathDoesntExistException if the file cannot be opened.
     * @throws GraphConstructionException if any line is malformed, references
     *         an unknown vertex ID, or edge colors are mixed.
     */
    static EdgeData
    parse_edge_file(const std::string& edges_path,
                    const std::unordered_map<uint32_t, uint32_t>& consecutive_index_by_original_id);
};

}  // namespace sgf
