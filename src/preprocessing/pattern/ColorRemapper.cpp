#include "ColorRemapper.h"

#include "ColoredGraph.h"

#include <cstdint>
#include <map>
#include <vector>

namespace sgf
{

void ColorRemapper::scan_graph_colors(const ColoredGraph& graph,
                                      std::map<int32_t, uint32_t>& original_to_remapped_color,
                                      std::vector<int32_t>& color_map)
{
    for (uint32_t vertex_index = 0; vertex_index < graph.vertex_count(); ++vertex_index)
    {
        const int32_t original_color = static_cast<int32_t>(graph.get_vertex_color(vertex_index));
        if (original_to_remapped_color.count(original_color) == 0)
        {
            original_to_remapped_color[original_color] = static_cast<uint32_t>(color_map.size());
            color_map.push_back(original_color);
        }
    }
}

void ColorRemapper::recolor_graph(const std::map<int32_t, uint32_t>& original_to_remapped_color,
                                  ColoredGraph& graph)
{
    for (uint32_t vertex_index = 0; vertex_index < graph.vertex_count(); ++vertex_index)
    {
        const int32_t original_color = static_cast<int32_t>(graph.get_vertex_color(vertex_index));
        graph.set_vertex_color(vertex_index, original_to_remapped_color.at(original_color));
    }
}

std::vector<int32_t> ColorRemapper::map_colors(ColoredGraph& graph)
{
    std::vector<int32_t> color_map;
    std::map<int32_t, uint32_t> original_to_remapped_color;
    scan_graph_colors(graph, original_to_remapped_color, color_map);
    recolor_graph(original_to_remapped_color, graph);
    return color_map;
}

std::vector<int32_t> ColorRemapper::map_colors(ColoredGraph& first_graph,
                                               ColoredGraph& second_graph)
{
    std::vector<int32_t> color_map;
    std::map<int32_t, uint32_t> original_to_remapped_color;
    scan_graph_colors(first_graph, original_to_remapped_color, color_map);
    scan_graph_colors(second_graph, original_to_remapped_color, color_map);
    recolor_graph(original_to_remapped_color, first_graph);
    recolor_graph(original_to_remapped_color, second_graph);
    return color_map;
}

std::vector<int32_t> ColorRemapper::map_colors(std::vector<ColoredGraph>& graphs)
{
    std::vector<int32_t> color_map;
    std::map<int32_t, uint32_t> original_to_remapped_color;
    for (const ColoredGraph& graph : graphs)
    {
        scan_graph_colors(graph, original_to_remapped_color, color_map);
    }
    for (ColoredGraph& graph : graphs)
    {
        recolor_graph(original_to_remapped_color, graph);
    }
    return color_map;
}

std::vector<int32_t> ColorRemapper::map_colors(ColoredGraph& primary,
                                               std::vector<ColoredGraph>& others)
{
    std::vector<int32_t> color_map;
    std::map<int32_t, uint32_t> original_to_remapped_color;
    scan_graph_colors(primary, original_to_remapped_color, color_map);
    for (const ColoredGraph& graph : others)
    {
        scan_graph_colors(graph, original_to_remapped_color, color_map);
    }
    recolor_graph(original_to_remapped_color, primary);
    for (ColoredGraph& graph : others)
    {
        recolor_graph(original_to_remapped_color, graph);
    }
    return color_map;
}

void ColorRemapper::restore_graph_colors(ColoredGraph& graph, const std::vector<int32_t>& color_map)
{
    for (uint32_t vertex_index = 0; vertex_index < graph.vertex_count(); ++vertex_index)
    {
        const uint32_t compact_id = graph.get_vertex_color(vertex_index);
        graph.set_vertex_color(vertex_index, static_cast<uint32_t>(color_map[compact_id]));
    }
}

void ColorRemapper::restore_colors(std::vector<ColoredGraph>& graphs,
                                   const std::vector<int32_t>& color_map)
{
    for (ColoredGraph& graph : graphs)
    {
        restore_graph_colors(graph, color_map);
    }
}

void ColorRemapper::restore_colors(ColoredGraph& primary, std::vector<ColoredGraph>& others,
                                   const std::vector<int32_t>& color_map)
{
    restore_graph_colors(primary, color_map);
    restore_colors(others, color_map);
}

}  // namespace sgf
