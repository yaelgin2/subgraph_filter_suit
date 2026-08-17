#pragma once

#include "BoostGraph.h"
#include "ColoredGraph.h"
#include "CountsMap.h"
#include "LoggerHandler.h"
#include "Node.h"
#include "Tree.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <tuple>
#include <optional>
#include <random>
#include <unordered_set>
#include <utility>
#include <vector>

namespace sgf
{

using MultiGraphPatternResult = std::pair<BoostGraph, std::unordered_set<uint32_t>>;

/**
 * @class MultiGraphPatternFinder
 * @brief Implements the core pattern-growth algorithm over multiple input graphs.
 *
 * The algorithm:
 *  - Uses pre-remapped graphs and a caller-supplied color_map
 *  - Chooses an initial colour
 *  - Incrementally grows a pattern graph
 *  - Maintains per-graph match trees with backtracking
 */
class MultiGraphPatternFinder
{

public:
    /**
     * @brief Construct a finder over a collection of pre-remapped input graphs.
     * @param graph_list   Input graphs (colours already remapped to compact IDs by caller).
     * @param is_directed  Whether the graphs are directed.
     * @param color_count  Number of distinct compact colour IDs in the remapped graphs.
     * @param thread_index Index of the worker thread running this finder, tagged onto every
     *                     log message so interleaved multi-threaded logs stay attributable.
     * @param logger       Logger for diagnostics.
     */
    MultiGraphPatternFinder(const std::vector<ColoredGraph>& graph_list, bool is_directed,
                            uint32_t color_count, uint32_t thread_index, LoggerHandler logger);

    /**
     * @brief Run the pattern-finding algorithm.
     * @param alive_threshold Minimum fraction of input graphs that must remain alive.
     * @param is_random       When true, vertex/edge selection is randomised.
     * @return Pair of {grown pattern BoostGraph, indexes of graphs still alive}.
     */
    MultiGraphPatternResult find_pattern(double alive_threshold, bool is_random = true);

private:
    using Entry = std::pair<std::tuple<uint32_t, uint32_t, bool>, uint32_t>;

    static constexpr uint64_t LOWER_32_BITS_MASK = 0xffffffffULL;
    static constexpr uint32_t UPPER_32_BITS_SHIFT = 32U;
    static constexpr double NON_RANDOM_PROBABILITY = 0.5;
    static constexpr uint32_t ROOT_DEPTH = 0U;

    const std::vector<ColoredGraph>& m_graph_list;
    bool m_is_directed;
    std::unordered_set<uint32_t> m_alive_graph_indexes;
    mutable std::unordered_set<uint64_t>
        m_dead_edge_pairs;  ///< Encoded (src, tgt) pairs scored 0 support; a scoring cache.
    std::vector<std::shared_ptr<Tree>> m_match_trees;
    BoostGraph m_pattern;
    uint32_t m_color_count;
    uint32_t m_thread_index;
    LoggerHandler m_logger;
    std::mt19937_64 m_random_engine;
    std::uniform_real_distribution<double> m_uniform_dist{0.0, 1.0};

    /* ---------- Initialisation helpers ---------- */

    /**
     * @brief Compute colour frequency distribution over all graphs.
     * @return Per-colour probability vector indexed by compact colour ID.
     */
    std::vector<double> build_color_distribution() const;

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
     * @param done_adding_vertices  Set true when no candidates remain.
     * @param failed_add_edge       Set true when edge addition fails.
     */
    void run_one_growth_step(double alive_threshold, bool is_random,
                             std::vector<std::vector<NodePtr>>& leaf_matches,
                             bool& done_adding_vertices, bool& failed_add_edge);

    /**
     * @brief Try to add one new vertex (with edge) to the pattern.
     *
     * Calls get_color_by_depth_neighbour_counts() on every alive tree, merges the
     * results into a single {colour, depth} → count map, then delegates selection
     * to choose_next_vertex().
     *
     * @param alive_threshold Minimum alive-graph fraction.
     * @param is_random       Passed through to choose_next_vertex.
     * @param leaf_matches    Updated in-place if a vertex is added.
     * @return True if a vertex was added.
     */
    bool attempt_add_vertex(double alive_threshold, bool is_random,
                            std::vector<std::vector<NodePtr>>& leaf_matches);

    /**
     * @brief Select the next (colour, depth) expansion key from the combined neighbour counts.
     *
     * When @p is_random is false the key with the highest count is returned.
     * When @p is_random is true the counts are used as weights for a
     * std::discrete_distribution and one key is sampled.
     *
     * @param combined_counts Map of {colour, depth} → total count across alive trees.
     * @param min_alive_count Minimum count a key must reach to be a candidate.
     * @param is_random       Whether to sample randomly (true) or pick the maximum (false).
     * @return Chosen {colour, depth} pair, or std::nullopt if no valid candidate exists.
     */
    std::optional<std::tuple<uint32_t, uint32_t, bool>>
    choose_next_vertex(const CountsMap& combined_counts, const CountsMap& tree_support,
                       uint32_t min_alive_count, bool is_random);

    /**
     * @brief Build filtered candidate list from combined counts and support map.
     * @param combined_counts  Merged neighbour counts across alive trees.
     * @param tree_support     Per-key count of trees that reported each neighbour.
     * @param min_alive_count  Minimum support count for a candidate to qualify.
     * @return Vector of (key, total-count) pairs that meet the support threshold.
     */
    static std::vector<Entry> build_candidates(const CountsMap& combined_counts,
                                               const CountsMap& tree_support,
                                               uint32_t min_alive_count);

    /**
     * @brief Sample one candidate using counts as weights.
     * @param candidates Non-empty list of (key, weight) pairs.
     * @return Chosen key.
     */
    std::tuple<uint32_t, uint32_t, bool>
    sample_candidate_random(const std::vector<Entry>& candidates);

    /**
     * @brief Accumulate combined and per-tree neighbour counts over all alive trees.
     * @param leaf_matches Current leaf nodes of each graph's match tree.
     * @return Pair of {combined_counts, tree_support}.
     */
    std::pair<CountsMap, CountsMap>
    accumulate_vertex_counts(const std::vector<std::vector<NodePtr>>& leaf_matches) const;

    /**
     * @brief Log the current set of alive graph indexes at DEBUG level.
     */
    void log_alive_graph_indexes() const;

    /**
     * @brief Build a "[thread N] " prefix identifying this finder's worker thread.
     * @return Prefix string to prepend to log messages.
     */
    std::string thread_log_prefix() const;

    /**
     * @brief Log a message tagged with this finder's thread index.
     * @param level   Log severity level.
     * @param message Message text; the thread-index prefix is prepended automatically.
     */
    void log_with_thread(LogLevel level, const std::string& message) const;

    /**
     * @brief Log the match tree's current size and leaf/match count after it has grown.
     * @param graph_idx    Index of the graph whose match tree grew.
     * @param leaf_count   Number of leaves (matches) in the tree after growth.
     */
    void log_tree_growth(uint32_t graph_idx, uint32_t leaf_count) const;

    /**
     * @brief Recolor the pattern and move results into the return value.
     * @param start_time Time point recorded at the start of find_pattern.
     * @return {result pattern, alive graph indexes}.
     */
    std::pair<BoostGraph, std::unordered_set<uint32_t>> finalize_and_return(
        const std::chrono::time_point<std::chrono::high_resolution_clock>& start_time);

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
     * @param connected_pattern_vertex The pattern vertex the new vertex connects to.
     */
    void update_tree_after_extension(
        uint32_t graph_idx, const std::vector<std::pair<uint32_t, NodePtr>>& extension_candidates,
        std::vector<std::vector<NodePtr>>& leaf_matches, uint32_t connected_pattern_vertex);

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
     *
     * Excludes self-loops, the redundant direction for undirected patterns, edges
     * already present in the pattern, and pairs already known to have 0 support
     * (see m_dead_edge_pairs) — support can only shrink as the tree grows, so a
     * pair once scored 0 never needs to be rescored.
     *
     * @param source_vertex Source vertex index in the pattern.
     * @param target_vertex Target vertex index in the pattern.
     */
    bool is_candidate_edge(uint32_t source_vertex, uint32_t target_vertex) const;

    /**
     * @brief Pack a (source, target) pattern-vertex pair into a single lookup key.
     * @param source_vertex Source vertex index in the pattern.
     * @param target_vertex Target vertex index in the pattern.
     * @return Combined 64-bit key for use in m_dead_edge_pairs.
     */
    static uint64_t encode_edge_key(uint32_t source_vertex, uint32_t target_vertex);

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
