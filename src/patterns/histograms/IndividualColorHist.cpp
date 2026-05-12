#include "IndividualColorHist.h"

namespace sgf
{

IndividualColorHist::IndividualColorHist(GeneralColorHist& general_color_hist)
    : m_num_colors(general_color_hist.get_color_count())
    , m_general_hist(general_color_hist)
{
}

void IndividualColorHist::update_hist_decrease_from_neighbours(
    const uint32_t current_vertex_color,
    const std::vector<uint32_t>& neighbour_depths)
{
    for (const uint32_t depth : neighbour_depths)
    {
        --m_number_of_neighbours[depth][current_vertex_color];
        if (m_number_of_neighbours[depth][current_vertex_color] == 0U)
        {
            m_general_hist.update_hist_decrease_tree_count(depth, current_vertex_color);
        }
    }
}

void IndividualColorHist::update_neighbours_add_node_add_neighbours_to_hist(
    const uint32_t new_node_depth,
    const std::vector<uint32_t>& neighbour_colors)
{
    while (new_node_depth >= m_number_of_neighbours.size())
    {
        m_number_of_neighbours.push_back(std::vector<uint32_t>(m_num_colors, 0U));
    }

    for (const uint32_t color : neighbour_colors)
    {
        ++m_number_of_neighbours[new_node_depth][color];
        if (m_number_of_neighbours[new_node_depth][color] == 1U)
        {
            m_general_hist.update_hist_increase_tree_count(new_node_depth, color);
        }
    }
}

void IndividualColorHist::update_neighbours_remove_node_decrease_neighbours_from_hist(
    const uint32_t remove_node_depth,
    const std::vector<uint32_t>& neighbour_colors)
{
    for (const uint32_t color : neighbour_colors)
    {
        --m_number_of_neighbours[remove_node_depth][color];
        if (m_number_of_neighbours[remove_node_depth][color] == 0U)
        {
            m_general_hist.update_hist_decrease_tree_count(remove_node_depth, color);
        }
    }
}

}  // namespace sgf
