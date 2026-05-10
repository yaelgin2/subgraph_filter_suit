#pragma once

#include <vector>
#include <cstdint>
#include <utility>
#include <unordered_map>

#include "Node.h"
#include "Graph.h"
#include "GeneralColorHist.h"

/**
 * @class IndevidualColorHist
 * @brief Maintains a histogram of neighbor appearances by color and pattern depth.
 *
 * The histogram is a 2D structure:
 *
 *     m_number_of_neighbours[color][depth]
 *
 * where:
 *  - color ∈ [0, C)
 *  - depth ∈ [0, MAX_S)
 *
 * This structure is used to decide which color and which depth in the pattern
 * should be extended next, based on the maximum number of compatible neighbors.
 */
class IndevidualColorHist {
private:
    /**
     * @brief Histogram matrix.
     *
     * m_number_of_neighbours[c][d] stores how many neighbors of color `c`
     * are compatible with extending the pattern at depth `d`.
     */
    std::vector<std::vector<uint32_t>> m_number_of_neighbours;

    /// Number of distinct colors in the graph
    int32_t C;

    GeneralColorHist& general_hist;

public:
    /**
     * @brief Construct a IndevidualColorHist object.
     *
     * @param num_colors Number of distinct colors (C)
     * @param max_s Maximum depth / pattern size
     */
    IndevidualColorHist(GeneralColorHist& general_color_hist);

    /**
     * @brief Decrease histogram counts due to neighbour added to match.
 
     * @param current_vertex_color uint32_t - color of the current vertex in pattern P
     * @param update_in_hist vector of indexes in p of current neighbors in pattern 
     * 
     */
    void update_hist_decrease_from_neighbours(
        const uint32_t current_vertex_color,
        const std::vector<uint32_t>& update_in_hist
    );

    /**
     * @brief Increase histogram counts when a node is added to the tree.
     *
     * Called after successfully adding a node to the pattern tree,
     * increasing compatibility counts for its neighbors.
     *
     * @param s_graph Graph f tree whose match is extanded.
     * @param new_node_depth Depth of the newly added node in the pattern
     * @param update_in_hist vector:
     *        - second: vertex color
     */
    void update_neigbours_add_node_add_neighbours_to_hist(
        uint32_t new_node_depth,
        const std::vector<uint32_t>& update_in_hist
    );

    /**
     * @brief Decrease histogram counts when removing a node from the tree.
     *
     * Used during backtracking when a node is removed and its contribution
     * to neighbor compatibility must be reverted.
     *
     * @param s_graph Graph f tree whose match is extanded.
     * @param update_in_hist vector of pairs:
     *        - first: index in pattern P (depth)
     *        - second: vertex color
     */
    void update_neigbours_remove_node_decrease_neighbours_from_hist(
        uint32_t remove_node_depth,
        const std::vector<uint32_t>& update_in_hist
        
    );
};

using IndevidualColorHistPtr = std::shared_ptr<IndevidualColorHist>;