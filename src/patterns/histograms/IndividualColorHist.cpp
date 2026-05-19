#include "IndividualColorHist.h"

#include "DebugLog.h"
#include "GeneralColorHist.h"
#include "HistFormatUtils.h"
#include "LogLevel.h"
#include "LoggerHandler.h"
#include "PatternException.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace sgf
{

IndividualColorHist::IndividualColorHist(GeneralColorHist& general_color_hist, LoggerHandler logger)
    : m_num_colors(general_color_hist.get_color_count())
    , m_general_hist(general_color_hist)
    , m_logger(std::move(logger))
{
}

void IndividualColorHist::update_hist_decrease_from_neighbours(
    const uint32_t current_vertex_color, const std::vector<uint32_t>& neighbour_depths)
{
    SGF_DEBUG_LOG(m_logger,
                       "decrease_from_neighbours: color=" + std::to_string(current_vertex_color) +
                           " neighbour_depths=" + format_uint_vector(neighbour_depths));
    SGF_DEBUG_LOG(m_logger,
                       "BEFORE individual: " + format_hist_matrix(m_number_of_neighbours));

    for (const uint32_t depth : neighbour_depths)
    {
        if (m_number_of_neighbours[depth][current_vertex_color] == 0U)
        {
            m_logger.log(LogLevel::ERROR,
                         "IndividualColorHist::decrease_from_neighbours: cell [depth=" +
                             std::to_string(depth) + "][color=" +
                             std::to_string(current_vertex_color) + "] is already zero");
            throw PatternException("cannot decrement a zero neighbour cell");
        }
        --m_number_of_neighbours[depth][current_vertex_color];
        if (m_number_of_neighbours[depth][current_vertex_color] == 0U)
        {
            m_general_hist.update_hist_decrease_tree_count(depth, current_vertex_color);
        }
    }
    SGF_DEBUG_LOG(m_logger,
                       "AFTER individual: " + format_hist_matrix(m_number_of_neighbours));
}

void IndividualColorHist::update_neighbours_add_node_add_neighbours_to_hist(
    const uint32_t new_node_depth, const std::vector<uint32_t>& neighbour_colors)
{
    SGF_DEBUG_LOG(m_logger, "add_node: depth=" + std::to_string(new_node_depth) +
                                     " neighbour_colors=" + format_uint_vector(neighbour_colors));
    SGF_DEBUG_LOG(m_logger,
                       "BEFORE individual: " + format_hist_matrix(m_number_of_neighbours));


    while (new_node_depth >= m_number_of_neighbours.size())
    {
        m_number_of_neighbours.emplace_back(m_num_colors, 0U);
    }

    for (const uint32_t color : neighbour_colors)
    {
        ++m_number_of_neighbours[new_node_depth][color];
        if (m_number_of_neighbours[new_node_depth][color] == 1U)
        {
            m_general_hist.update_hist_increase_tree_count(new_node_depth, color);
        }
    }
    SGF_DEBUG_LOG(m_logger,
                       "AFTER individual: " + format_hist_matrix(m_number_of_neighbours));
}

void IndividualColorHist::update_neighbours_remove_node_decrease_neighbours_from_hist(
    const uint32_t remove_node_depth, const std::vector<uint32_t>& neighbour_colors)
{
    SGF_DEBUG_LOG(m_logger,
                       "remove_node: depth=" + std::to_string(remove_node_depth) +
                           " neighbour_colors=" + format_uint_vector(neighbour_colors));
    SGF_DEBUG_LOG(m_logger,
                       "BEFORE individual: " + format_hist_matrix(m_number_of_neighbours));

    for (const uint32_t color : neighbour_colors)
    {
        if (m_number_of_neighbours[remove_node_depth][color] == 0U)
        {
            m_logger.log(LogLevel::ERROR,
                         "IndividualColorHist::remove_node_decrease_neighbours: cell [depth=" +
                             std::to_string(remove_node_depth) +
                             "][color=" + std::to_string(color) + "] is already zero");
            throw PatternException("cannot decrement a zero neighbour cell");
        }
        --m_number_of_neighbours[remove_node_depth][color];
        if (m_number_of_neighbours[remove_node_depth][color] == 0U)
        {
            m_general_hist.update_hist_decrease_tree_count(remove_node_depth, color);
        }
    }
    SGF_DEBUG_LOG(m_logger,
                       "AFTER individual: " + format_hist_matrix(m_number_of_neighbours));
}

}  // namespace sgf
