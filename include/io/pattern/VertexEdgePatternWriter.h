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
     * @brief Returns an empty string because this writer appends its own suffixes.
     *
     * The VertexEdge format writes two files: base_path.node_labels and
     * base_path.edges. The caller should pass a bare base path; this method
     * signals that no additional extension is prepended.
     *
     * @return Empty string.
     */
    [[nodiscard]] std::string get_file_extension() const override;

private:
    /**
     * @brief Writes @p graph to @p path.node_labels and @p path.edge.
     *
     * Each vertex is written as "<index> <color>". Each edge is written as
     * "<source> <target> <color>".
     *
     * @param graph The pattern graph to serialize.
     * @param path Base file path; suffixes are appended automatically.
     * @throws SgfPathExistsException if either output file cannot be opened.
     */
    void do_write(const BoostGraph& graph, const std::string& path,
                  bool is_directed) const override;
    /**
     * @brief Writes vertex data to @p base_path.node_labels.
     *
     * @param graph The source graph.
     * @param base_path Base file path without suffix.
     * @throws SgfPathExistsException if the file cannot be opened for writing.
     */
    static void write_node_labels(const BoostGraph& graph, const std::string& base_path);

    /**
     * @brief Writes edge data to @p base_path.edge.
     *
     * @param graph       The source graph.
     * @param base_path   Base file path without suffix.
     * @param is_directed When false, skips the higher-index endpoint of each symmetric edge pair.
     * @throws SgfPathExistsException if the file cannot be opened for writing.
     */
    static void write_edge_file(const BoostGraph& graph, const std::string& base_path,
                                bool is_directed);
};

}  // namespace sgf
