#include "PatternPeriphery.h"

#include "DebugLog.h"
#include "GeneralColorHist.h"
#include "HistFormatUtils.h"
#include "LoggerHandler.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace
{

std::string format_periphery_map(
    const sgf::PatternPrepipheryPatternNeighboursCount& map)
{
    std::string result = "{";
    bool first_color = true;
    for (const auto& [color, index_map] : map)
    {
        if (!first_color)
        {
            result += ", ";
        }
        first_color = false;
        result += std::to_string(color) + ":{";
        bool first_index = true;
        for (const auto& [index, count] : index_map)
        {
            if (!first_index)
            {
                result += ",";
            }
            first_index = false;
            result += std::to_string(index) + ":" + std::to_string(count);
        }
        result += "}";
    }
    result += "}";
    return result;
}

}  // namespace

namespace sgf
{

PatternPeriphery::PatternPeriphery(GeneralColorHist& general_color_hist,
                                   const uint32_t hist_index, LoggerHandler logger)
    : m_general_hist(general_color_hist)
    , m_hist_index(hist_index)
    , m_logger(std::move(logger))
{
}

void PatternPeriphery::added_vertex_to_pattern_remove_from_periphery(uint32_t vertex_color,
    std::vector<uint32_t> vertex_pattern_neighbours)
{
    SGF_DEBUG_LOG(m_logger, "[hist=" + std::to_string(m_hist_index) +
                                "] remove_from_periphery: vertex_color=" +
                                std::to_string(vertex_color) + " pattern_neighbours=" +
                                format_uint_vector(vertex_pattern_neighbours));
    SGF_DEBUG_LOG(m_logger, "[hist=" + std::to_string(m_hist_index) +
                                "] BEFORE: " + format_periphery_map(m_periphery_pattern_neighbours_count));

    for (uint32_t neighbour_depth : vertex_pattern_neighbours)
    {
        m_periphery_pattern_neighbours_count[vertex_color][neighbour_depth]--;
        if (m_periphery_pattern_neighbours_count[vertex_color][neighbour_depth] == 0U)
        {
            m_general_hist.update_hist_decrease_tree_count(neighbour_depth, vertex_color);
        }
    }

    SGF_DEBUG_LOG(m_logger, "[hist=" + std::to_string(m_hist_index) +
                                "] AFTER: " + format_periphery_map(m_periphery_pattern_neighbours_count));
}

void PatternPeriphery::added_vertex_to_pattern_add_neihgbours_to_periphery(uint32_t pattern_index,
    std::vector<uint32_t> periphery_neighbours_colors)
{
    SGF_DEBUG_LOG(m_logger, "[hist=" + std::to_string(m_hist_index) +
                                "] add_neighbours_to_periphery: pattern_index=" +
                                std::to_string(pattern_index) + " neighbour_colors=" +
                                format_uint_vector(periphery_neighbours_colors));
    SGF_DEBUG_LOG(m_logger, "[hist=" + std::to_string(m_hist_index) +
                                "] BEFORE: " + format_periphery_map(m_periphery_pattern_neighbours_count));

    for (uint32_t neighbour_color : periphery_neighbours_colors)
    {
        if (m_periphery_pattern_neighbours_count[neighbour_color][pattern_index] == 0U)
        {
            m_general_hist.update_hist_increase_tree_count(pattern_index, neighbour_color);
        }
        m_periphery_pattern_neighbours_count[neighbour_color][pattern_index]++;
    }

    SGF_DEBUG_LOG(m_logger, "[hist=" + std::to_string(m_hist_index) +
                                "] AFTER: " + format_periphery_map(m_periphery_pattern_neighbours_count));
}

void PatternPeriphery::remove_vertex_from_pattern_remove_from_periphery(uint32_t pattern_index,
    std::vector<uint32_t> periphery_neighbours_colors)
{
    SGF_DEBUG_LOG(m_logger, "[hist=" + std::to_string(m_hist_index) +
                                "] remove_vertex_from_periphery: pattern_index=" +
                                std::to_string(pattern_index) + " neighbour_colors=" +
                                format_uint_vector(periphery_neighbours_colors));
    SGF_DEBUG_LOG(m_logger, "[hist=" + std::to_string(m_hist_index) +
                                "] BEFORE: " + format_periphery_map(m_periphery_pattern_neighbours_count));

    for (uint32_t neighbour_color : periphery_neighbours_colors)
    {
        m_periphery_pattern_neighbours_count[neighbour_color][pattern_index]--;
        if (m_periphery_pattern_neighbours_count[neighbour_color][pattern_index] == 0U)
        {
            m_general_hist.update_hist_decrease_tree_count(pattern_index, neighbour_color);
        }
    }

    SGF_DEBUG_LOG(m_logger, "[hist=" + std::to_string(m_hist_index) +
                                "] AFTER: " + format_periphery_map(m_periphery_pattern_neighbours_count));
}

}  // namespace sgf
