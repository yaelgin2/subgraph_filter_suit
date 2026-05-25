#pragma once

#include "ColoredGraph.h"
#include "PriorPolicy.h"

#include <atomic>
#include <cstdint>
#include <limits>
#include <mutex>
#include <ostream>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sgf
{

/**
 * @brief Backtracking subgraph isomorphism searcher with prior-based ordering.
 *
 * Transliterated from the Go "linegraph" implementation. Finds all subgraph
 * isomorphisms of @p subgraph inside @p graph. Vertex ordering is controlled
 * by a PriorPolicy. Search is parallelised at the top level: one thread per
 * candidate start vertex, batched in groups of BATCH_SIZE.
 *
 * Each match is written to the output stream as a space-separated list of
 * "graph_vertex:subgraph_vertex" pairs, followed by a newline.
 */
class SubgraphSearcher
{
public:
    /**
     * @brief Constructs a SubgraphSearcher.
     * @param policy     Vertex ordering heuristic for the search.
     * @param is_directed Whether the graphs are directed.
     * @param is_induced  Whether to enforce induced-subgraph semantics.
     * @param output     Stream to which matches are written.
     */
    SubgraphSearcher(PriorPolicy policy, bool is_directed, bool is_induced, std::ostream& output);

    SubgraphSearcher(const SubgraphSearcher&) = delete;
    SubgraphSearcher& operator=(const SubgraphSearcher&) = delete;
    SubgraphSearcher(SubgraphSearcher&&) = delete;
    SubgraphSearcher& operator=(SubgraphSearcher&&) = delete;

    /**
     * @brief Default destructor.
     */
    ~SubgraphSearcher() = default;

    /**
     * @brief Finds all subgraph isomorphisms of @p subgraph in @p graph.
     * @param graph    The host graph to search in.
     * @param subgraph The pattern graph to search for.
     * @return Total number of matches found.
     */
    uint64_t find_all(const ColoredGraph& graph, const ColoredGraph& subgraph) const;

    /**
     * @brief Computes the prior-score map for a given policy.
     *
     * For SUBGRAPH_DEGREE_SQUARED: maps each subgraph vertex to the sum of
     * its neighbours' degrees in the subgraph. For GRAPH_DEGREE_SQUARED: maps
     * each graph vertex to the sum of its neighbours' degrees in the graph.
     * For SUBGRAPH_DEGREE: maps each subgraph vertex to its own degree. All
     * other policies return an empty map.
     *
     * @param subgraph The pattern graph.
     * @param graph    The host graph.
     * @param policy   Which prior to compute.
     * @return Map from vertex ID to prior score.
     */
private:
    using VertexSet = std::unordered_set<uint32_t>;
    using RestrictionMap = std::unordered_map<uint32_t, VertexSet>;
    using PriorMap = std::unordered_map<uint32_t, float>;
    using PathMap = std::unordered_map<uint32_t, uint32_t>;

    /**
     * @brief Bundles the per-thread mutable search state for backtracking.
     */
    struct SearchContext
    {
        const ColoredGraph& m_graph;
        const ColoredGraph& m_subgraph;
        const PriorMap& m_prior;
        RestrictionMap m_restrictions;
        PathMap m_path;
        VertexSet m_chosen;
    };

    /**
     * @brief Groups parameters needed when filtering a restriction entry.
     */
    struct FilterParams
    {
        uint32_t m_subgraph_neighbor;
        uint32_t m_subgraph_vertex;
        uint32_t m_graph_vertex;
    };

    static constexpr uint32_t INVALID_VERTEX_ID = std::numeric_limits<uint32_t>::max();
    static constexpr uint32_t BATCH_SIZE = 512U;
    static constexpr float NEGATIVE_INFINITY = -std::numeric_limits<float>::infinity();

    /**
     * @brief Joins and clears a thread vector.
     * @param threads Vector of threads to join and clear.
     */
    static void join_all(std::vector<std::thread>& threads);

    /**
     * @brief Computes the prior-score map for a given policy.
     * @param subgraph The pattern graph.
     * @param graph    The host graph.
     * @param policy   Which prior to compute.
     * @return Map from vertex ID to prior score.
     */
    PriorMap calculate_prior(const ColoredGraph& subgraph, const ColoredGraph& graph,
                             PriorPolicy policy) const;

    /**
     * @brief Builds a prior map using second-degree scores from @p target.
     * @param target The graph whose vertices are scored.
     * @return Prior map from vertex ID to second-degree score.
     */
    PriorMap prior_second_degree(const ColoredGraph& target) const;

    /**
     * @brief Builds a prior map using first-degree (adjacency count) scores from @p target.
     * @param target The graph whose vertices are scored.
     * @return Prior map from vertex ID to degree score.
     */
    PriorMap prior_first_degree(const ColoredGraph& target) const;

    /**
     * @brief Computes the restriction score for the GRAPH_DEGREE_SQUARED policy.
     * @param restrictions Current restriction map.
     * @param prior        Prior score map.
     * @param vertex       Subgraph vertex to score.
     * @return Negative sum of prior scores of candidates in the restriction set.
     */
    static float score_graph_degree_squared(const RestrictionMap& restrictions,
                                            const PriorMap& prior, uint32_t vertex);

    /**
     * @brief Computes the sum of neighbour degrees for @p vertex in @p graph.
     *
     * Counts neighbours in both directions (matching Go's @c neighborhood map),
     * so directed and undirected graphs are treated consistently.
     *
     * @param graph  The graph to query.
     * @param vertex The vertex whose second-order degree is computed.
     * @return Sum of neighbour degrees.
     */
    float vertex_second_degree(const ColoredGraph& graph, uint32_t vertex) const;

    /**
     * @brief Returns the out-degree (or total degree for undirected) of @p vertex.
     * @param graph  The graph to query.
     * @param vertex The vertex to measure.
     * @return Number of out-edges.
     */
    static uint32_t out_degree(const ColoredGraph& graph, uint32_t vertex);

    /**
     * @brief Returns the in-degree of @p vertex in a directed graph.
     * @param graph  The directed graph to query.
     * @param vertex The vertex to measure.
     * @return Number of in-edges.
     */
    static uint32_t in_degree(const ColoredGraph& graph, uint32_t vertex);

    /**
     * @brief Scores @p vertex for choose_next / choose_start under the active policy.
     * @param restrictions Current restriction map (may be empty for choose_start).
     * @param prior        Prior score map.
     * @param vertex       Subgraph vertex to score.
     * @return Score; higher means chosen earlier.
     */
    float restriction_score(const RestrictionMap& restrictions, const PriorMap& prior,
                            uint32_t vertex) const;

    /**
     * @brief Selects the first subgraph vertex to match.
     * @param subgraph The pattern graph.
     * @param prior    Prior score map.
     * @return The chosen starting subgraph vertex.
     */
    uint32_t choose_start(const ColoredGraph& subgraph, const PriorMap& prior) const;

    /**
     * @brief Selects the next subgraph vertex to match during backtracking.
     * @param restrictions Current restriction map.
     * @param chosen       Already-matched subgraph vertices.
     * @param subgraph     The pattern graph.
     * @param prior        Prior score map.
     * @return The chosen next subgraph vertex.
     */
    uint32_t choose_next(const RestrictionMap& restrictions, const VertexSet& chosen,
                         const ColoredGraph& subgraph, const PriorMap& prior) const;

    /**
     * @brief Returns the first subgraph vertex not in @p chosen.
     * @param subgraph The pattern graph.
     * @param chosen   Already-matched subgraph vertices.
     * @return First unchosen vertex, or INVALID_VERTEX_ID if all are chosen.
     */
    static uint32_t find_unchosen_vertex(const ColoredGraph& subgraph, const VertexSet& chosen);

    /**
     * @brief Checks induced-subgraph constraints for undirected graphs.
     * @param context       Current search context.
     * @param graph_vertex  Candidate graph vertex being assigned.
     * @param subgraph_vertex The subgraph vertex it is assigned to.
     * @return True if induced constraint is satisfied.
     */
    static bool is_induced_valid_undirected(const SearchContext& context, uint32_t graph_vertex,
                                            uint32_t subgraph_vertex);

    /**
     * @brief Checks induced-subgraph constraints for out-edges in a directed graph.
     * @param context         Current search context.
     * @param graph_vertex    Candidate graph vertex being assigned.
     * @param subgraph_vertex The subgraph vertex it is assigned to.
     * @return True if out-edge induced constraint is satisfied.
     */
    static bool check_out_induced(const SearchContext& context, uint32_t graph_vertex,
                                  uint32_t subgraph_vertex);

    /**
     * @brief Checks induced-subgraph constraints for in-edges in a directed graph.
     * @param context         Current search context.
     * @param graph_vertex    Candidate graph vertex being assigned.
     * @param subgraph_vertex The subgraph vertex it is assigned to.
     * @return True if in-edge induced constraint is satisfied.
     */
    static bool check_in_induced(const SearchContext& context, uint32_t graph_vertex,
                                 uint32_t subgraph_vertex);

    /**
     * @brief Dispatches to directed or undirected induced validity check.
     * @param context         Current search context.
     * @param graph_vertex    Candidate graph vertex being assigned.
     * @param subgraph_vertex The subgraph vertex it is assigned to.
     * @return True if induced constraints are satisfied.
     */
    bool is_induced_valid(const SearchContext& context, uint32_t graph_vertex,
                          uint32_t subgraph_vertex) const;

    /**
     * @brief Writes a completed match to the output stream (thread-safe).
     *
     * Temporarily inserts the final (graph_vertex, subgraph_vertex) mapping,
     * serialises the path, then removes it before returning.
     *
     * @param context         Search context holding the current partial path.
     * @param graph_vertex    The final graph vertex in the match.
     * @param subgraph_vertex The final subgraph vertex in the match.
     */
    void write_match(SearchContext& context, uint32_t graph_vertex, uint32_t subgraph_vertex) const;

    /**
     * @brief Collects neighbours of @p vertex in @p graph with @p color and
     *        out-degree >= @p min_degree (undirected neighbourhood).
     * @param graph      Host graph to query.
     * @param vertex     Centre vertex.
     * @param color      Required vertex color.
     * @param min_degree Minimum degree of accepted neighbours.
     * @return Set of qualifying neighbour IDs.
     */
    static VertexSet colored_neighborhood(const ColoredGraph& graph, uint32_t vertex,
                                          uint32_t color, uint32_t min_degree);

    /**
     * @brief Collects out-neighbours of @p vertex with @p color and out-degree
     *        >= @p min_degree.
     * @param graph      Host graph to query.
     * @param vertex     Centre vertex.
     * @param color      Required vertex color.
     * @param min_degree Minimum out-degree of accepted neighbours.
     * @return Set of qualifying out-neighbour IDs.
     */
    static VertexSet colored_neighborhood_out(const ColoredGraph& graph, uint32_t vertex,
                                              uint32_t color, uint32_t min_degree);

    /**
     * @brief Collects in-neighbours of @p vertex with @p color and in-degree
     *        >= @p min_degree.
     * @param graph      Host graph to query.
     * @param vertex     Centre vertex.
     * @param color      Required vertex color.
     * @param min_degree Minimum in-degree of accepted neighbours.
     * @return Set of qualifying in-neighbour IDs.
     */
    static VertexSet colored_neighborhood_in(const ColoredGraph& graph, uint32_t vertex,
                                             uint32_t color, uint32_t min_degree);

    /**
     * @brief Returns vertices that are both in the out-neighbourhood and the
     *        in-neighbourhood of @p vertex, with matching color and degrees.
     * @param graph       Host graph.
     * @param vertex      Centre vertex.
     * @param color       Required vertex color.
     * @param min_deg_out Minimum out-degree of accepted vertices.
     * @param min_deg_in  Minimum in-degree of accepted vertices.
     * @return Intersection set.
     */
    static VertexSet intersect_neighborhoods(const ColoredGraph& graph, uint32_t vertex,
                                             uint32_t color, uint32_t min_deg_out,
                                             uint32_t min_deg_in);

    /**
     * @brief Computes the initial restriction set for a newly constrained
     *        subgraph vertex in the directed case.
     * @param context          Current search context.
     * @param subgraph_neighbor The subgraph vertex gaining a restriction.
     * @param subgraph_vertex  The currently matched subgraph vertex.
     * @param graph_vertex     The currently matched graph vertex.
     * @return The initial candidate set.
     */
    static VertexSet compute_new_restriction_directed(const SearchContext& context,
                                                      uint32_t subgraph_neighbor,
                                                      uint32_t subgraph_vertex,
                                                      uint32_t graph_vertex);

    /**
     * @brief Computes the initial restriction set for a newly constrained
     *        subgraph vertex (dispatches undirected / directed).
     * @param context          Current search context.
     * @param subgraph_neighbor The subgraph vertex gaining a restriction.
     * @param subgraph_vertex  The currently matched subgraph vertex.
     * @param graph_vertex     The currently matched graph vertex.
     * @return The initial candidate set.
     */
    VertexSet compute_new_restriction(const SearchContext& context, uint32_t subgraph_neighbor,
                                      uint32_t subgraph_vertex, uint32_t graph_vertex) const;

    /**
     * @brief Removes from @p restriction candidates that are not neighbours of
     *        @p graph_vertex, recording removals in @p inverse_entry.
     * @param graph        Host graph.
     * @param graph_vertex The currently matched graph vertex.
     * @param restriction  Candidate set to filter (modified in place).
     * @param inverse_entry Records removed candidates for rollback.
     */
    static void filter_restriction_undirected(const ColoredGraph& graph, uint32_t graph_vertex,
                                              VertexSet& restriction, VertexSet& inverse_entry);

    /**
     * @brief Removes candidates from a directed restriction that violate
     *        out-edge or in-edge constraints imposed by the current match.
     * @param context      Current search context.
     * @param params       Grouped filter parameters.
     * @param restriction  Candidate set to filter (modified in place).
     * @param inverse_entry Records removed candidates for rollback.
     */
    static void filter_restriction_directed(const SearchContext& context,
                                            const FilterParams& params, VertexSet& restriction,
                                            VertexSet& inverse_entry);

    /**
     * @brief Dispatches to directed or undirected restriction filtering.
     * @param context      Current search context.
     * @param params       Grouped filter parameters.
     * @param restriction  Candidate set to filter (modified in place).
     * @param inverse_entry Records removed candidates for rollback.
     */
    void filter_restriction(const SearchContext& context, const FilterParams& params,
                            VertexSet& restriction, VertexSet& inverse_entry) const;

    /**
     * @brief Removes candidates from @p restriction not reachable via out-edges
     *        from @p graph_vertex, storing removals in @p inverse_entry.
     * @param graph        Host graph.
     * @param graph_vertex Centre graph vertex.
     * @param restriction  Candidate set to filter.
     * @param inverse_entry Rollback store.
     */
    static void remove_non_out_neighbors(const ColoredGraph& graph, uint32_t graph_vertex,
                                         VertexSet& restriction, VertexSet& inverse_entry);

    /**
     * @brief Removes candidates from @p restriction that have no in-edge to
     *        @p graph_vertex, storing removals in @p inverse_entry.
     * @param graph        Host graph.
     * @param graph_vertex Centre graph vertex.
     * @param restriction  Candidate set to filter.
     * @param inverse_entry Rollback store.
     */
    static void remove_non_in_neighbors(const ColoredGraph& graph, uint32_t graph_vertex,
                                        VertexSet& restriction, VertexSet& inverse_entry);

    /**
     * @brief Moves @p to_remove entries from @p restriction into @p inverse_entry.
     * @param to_remove    Vertices to remove.
     * @param restriction  Source set (modified in place).
     * @param inverse_entry Destination rollback set.
     */
    static void apply_removal(const VertexSet& to_remove, VertexSet& restriction,
                              VertexSet& inverse_entry);

    /**
     * @brief Collects all vertices adjacent to @p vertex via any edge direction.
     *
     * For undirected graphs returns the standard neighbour list. For directed
     * graphs returns the union of out-neighbours and in-neighbours, so that
     * restriction propagation covers edges in both directions.
     *
     * @param subgraph The pattern graph to query.
     * @param vertex   Vertex whose full adjacency is requested.
     * @return Set of all adjacent vertex IDs.
     */
    VertexSet all_adjacent(const ColoredGraph& subgraph, uint32_t vertex) const;

    /**
     * @brief Updates restriction sets for all un-chosen neighbours of
     *        @p subgraph_vertex after matching it to @p graph_vertex.
     *
     * Each neighbour's restriction is either initialised (new) or narrowed
     * (existing). Returns a rollback map and an emptiness flag.
     *
     * @param context         Search context (restrictions modified in place).
     * @param graph_vertex    Graph vertex that was just matched.
     * @param subgraph_vertex Subgraph vertex that was just matched.
     * @return {inverse_restrictions, any_restriction_became_empty}.
     */
    std::pair<RestrictionMap, bool> update_restrictions(SearchContext& context,
                                                        uint32_t graph_vertex,
                                                        uint32_t subgraph_vertex) const;

    /**
     * @brief Updates a single neighbour's restriction entry.
     * @param context    Search context.
     * @param params     Grouped filter parameters (neighbor ID in params.subgraph_neighbor).
     * @param inverse    Inverse map being built.
     * @param is_empty   Set to true if the restriction becomes empty.
     */
    void update_restriction_entry(SearchContext& context, const FilterParams& params,
                                  RestrictionMap& inverse, bool& is_empty) const;

    /**
     * @brief Rolls back restrictions to their pre-update state.
     * @param context Search context whose restrictions are restored.
     * @param inverse The inverse map returned by update_restrictions.
     */
    static void restore_restrictions(SearchContext& context, RestrictionMap& inverse);

    /**
     * @brief Saves and removes the restriction entry for @p vertex.
     * @param context Search context.
     * @param vertex  Subgraph vertex whose restriction is saved and erased.
     * @return The saved candidate set (empty if no entry existed).
     */
    static VertexSet save_and_remove_restriction(SearchContext& context, uint32_t vertex);

    /**
     * @brief Iterates candidates for @p next_vertex and recurses the search.
     * @param context     Search context.
     * @param next_vertex The chosen next subgraph vertex.
     * @return Total matches found in this branch.
     */
    uint64_t recurse_candidates(SearchContext& context, uint32_t next_vertex) const;

    /**
     * @brief Continues the search after restrictions have been updated.
     * @param context  Search context.
     * @param is_empty True if any restriction became empty (prune immediately).
     * @return Total matches found in this branch.
     */
    uint64_t expand_if_feasible(SearchContext& context, bool is_empty) const;

    /**
     * @brief Expands the search from the current partial match.
     * @param context         Search context.
     * @param graph_vertex    Graph vertex just matched.
     * @param subgraph_vertex Subgraph vertex it was matched to.
     * @return Total matches found in this subtree.
     */
    uint64_t expand_search(SearchContext& context, uint32_t graph_vertex,
                           uint32_t subgraph_vertex) const;

    /**
     * @brief Core backtracking routine — tries matching @p graph_vertex to
     *        @p subgraph_vertex and recurses.
     * @param context         Per-thread search state.
     * @param graph_vertex    Candidate graph vertex to match.
     * @param subgraph_vertex Target subgraph vertex.
     * @return Number of complete matches found in this subtree.
     */
    uint64_t recursion_search(SearchContext& context, uint32_t graph_vertex,
                              uint32_t subgraph_vertex) const;

    PriorPolicy m_policy;
    bool m_directed;
    bool m_induced;
    std::ostream& m_output;
    mutable std::mutex m_output_mutex;
};

}  // namespace sgf
