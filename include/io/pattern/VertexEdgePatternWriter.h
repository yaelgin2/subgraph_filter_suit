#pragma once

#include "BoostGraph.h"
#include "IPatternWriter.h"

#include <string>

namespace sgf
{

/**
 * @class VertexEdgePatternIOManager
 * @brief Reads and writes pattern graphs as a pair of plain-text files.
 *
 * The format uses two files sharing the same base path:
 *
 * **@p path.node_labels** — one vertex per line:
 * @code
 * <vertex_id> <color>
 * @endcode
 *
 * **@p path.edge** — one edge per line:
 * @code
 * <src_vertex_id> <dst_vertex_id> [color]
 * @endcode
 *
 * Edge colors follow an all-or-nothing rule on read: every edge must carry a
 * color token or none may. A mix throws GraphConstructionException. The write
 * method always emits edge colors because the BoostGraph always carries edge
 * color properties.
 */
class VertexEdgePatternWriter : public IPatternWriter
{
public:
    /**
     * @brief Default constructor.
     */
    VertexEdgePatternWriter() = default;

    /**
     * @brief Writes a BoostGraph to @p path.node_labels and @p path.edge.
     *
     * Each vertex is written as "<index> <color>". Each edge is written as
     * "<source> <target> <color>".
     *
     * @param graph The pattern graph to serialize.
     * @param path Base file path; suffixes are appended automatically.
     * @throws SgfPathDoesntExistException if either output file cannot be opened.
     */
    void write(const BoostGraph& graph, const std::string& path) const override;

private:
    /**
     * @brief Writes vertex data to @p base_path.node_labels.
     *
     * @param graph The source graph.
     * @param base_path Base file path without suffix.
     * @throws SgfPathDoesntExistException if the file cannot be opened for writing.
     */
    static void write_node_labels(const BoostGraph& graph, const std::string& base_path);

    /**
     * @brief Writes edge data to @p base_path.edge.
     *
     * @param graph The source graph.
     * @param base_path Base file path without suffix.
     * @throws SgfPathDoesntExistException if the file cannot be opened for writing.
     */
    static void write_edge_file(const BoostGraph& graph, const std::string& base_path);
};

}  // namespace sgf
