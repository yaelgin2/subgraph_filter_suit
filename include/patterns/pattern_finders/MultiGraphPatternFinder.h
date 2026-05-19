#pragma once

#include "BoostGraph.h"
#include "ColoredGraph.h"
#include "GeneralColorHist.h"
#include "LoggerHandler.h"
#include "Node.h"
#include "Tree.h"

#include <boost/optional.hpp>
#include <chrono>
#include <cstdint>
#include <memory>
#include <random>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

namespace sgf
{
/**
 * @class MultiGraphPatternFinder
 * @brief Implements the core pattern-growth algorithm over multiple input graphs.
 *
 * The algorithm:
 *  - Recolors graphs to compact colour IDs (via PatternUtils)
 *  - Chooses an initial colour
 *  - Incrementally grows a pattern graph
 *  - Maintains per-graph match trees with backtracking
 */
class MultiGraphPatternFinder
{

public:
    /**
     * @brief Construct a finder over a collection of input graphs.
     * @param graph_list   Input graphs (colours are remapped in-place during find_pattern).
     * @param is_directed  Whether the graphs are directed.
     * @param logger       Logger for diagnostics.
     */
    MultiGraphPatternFinder(std::vector<ColoredGraph>& graph_list, bool is_directed,
                            LoggerHandler logger);

    /**
     * @brief Run the pattern-finding algorithm.
     * @param alive_threshold Minimum fraction of input graphs that must remain alive.
     * @param is_random       When true, vertex/edge selection is randomised.
     * @return Pair of {grown pattern BoostGraph, indexes of graphs still alive}.
     */
    std::pair<BoostGraph, std::unordered_set<uint32_t>> find_pattern(double alive_threshold,
                                                                     bool is_random = true);

private:
    static constexpr uint64_t LOWER_32_BITS_MASK = 0xffffffffULL;
    static constexpr uint32_t UPPER_32_BITS_SHIFT = 32U;
    static constexpr double NON_RANDOM_PROBABILITY = 0.5;

    std::vector<ColoredGraph>& m_graph_list;
    bool m_is_directed;
    std::unordered_set<uint32_t> m_alive_graph_indexes;
    std::vector<std::shared_ptr<Tree>> m_match_trees;
    BoostGraph m_pattern;
    LoggerHandler m_logger;
    std::vector<int32_t> m_color_map;
    boost::optional<GeneralColorHist> m_forward_color_hist;
    boost::optional<GeneralColorHist> m_reverse_color_hist;
    std::mt19937_64 m_random_engine;
    std::uniform_real_distribution<double> m_uniform_dist{0.0, 1.0};

    /* ---------- Initialisation helpers ---------- */

    /**
     * @brief Remap all graph colours to compact IDs; store the map in m_color_map.
     */
    void build_color_map();

    /**
     * @brief Compute colour frequency distribution over all graphs.
     * @return Per-colour probability vector indexed by compact colour ID.
     */
    std::vector<double> build_color_distribution() const;

    /**
     * @brief Construct m_forward_color_hist (and m_reverse_color_hist if directed).
     * @param color_count Number of distinct colours.
     */
    void create_histograms(uint32_t color_count);

    /**
     * @brief Resize m_match_trees and build one Tree per graph.
     * @return Empty leaf-match list (one entry per graph).
     */
    std::vector<std::vector<NodePtr>> initialize_match_trees();

    /**
     * @brief Pick the colour with the highest frequency.
     * @param color_distribution Per-colour probability vector.
     * @return Compact colour index.
     */
    static uint32_t select_first_color(const std::vector<double>& color_distribution);

    /**
     * @brief Add the first pattern vertex, populate initial matches per graph.
     * @param first_color  Compact colour index of the first vertex.
     * @param leaf_matches Updated in-place with the initial match nodes.
     */
    void seed_initial_matches(uint32_t first_color,
                              std::vector<std::vector<NodePtr>>& leaf_matches);

    /**
     * @brief Seed m_random_engine from the current wall-clock time.
     */
    void setup_random_engine();

    /* ---------- Growth loop helpers ---------- */

    /**
     * @brief Execute one iteration of the growth loop.
     * @param alive_threshold       Minimum alive-graph fraction.
     * @param is_random             Whether to use randomised selection.
     * @param leaf_matches          Current leaf nodes, updated in-place.
     * @param done_adding_vertices  Set true when histogram is exhausted.
     * @param failed_add_edge       Set true when edge addition fails.
     */
    void run_one_growth_step(double alive_threshold, bool is_random,
                             std::vector<std::vector<NodePtr>>& leaf_matches,
                             bool& done_adding_vertices, bool& failed_add_edge);

    /**
     * @brief Try to add one new vertex (with edge) to the pattern.
     * @param alive_threshold Minimum alive-graph fraction.
     * @param is_random       Whether to use randomised selection.
     * @param leaf_matches    Updated in-place if a vertex is added.
     * @return True if a vertex was added.
     */
    bool attempt_add_vertex(double alive_threshold, bool is_random,
                            std::vector<std::vector<NodePtr>>& leaf_matches);

    /**
     * @brief Log the current set of alive graph indexes at DEBUG level.
     */
    void log_alive_graph_indexes() const;

    /**
     * @brief Recolor the pattern and move results into the return value.
     * @param start_time Time point recorded at the start of find_pattern.
     * @return {result pattern, alive graph indexes}.
     */
    std::pair<BoostGraph, std::unordered_set<uint32_t>> finalize_and_return(
        const std::chrono::time_point<std::chrono::high_resolution_clock>& start_time);

    /* ---------- Histogram selection ---------- */

    /**
     * @brief Select the next vertex colour and attachment point from the histograms.
     * @param min_alive_count Minimum support count a histogram cell must reach.
     * @param is_random       Whether selection is weighted-random.
     * @return Tuple of {colour index, depth index, is_edge_reversed}.
     */
    std::tuple<int32_t, int32_t, bool> get_candidates_from_histogram(uint32_t min_alive_count,
                                                                     bool is_random = true);

    /**
     * @brief Choose between forward and reverse candidates for a directed graph.
     * @param forward_candidate Forward-direction candidate from the histogram.
     * @param min_alive_count   Minimum support count.
     * @param is_random         Whether selection is weighted-random.
     * @return Tuple of {colour index, depth index, is_edge_reversed}.
     */
    std::tuple<int32_t, int32_t, bool>
    select_directed_candidate(const std::tuple<int32_t, int32_t, uint32_t>& forward_candidate,
                              uint32_t min_alive_count, bool is_random);

    /* ---------- Pattern extension ---------- */

    /**
     * @brief Extend the pattern by one vertex and update match trees in all graphs.
     * @param new_vertex_color     Colour of the newly added vertex.
     * @param connection_vertex_id Pattern vertex the new vertex connects to.
     * @param is_edge_reversed     When true, the edge goes new_vertex → connection_vertex.
     * @param leaf_matches         Current leaf nodes updated in-place.
     */
    void extend_pattern_at_node_find_matches_in_s(uint32_t new_vertex_color,
                                                  uint32_t connection_vertex_id,
                                                  bool is_edge_reversed,
                                                  std::vector<std::vector<NodePtr>>& leaf_matches);

    /**
     * @brief Collect all valid extension vertices for one graph.
     * @param graph_idx            Index into m_graph_list.
     * @param leaf_matches         Current leaf nodes (read-only).
     * @param new_vertex_color     Required colour for the new vertex.
     * @param connection_vertex_id Pattern vertex the new vertex attaches to.
     * @param is_edge_reversed     Edge direction flag.
     * @return List of {vertex index, parent leaf} pairs.
     */
    std::vector<std::pair<uint32_t, NodePtr>> collect_extension_candidates(
        uint32_t graph_idx, const std::vector<std::vector<NodePtr>>& leaf_matches,
        uint32_t new_vertex_color, uint32_t connection_vertex_id, bool is_edge_reversed);

    /**
     * @brief Update the match tree for one graph after an extension step.
     * @param graph_idx            Index into m_graph_list / m_match_trees.
     * @param extension_candidates Candidates produced by collect_extension_candidates.
     * @param leaf_matches         Updated in-place.
     */
    void update_tree_after_extension(
        uint32_t graph_idx, const std::vector<std::pair<uint32_t, NodePtr>>& extension_candidates,
        std::vector<std::vector<NodePtr>>& leaf_matches);

    /* ---------- Edge scoring and pruning ---------- */

    /**
     * @brief Try to add the highest-scoring edge to the pattern.
     * @param leaf_matches      Current leaf nodes of each graph's match tree.
     * @param support_threshold Minimum support fraction among alive graphs.
     * @param alive_threshold   Minimum support fraction among all input graphs.
     * @return True if an edge was added.
     */
    bool add_edge(std::vector<std::vector<NodePtr>>& leaf_matches, double support_threshold,
                  double alive_threshold);

    /**
     * @brief Return true if (source, target) is a valid candidate edge to score.
     * @param source_vertex Source vertex index in the pattern.
     * @param target_vertex Target vertex index in the pattern.
     */
    bool is_candidate_edge(uint32_t source_vertex, uint32_t target_vertex) const;

    /**
     * @brief Find the (src, tgt) pair with the highest edge-support score.
     * @param leaf_matches   Current leaf nodes (read-only).
     * @param best_src_out   Filled with the best source vertex on success.
     * @param best_tgt_out   Filled with the best target vertex on success.
     * @param best_score_out Filled with the best score on success.
     * @return True if any candidate edge was found.
     */
    bool find_best_candidate_edge(const std::vector<std::vector<NodePtr>>& leaf_matches,
                                  uint32_t& best_src_out, uint32_t& best_tgt_out,
                                  uint32_t& best_score_out) const;

    /**
     * @brief Compute the number of alive graphs whose matches support edge (src, tgt).
     * @param pattern_src  Source vertex index in the pattern.
     * @param pattern_tgt  Target vertex index in the pattern.
     * @param leaf_matches Current leaf nodes of each graph's match tree.
     * @return Support count.
     */
    uint32_t score_edge_support(uint32_t pattern_src, uint32_t pattern_tgt,
                                const std::vector<std::vector<NodePtr>>& leaf_matches) const;

    /**
     * @brief Return true if any match in one graph supports edge (src, tgt).
     * @param graph_idx    Index into m_graph_list.
     * @param pattern_src  Source vertex index in the pattern.
     * @param pattern_tgt  Target vertex index in the pattern.
     * @param leaf_matches Current leaf nodes (read-only).
     */
    bool is_edge_supported_by_graph(uint32_t graph_idx, uint32_t pattern_src, uint32_t pattern_tgt,
                                    const std::vector<std::vector<NodePtr>>& leaf_matches) const;

    /**
     * @brief Add edge (src, tgt) to the pattern and prune non-supporting matches.
     * @param pattern_src  Source vertex index in the pattern.
     * @param pattern_tgt  Target vertex index in the pattern.
     * @param leaf_matches Updated in-place.
     */
    void apply_edge_and_prune(uint32_t pattern_src, uint32_t pattern_tgt,
                              std::vector<std::vector<NodePtr>>& leaf_matches);

    /**
     * @brief Remove leaves from one tree that do not support edge (src, tgt).
     * @param tree_idx     Index into m_match_trees.
     * @param pattern_src  Source vertex index in the pattern.
     * @param pattern_tgt  Target vertex index in the pattern.
     * @param leaf_matches Updated in-place.
     */
    void filter_tree_matches_by_edge(uint32_t tree_idx, uint32_t pattern_src, uint32_t pattern_tgt,
                                     std::vector<std::vector<NodePtr>>& leaf_matches);
};

}  // namespace sgf
