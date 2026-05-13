#pragma once

#include "HistFormatUtils.h"
#include "LoggerHandler.h"
#include "PatternException.h"

#include <cstdint>
#include <memory>
#include <tuple>
#include <vector>

namespace sgf
{

/**
 * @class GeneralColorHist
 * @brief Tracks how many distinct trees can still be extended at each (depth, color) cell.
 *
 * The histogram is indexed as `m_number_of_trees[depth][color]`:
 *  - depth: position in the pattern being expanded (grows on demand).
 *  - color: vertex color in the underlying graph.
 *
 * Used by the pattern expander to choose the next (color, depth) pair to extend.
 */
class GeneralColorHist
{
public:
    /**
     * @brief Construct an empty histogram for the given color count.
     * @param num_colors Number of distinct vertex colors in the graph.
     * @param logger Optional logger; defaults to a no-op handler.
     */
    explicit GeneralColorHist(uint32_t num_colors,
                               LoggerHandler logger = LoggerHandler::null());

    /**
     * @brief Choose a (color, depth) pair to extend next.
     *
     * Cells whose tree count is below @p threshold or zero are skipped. When
     * @p is_random is true, the result is sampled with weights proportional to
     * the cell's tree count; otherwise the last-scanned candidate is returned.
     *
     * @param threshold Minimum tree count for a cell to be considered.
     * @param is_random If true, perform weighted random sampling.
     * @return Tuple `{color, depth, weight}`; all -1/0 if no candidate exists.
     */
    std::tuple<int32_t, int32_t, uint32_t>
    get_color_to_add(uint32_t threshold = 0U, bool is_random = true) const;

    /**
     * @brief Increment the tree count for a (depth, color) cell, growing rows as needed.
     * @param pattern_depth Depth within the pattern being extended.
     * @param current_vertex_color Color of the vertex that adds support.
     */
    void update_hist_increase_tree_count(uint32_t pattern_depth,
                                         uint32_t current_vertex_color);

    /**
     * @brief Decrement the tree count for an existing (depth, color) cell.
     * @param pattern_depth Depth within the pattern being extended.
     * @param current_vertex_color Color of the vertex that loses support.
     */
    void update_hist_decrease_tree_count(uint32_t pattern_depth,
                                         uint32_t current_vertex_color);

    /**
     * @brief Returns the configured number of distinct colors.
     * @return Color count provided at construction.
     */
    uint32_t get_color_count() const
    {
        return m_num_colors;
    }

    /**
     * @brief Return the raw histogram matrix for inspection.
     * @return Reference to the internal [depth][color] tree-count matrix.
     */
    const std::vector<std::vector<uint32_t>>& get_histogram() const
    {
        return m_number_of_trees;
    }

    /// Maximum allowed pattern depth passed to update_hist_increase_tree_count.
    static constexpr uint32_t MAX_PATTERN_DEPTH = UINT32_MAX;

private:
    static constexpr int32_t NO_CANDIDATE_COLOR = -1;   ///< Sentinel: no valid color found.
    static constexpr int32_t NO_CANDIDATE_DEPTH = -1;   ///< Sentinel: no valid depth found.
    static constexpr uint32_t NO_CANDIDATE_WEIGHT = 0U; ///< Sentinel: no valid weight found.

    std::vector<std::vector<uint32_t>> m_number_of_trees;  ///< [depth][color] -> tree count.
    uint32_t m_num_colors;                                  ///< Number of distinct colors.
    LoggerHandler m_logger;                                 ///< Logger for before/after tracing.
};

using GeneralColorHistPtr = std::shared_ptr<GeneralColorHist>;

}  // namespace sgf
