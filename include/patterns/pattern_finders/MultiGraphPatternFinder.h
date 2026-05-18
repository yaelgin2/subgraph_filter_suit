#pragma once

#include "ColoredGraph.h"
#include "BoostGraph.h"
#include "Tree.h"
#include "IndividualColorHist.h"
#include "PatternUtils.h"
#include "GeneralColorHist.h"

#include <unordered_set>
#include <utility>
#include <vector>
#include <map>
#include <memory>
#include <random>
#include <boost/optional.hpp>

namespace sgf
{
/**
 * @class MultiGraphPatternFinder
 * @brief Implements the core pattern-growth algorithm over multiple S-graphs.
 *
 * The algorithm:
 *  - Recolors graphs to compact colour IDs (via PatternUtils)
 *  - Chooses an initial colour
 *  - Incrementally grows a pattern graph
 *  - Maintains per-S_i match trees with backtracking
 */
class MultiGraphPatternFinder {

public:

    MultiGraphPatternFinder(std::vector<ColoredGraph>& s_list, bool is_directed, LoggerHandler logger);

    /**
     * @brief Run the pattern-finding algorithm.
     *
     * @param s_list          Input graphs (modified in-place: colours remapped).
     * @param alive_threshold Fraction of S-graphs that must remain alive.
     * @return {pattern BoostGraph, alive_indexes}
     */
    std::pair<BoostGraph, std::unordered_set<uint32_t>>
    find_pattern(double alive_threshold,
                 bool is_random = true);


private:
    static constexpr uint32_t FIRST_COLOR_ORDER = 1;

    std::vector<ColoredGraph>& m_s_list;
    bool m_is_directed;
    std::unordered_set<uint32_t> m_alive_indexes;
    std::vector<std::shared_ptr<Tree>> m_trees;
    BoostGraph m_pattern;
    LoggerHandler m_logger;

    /* ---------- Helpers ---------- */

    /**
     * Try to add a new edge to the current pattern.
     * Returns true if an edge was added, false otherwise.
     */
    bool add_edge(std::vector<std::vector<NodePtr>>& last_nodes,
                  double threshold,
                  double alive_threshold);

    /**
     * Compute support score for a candidate pattern edge (uP, vP).
     */
    uint32_t score_edge_support(
        uint32_t uP, uint32_t vP,
        const std::vector<std::vector<NodePtr>>& last_nodes);

    /**
     * Apply an accepted edge to the pattern and prune unsupported trees.
     */
    void apply_edge_and_prune(
        uint32_t uP, uint32_t vP,
        std::vector<std::vector<NodePtr>>& last_nodes);

    uint32_t find_first_color(uint32_t color_number);

    std::tuple<int32_t, int32_t, bool> get_candidates_from_histogram(
        GeneralColorHist& color_hist,
        boost::optional<GeneralColorHist>& reverse_color_hist,
        uint32_t alive_threshold,
        bool is_random = true);

    std::pair<int32_t,int32_t> extend_pattern_at_node_find_matches_in_s(
        uint32_t new_color,
        uint32_t node_to_connect_id,
        bool is_reversed,
        std::vector<std::vector<NodePtr>>& last_nodes);

};

}
