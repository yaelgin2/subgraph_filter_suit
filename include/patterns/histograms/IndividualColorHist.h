#pragma once

#include "ColoredGraph.h"
#include "GeneralColorHist.h"
#include "HistFormatUtils.h"
#include "LoggerHandler.h"
#include "Node.h"

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sgf
{

/**
 * @class IndividualColorHist
 * @brief Maintains a histogram of neighbor appearances by color and pattern depth.
 *
 * The histogram is a 2D structure:
 *
 *     m_number_of_neighbours[depth][color]
 *
 * where:
 *  - depth ∈ [0, MAX_DEPTH)
 *  - color ∈ [0, num_colors)
 *
 * Used to decide which color and depth in the pattern should be extended next,
 * based on the maximum number of compatible neighbors.
 */
class IndividualColorHist
{
public:
    /**
     * @brief Construct an IndividualColorHist linked to a shared GeneralColorHist.
     * @param logger Logger for before/after tracing of histogram operations.
     * @param general_color_hist Shared histogram updated when this one's counts cross zero.
     */
    IndividualColorHist(const LoggerHandler& logger, GeneralColorHist& general_color_hist);

    /**
     * @brief Decrease histogram counts when a vertex is added to the match path.
     *
     * For each depth index in @p neighbour_depths whose count drops to zero,
     * the shared general histogram is decremented accordingly.
     *
     * @param current_vertex_color Color of the vertex being added to the match.
     * @param neighbour_depths Depths of tree-path neighbours invalidated by the addition.
     */
    void update_hist_decrease_from_neighbours(
        uint32_t current_vertex_color,
        const std::vector<uint32_t>& neighbour_depths);

    /**
     * @brief Increase histogram counts when a node is added to the tree.
     *
     * For each color in @p neighbour_colors whose count rises from zero to one,
     * the shared general histogram is incremented accordingly.
     *
     * @param new_node_depth Depth of the newly added node in the pattern.
     * @param neighbour_colors Colors of the new node's neighbours not in the tree path.
     */
    void update_neighbours_add_node_add_neighbours_to_hist(
        uint32_t new_node_depth,
        const std::vector<uint32_t>& neighbour_colors);

    /**
     * @brief Decrease histogram counts when a node is removed from the tree (backtracking).
     *
     * For each color in @p neighbour_colors whose count drops to zero,
     * the shared general histogram is decremented accordingly.
     *
     * @param remove_node_depth Depth of the node being removed.
     * @param neighbour_colors Colors of the removed node's neighbours not in the tree path.
     */
    void update_neighbours_remove_node_decrease_neighbours_from_hist(
        uint32_t remove_node_depth,
        const std::vector<uint32_t>& neighbour_colors);

private:
    std::vector<std::vector<uint32_t>> m_number_of_neighbours;  ///< [depth][color] neighbour count.
    uint32_t m_num_colors;                                       ///< Number of distinct colors.
    GeneralColorHist& m_general_hist;                            ///< Shared general histogram.
    LoggerHandler m_logger;                                      ///< Logger for before/after tracing.
};

using IndividualColorHistPtr = std::shared_ptr<IndividualColorHist>;

}  // namespace sgf
