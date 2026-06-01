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
using PatternNeighboursCount = std::unordered_map<uint32_t, uint32_t>;  ///< pattern_index -> number of appearances
using PatternPrepipheryPatternNeighboursCount =
    std::unordered_map<uint32_t, PatternNeighboursCount>;  ///< periphery_vertex_color -> {pattern_index -> count}

class PatternPeriphery
{
public:
    /**
     * @brief Construct an IndividualColorHist linked to a shared GeneralColorHist.
     * @param logger Logger for before/after tracing of histogram operations.
     * @param general_color_hist Shared histogram updated when this one's counts cross zero.
     */
    explicit PatternPeriphery(GeneralColorHist& general_color_hist,
                                 uint32_t hist_index = 0U,
                                 LoggerHandler logger = LoggerHandler::null());

    void added_vertex_to_pattern_remove_from_periphery(uint32_t vertex_color, std::vector<uint32_t> vertex_pattern_neighbours);
    void added_vertex_to_pattern_add_neihgbours_to_periphery(uint32_t pattern_index, std::vector<uint32_t> periphery_neighbours_colors);
    void remove_vertex_from_pattern_remove_from_periphery(uint32_t pattern_index, std::vector<uint32_t> periphery_neighbours_colors);

private:
    GeneralColorHist& m_general_hist;                           ///< Shared general histogram.
    uint32_t m_hist_index;      ///< Index of the graph this histogram tracks.
    LoggerHandler m_logger;  ///< Logger for before/after tracing.
    PatternPrepipheryPatternNeighboursCount m_periphery_pattern_neighbours_count;  ///< periphery_vertex_color -> {pattern_index -> count}
};

}  // namespace sgf
