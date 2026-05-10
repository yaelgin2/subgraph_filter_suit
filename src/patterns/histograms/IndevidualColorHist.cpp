#include "IndevidualColorHist.h"
#include <algorithm>
#include <random>
#include <iostream>

/**
 * @brief Constructor initializes the histogram matrix to zeros.
 */
IndevidualColorHist::IndevidualColorHist(GeneralColorHist& general_color_hist): C(general_color_hist.get_color_number()), general_hist(general_color_hist)
{
}

/**
 * @brief Decreases histogram counts for neighbors invalidated by path extension.
 */
void IndevidualColorHist::update_hist_decrease_from_neighbours(
    const uint32_t current_vertex_color,
    const std::vector<uint32_t>& update_in_hist)
{
    for (const auto& neighbour : update_in_hist)
    {
        m_number_of_neighbours[neighbour][current_vertex_color]--;
        if (m_number_of_neighbours[neighbour][current_vertex_color] == 0)
        {
            general_hist.update_hist_decrease_tree_count(
                neighbour,
                current_vertex_color);
        }
    }
}

/**
 * @brief Increases histogram counts after adding a new node to the pattern.
 */
void IndevidualColorHist::update_neigbours_add_node_add_neighbours_to_hist(
    uint32_t new_node_depth,
    const std::vector<uint32_t>& update_in_hist)
{
    while(new_node_depth >= m_number_of_neighbours.size())
    {
        this->m_number_of_neighbours.push_back(std::vector<uint32_t>(C, 0));
    }
    for (const auto& neighbour : update_in_hist)
    {
        int color = neighbour;
        ++m_number_of_neighbours[new_node_depth][color];
        if (m_number_of_neighbours[new_node_depth][color] == 1)
        {
            general_hist.update_hist_increase_tree_count(
                new_node_depth,
                color);
        }
    }
}

/**
 * @brief Decreases histogram counts during node removal (backtracking).
 */
void IndevidualColorHist::update_neigbours_remove_node_decrease_neighbours_from_hist(
    uint32_t remove_node_depth,
    const std::vector<uint32_t>& update_in_hist)
{
    for (const auto& color : update_in_hist) {
        --m_number_of_neighbours[remove_node_depth][color];
        if (m_number_of_neighbours[remove_node_depth][color] == 0)
        {
            general_hist.update_hist_decrease_tree_count(
                remove_node_depth,
                color);
        }
    }
}
