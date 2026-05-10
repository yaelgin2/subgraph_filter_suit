#pragma once

#include <vector>
#include <cstdint>
#include <utility>
#include <unordered_map>

#include "Node.h"
#include "Graph.h"

/**
 * @class GeneralColorHist
 * @brief Maintains a histogram of trees that have appearances by color and pattern depth.
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
class GeneralColorHist {
private:
    /**
     * @brief Histogram matrix.
     *
     * m_number_of_neighbours[c][d] stores how many neighbors of color `c`
     * are compatible with extending the pattern at depth `d`.
     */
    std::vector<std::vector<uint32_t>> m_number_of_trees;

    /// Number of distinct colors in the graph
    int32_t C;

private:
    /**
     * @brief Computes the softmax of a vector of uint32_t values.
     * 
     * @param input Vector of uint32_t values.
     * @return Vector of doubles representing the softmax probabilities.
     */
    std::vector<double> compute_softmax(const std::vector<uint32_t>& input) const;
    int32_t num_colors;

    std::vector<double> global_color_prob;

    bool is_color_available(uint32_t c);
    uint32_t get_color_support(uint32_t c);
    int32_t get_best_node_to_connect_for_color(int32_t c, uint32_t threshold);



public:
    /**
     * @brief Construct a IndevidualColorHist object.
     *
     * @param num_colors Number of distinct colors (C)
     * @param max_s Maximum depth / pattern size
     */
    GeneralColorHist(int32_t num_colors);

    /**
     * @brief Select the best color and depth to extend next.
     *
     * Scans the entire histogram and returns the (color, depth) pair
     * with the maximum number of compatible neighbors.
     *
     * @return std::pair<color, depth>
     *         - color: color index with maximum support
     *         - depth: pattern depth where extension is most promising
     */
    /// @brief Returns the color, vertex to connect and weight of the best candidate
    std::tuple<int32_t, int32_t, uint32_t> get_color_to_add(uint32_t threshold=0, bool is_random=true); ;

    /**
     * @brief Increase number of trees that stay valid if color is added as neighbour at depth.
     */
    void update_hist_increase_tree_count(
        const uint32_t pattern_depth,
        const uint32_t current_vertex_color);

    /**
     * @brief Decrease number of trees that stay valid if color is added as neighbour at depth.
     */
    void update_hist_decrease_tree_count(
        const uint32_t pattern_depth,
        const uint32_t current_vertex_color);


    uint32_t get_color_number() const { return C; }

   
};

using GeneralColorHistPtr = std::shared_ptr<GeneralColorHist>;