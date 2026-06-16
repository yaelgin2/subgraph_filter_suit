#pragma once

#include "ColoredGraph.h"

#include <cstdint>
#include <map>
#include <vector>

namespace sgf
{

/**
 * @brief Pure-static colour-remapping utilities used by pattern preprocessors.
 *
 * Compacts sparse colour values to dense 0..N-1 IDs and rewrites graph
 * vertices in-place.  The returned color_map is the inverse:
 * color_map[compact_id] == original_value.
 */
class ColorRemapper
{
public:
    /**
     * @brief Remap vertex colours in a single graph to compact zero-based IDs.
     *
     * Colours are assigned new IDs in first-encounter order.
     * The graph is modified in-place.
     *
     * @return color_map where color_map[new_id] == original colour value.
     */
    static std::vector<int32_t> map_colors(ColoredGraph& graph);

    /**
     * @brief Remap vertex colours across two graphs to a shared compact ID space.
     *
     * Both graphs are scanned together so they share the same compact IDs.
     *
     * @return color_map where color_map[new_id] == original colour value.
     */
    static std::vector<int32_t> map_colors(ColoredGraph& first_graph, ColoredGraph& second_graph);

    /**
     * @brief Remap vertex colours across multiple graphs to a shared compact ID space.
     *
     * All graphs are scanned first (building one global colour map), then all
     * are recoloured in a second pass.
     *
     * @return color_map where color_map[new_id] == original colour value.
     */
    static std::vector<int32_t> map_colors(std::vector<ColoredGraph>& graphs);

    /**
     * @brief Remap vertex colours across a primary graph and a collection of others to a shared
     *        compact ID space.
     *
     * Scans the primary graph first, then all others, building one global colour map. Applies
     * recolor_graph to every graph. This is the variant used by preprocessors that own a background
     * graph separately from the library.
     *
     * @param primary  The primary graph (e.g. background) — scanned first, remapped in-place.
     * @param others   Additional graphs — scanned and remapped in-place.
     * @return color_map where color_map[new_id] == original colour value.
     */
    static std::vector<int32_t> map_colors(ColoredGraph& primary,
                                           std::vector<ColoredGraph>& others);

    /**
     * @brief Restore vertex colours from compact IDs back to original values in a graph vector.
     *
     * Reverses the effect of map_colors: each vertex's compact ID is replaced by
     * color_map[compact_id] (the original colour value).
     *
     * @param graphs     Graphs whose vertex colours are in compact form.
     * @param color_map  color_map[compact_id] == original colour value.
     */
    static void restore_colors(std::vector<ColoredGraph>& graphs,
                               const std::vector<int32_t>& color_map);

    /**
     * @brief Restore original colours in a primary graph and a vector of others.
     *
     * Convenience overload used by SingleGraphPatternPreprocessor to restore
     * both the background graph and the library in one call.
     *
     * @param primary    Primary graph (e.g. background) to restore.
     * @param others     Additional graphs to restore.
     * @param color_map  color_map[compact_id] == original colour value.
     */
    static void restore_colors(ColoredGraph& primary, std::vector<ColoredGraph>& others,
                               const std::vector<int32_t>& color_map);

private:
    /**
     * @brief Scan one graph's vertex colours and extend the compact colour mapping.
     *
     * @param graph                       Graph to scan.
     * @param original_to_remapped_color  Map updated in-place: original_color → compact_id.
     * @param color_map                   Inverse map updated in-place: compact_id → original_color.
     */
    static void scan_graph_colors(const ColoredGraph& graph,
                                  std::map<int32_t, uint32_t>& original_to_remapped_color,
                                  std::vector<int32_t>& color_map);

    /**
     * @brief Rewrite vertex colours in a graph using the compact mapping.
     *
     * @param original_to_remapped_color  original_color → compact_id.
     * @param graph                       Graph to recolor in-place.
     */
    static void recolor_graph(const std::map<int32_t, uint32_t>& original_to_remapped_color,
                              ColoredGraph& graph);

    /**
     * @brief Restore one graph's vertex colours from compact IDs to original values.
     *
     * @param graph      Graph whose vertex colours are in compact form.
     * @param color_map  color_map[compact_id] == original colour value.
     */
    static void restore_graph_colors(ColoredGraph& graph, const std::vector<int32_t>& color_map);
};

}  // namespace sgf
