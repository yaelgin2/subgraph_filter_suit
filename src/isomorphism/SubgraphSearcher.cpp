#include "SubgraphSearcher.h"

#include "ColoredGraph.h"
#include "MatchFoundException.h"
#include "PriorPolicy.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <ostream>
#include <random>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace sgf
{

namespace
{

/**
 * @brief Returns a uniformly distributed double in [0, 1) for the calling thread.
 * @return Random double in [0, 1).
 */
double random_score()
{
    thread_local std::mt19937 rng{std::random_device{}()};
    thread_local std::uniform_real_distribution<double> dist{0.0, 1.0};
    return dist(rng);
}

/**
 * @brief Returns a uniformly random vertex ID in [0, count).
 * @param count Total number of vertices.
 * @return Random vertex ID.
 */
uint32_t random_vertex(const uint32_t count)
{
    thread_local std::mt19937 rng{std::random_device{}()};
    return std::uniform_int_distribution<uint32_t>{0U, count - 1U}(rng);
}

}  // namespace

SubgraphSearcher::SubgraphSearcher(const PriorPolicy policy, const bool is_directed,
                                   const bool is_induced, std::ostream& output)
    : m_policy(policy)
    , m_directed(is_directed)
    , m_induced(is_induced)
    , m_output(output)
{
}

void SubgraphSearcher::join_all(std::vector<std::thread>& threads)
{
    for (auto& thread : threads)
    {
        thread.join();
    }
    threads.clear();
}

uint64_t SubgraphSearcher::score_second_degree_vertices(const ColoredGraph& graph,
                                                        const uint32_t vertex) const
{
    const VertexSet neighbors = all_adjacent(graph, vertex);
    uint64_t score = 0ULL;
    for (const uint32_t neighbor : neighbors)
    {
        score += graph.out_degree(neighbor);
        if (m_directed)
        {
            score += graph.in_degree(neighbor);
        }
    }
    return score;
}

SubgraphSearcher::PriorMap
SubgraphSearcher::calculate_prior_second_degree(const ColoredGraph& target) const
{
    PriorMap prior;
    for (uint32_t vertex = 0; vertex < target.vertex_count(); ++vertex)
    {
        prior[vertex] = static_cast<double>(score_second_degree_vertices(target, vertex));
    }
    return prior;
}

SubgraphSearcher::PriorMap
SubgraphSearcher::calculate_prior_first_degree(const ColoredGraph& target) const
{
    PriorMap prior;
    for (uint32_t vertex = 0; vertex < target.vertex_count(); ++vertex)
    {
        prior[vertex] = static_cast<double>(all_adjacent(target, vertex).size());
    }
    return prior;
}

SubgraphSearcher::PriorMap SubgraphSearcher::calculate_prior(const ColoredGraph& subgraph,
                                                             const ColoredGraph& graph,
                                                             const PriorPolicy policy) const
{
    if (policy == PriorPolicy::SUBGRAPH_DEGREE_SQUARED)
    {
        return calculate_prior_second_degree(subgraph);
    }
    if (policy == PriorPolicy::GRAPH_DEGREE_SQUARED)
    {
        return calculate_prior_second_degree(graph);
    }
    if (policy == PriorPolicy::SUBGRAPH_DEGREE)
    {
        return calculate_prior_first_degree(subgraph);
    }
    return {};
}

double SubgraphSearcher::score_graph_degree_squared(const RestrictionMap& restrictions,
                                                    const PriorMap& prior, const uint32_t vertex)
{
    // Sum the prior (host-graph second-degree) scores of all candidates still
    // allowed for this subgraph vertex. Returned as negative so that a vertex
    // with fewer / lower-scored candidates ranks higher (choose_next picks the
    // maximum score, and more-constrained vertices should be matched first).
    double score = 0.0;
    const RestrictionMap::const_iterator restriction_iter = restrictions.find(vertex);
    if (restriction_iter != restrictions.end())
    {
        for (const auto& instance : restriction_iter->second)
        {
            if (prior.count(instance) != 0U)
            {
                score += prior.at(instance);
            }
        }
    }
    return -score;
}

double SubgraphSearcher::restriction_score(const RestrictionMap& restrictions,
                                           const PriorMap& prior, const uint32_t vertex) const
{
    switch (m_policy)
    {
    case PriorPolicy::SUBGRAPH_DEGREE_SQUARED:
    case PriorPolicy::SUBGRAPH_DEGREE:
    case PriorPolicy::COMBINED:
        return prior.count(vertex) != 0U ? prior.at(vertex) : 0.0;
    case PriorPolicy::GRAPH_DEGREE_SQUARED:
        return score_graph_degree_squared(restrictions, prior, vertex);
    case PriorPolicy::CONSTANT:
    {
        // Score = negative number of remaining candidates for this subgraph vertex.
        // All graph vertices are treated as equal (no prior), so the only signal
        // is how many candidates are still allowed — fewer is better (more constrained).
        // Returned as double to share the same return type as all other policies;
        // cast from size_t is lossless for any realistic candidate set size.
        const RestrictionMap::const_iterator restriction_iter = restrictions.find(vertex);
        const double size = restriction_iter != restrictions.end()
                                ? static_cast<double>(restriction_iter->second.size())
                                : 0.0;
        return -size;
    }
    case PriorPolicy::RANDOM:
        return random_score();
    }
    return 0.0;
}

uint32_t SubgraphSearcher::choose_start(const ColoredGraph& subgraph, const PriorMap& prior) const
{
    if (m_policy == PriorPolicy::CONSTANT || m_policy == PriorPolicy::GRAPH_DEGREE_SQUARED)
    {
        return random_vertex(subgraph.vertex_count());
    }
    static const RestrictionMap empty_restrictions{};
    double max_score = NEGATIVE_INFINITY;
    uint32_t best_vertex = 0U;
    for (uint32_t vertex = 0; vertex < subgraph.vertex_count(); ++vertex)
    {
        const double score = restriction_score(empty_restrictions, prior, vertex);
        if (score > max_score)
        {
            max_score = score;
            best_vertex = vertex;
        }
    }
    return best_vertex;
}

uint32_t SubgraphSearcher::find_unchosen_vertex(const ColoredGraph& subgraph,
                                                const VertexSet& chosen)
{
    for (uint32_t vertex = 0; vertex < subgraph.vertex_count(); ++vertex)
    {
        if (chosen.count(vertex) == 0U)
        {
            return vertex;
        }
    }
    return INVALID_VERTEX_ID;
}

uint32_t SubgraphSearcher::choose_next(const RestrictionMap& restrictions, const VertexSet& chosen,
                                       const ColoredGraph& subgraph, const PriorMap& prior) const
{
    double max_score = NEGATIVE_INFINITY;
    uint32_t best_vertex = INVALID_VERTEX_ID;
    for (const RestrictionMap::value_type& restriction_entry : restrictions)
    {
        const uint32_t vertex = restriction_entry.first;
        if (chosen.count(vertex) != 0U)
        {
            continue;
        }
        if (restriction_entry.second.size() <= 1U)
        {
            return vertex;
        }
        const double score = restriction_score(restrictions, prior, vertex);
        if (score > max_score)
        {
            max_score = score;
            best_vertex = vertex;
        }
    }
    if (best_vertex != INVALID_VERTEX_ID)
    {
        return best_vertex;
    }
    return find_unchosen_vertex(subgraph, chosen);
}

bool SubgraphSearcher::check_induced(const SearchContext& context, const uint32_t graph_vertex,
                                     const uint32_t subgraph_vertex, const bool reversed)
{
    const std::pair<std::vector<uint32_t>::const_iterator, std::vector<uint32_t>::const_iterator>
        neighbours_range = context.m_graph.get_neighbours(graph_vertex, reversed);
    for (std::vector<uint32_t>::const_iterator neighbours_iter = neighbours_range.first;
         neighbours_iter != neighbours_range.second; ++neighbours_iter)
    {
        const PathMap::const_iterator path_it = context.m_path.find(*neighbours_iter);
        if (path_it == context.m_path.end())
        {
            continue;
        }
        const bool edge_exists = reversed
                                     ? context.m_subgraph.is_edge(path_it->second, subgraph_vertex)
                                     : context.m_subgraph.is_edge(subgraph_vertex, path_it->second);
        if (!edge_exists)
        {
            return false;
        }
    }
    return true;
}

bool SubgraphSearcher::is_induced_valid(const SearchContext& context, const uint32_t graph_vertex,
                                        const uint32_t subgraph_vertex) const
{
    if (m_directed)
    {
        return check_induced(context, graph_vertex, subgraph_vertex, false) &&
               check_induced(context, graph_vertex, subgraph_vertex, true);
    }
    return check_induced(context, graph_vertex, subgraph_vertex, false);
}

void SubgraphSearcher::write_match(SearchContext& context, const uint32_t graph_vertex,
                                   const uint32_t subgraph_vertex) const
{
    context.m_path[graph_vertex] = subgraph_vertex;
    std::ostringstream oss;
    oss << "{";
    for (const auto& mapping : context.m_path)
    {
        oss << mapping.first << ":" << mapping.second << " ";
    }
    oss << "}";
    context.m_path.erase(graph_vertex);
    const std::lock_guard<std::mutex> lock{m_output_mutex};
    m_output << oss.str() << "\n";
    if (context.m_stop_after_first)
    {
        throw MatchFoundException{};
    }
}

SubgraphSearcher::VertexSet SubgraphSearcher::colored_neighborhood(const ColoredGraph& graph,
                                                                   const uint32_t vertex,
                                                                   const uint32_t color,
                                                                   const uint32_t min_degree,
                                                                   const bool reversed)
{
    VertexSet result;
    const std::pair<std::vector<uint32_t>::const_iterator, std::vector<uint32_t>::const_iterator>
        neighbours_range = graph.get_neighbours(vertex, reversed);
    for (std::vector<uint32_t>::const_iterator neighbours_iter = neighbours_range.first;
         neighbours_iter != neighbours_range.second; ++neighbours_iter)
    {
        const uint32_t neighbor = *neighbours_iter;
        if (graph.get_vertex_color(neighbor) != color)
        {
            continue;
        }
        const uint32_t degree = reversed ? graph.in_degree(neighbor) : graph.out_degree(neighbor);
        if (degree >= min_degree)
        {
            result.insert(neighbor);
        }
    }
    return result;
}

SubgraphSearcher::VertexSet SubgraphSearcher::intersect_neighborhoods(const ColoredGraph& graph,
                                                                      const uint32_t vertex,
                                                                      const uint32_t color,
                                                                      const uint32_t min_deg_out,
                                                                      const uint32_t min_deg_in)
{
    const VertexSet out_set = colored_neighborhood(graph, vertex, color, min_deg_out, false);
    VertexSet result;
    const std::pair<std::vector<uint32_t>::const_iterator, std::vector<uint32_t>::const_iterator>
        in_range = graph.get_neighbours(vertex, true);
    for (std::vector<uint32_t>::const_iterator in_iter = in_range.first; in_iter != in_range.second;
         ++in_iter)
    {
        const uint32_t neighbor = *in_iter;
        if (graph.get_vertex_color(neighbor) != color)
        {
            continue;
        }
        if (graph.in_degree(neighbor) < min_deg_in)
        {
            continue;
        }
        if (out_set.count(neighbor) != 0U)
        {
            result.insert(neighbor);
        }
    }
    return result;
}

SubgraphSearcher::VertexSet SubgraphSearcher::compute_new_restriction_directed(
    const SearchContext& context, const uint32_t subgraph_neighbor, const uint32_t subgraph_vertex,
    const uint32_t graph_vertex)
{
    const bool has_out_edge = context.m_subgraph.is_edge(subgraph_vertex, subgraph_neighbor);
    const bool has_in_edge = context.m_subgraph.is_edge(subgraph_neighbor, subgraph_vertex);
    const uint32_t color = context.m_subgraph.get_vertex_color(subgraph_neighbor);
    if (has_out_edge && !has_in_edge)
    {
        return colored_neighborhood(context.m_graph, graph_vertex, color,
                                    context.m_subgraph.out_degree(subgraph_neighbor), false);
    }
    if (has_in_edge && !has_out_edge)
    {
        return colored_neighborhood(context.m_graph, graph_vertex, color,
                                    context.m_subgraph.in_degree(subgraph_neighbor), true);
    }
    return intersect_neighborhoods(context.m_graph, graph_vertex, color,
                                   context.m_subgraph.out_degree(subgraph_neighbor),
                                   context.m_subgraph.in_degree(subgraph_neighbor));
}

SubgraphSearcher::VertexSet SubgraphSearcher::compute_new_restriction(
    const SearchContext& context, const uint32_t subgraph_neighbor, const uint32_t subgraph_vertex,
    const uint32_t graph_vertex) const
{
    if (m_directed)
    {
        return compute_new_restriction_directed(context, subgraph_neighbor, subgraph_vertex,
                                                graph_vertex);
    }
    const uint32_t color = context.m_subgraph.get_vertex_color(subgraph_neighbor);
    const uint32_t min_deg = context.m_subgraph.out_degree(subgraph_neighbor);
    return colored_neighborhood(context.m_graph, graph_vertex, color, min_deg, false);
}

void SubgraphSearcher::apply_removal(const VertexSet& to_remove, VertexSet& restriction,
                                     VertexSet& inverse_restrictions_entry)
{
    for (const auto& candidate : to_remove)
    {
        inverse_restrictions_entry.insert(candidate);
        restriction.erase(candidate);
    }
}

void SubgraphSearcher::remove_non_neighbors(const ColoredGraph& graph, const uint32_t graph_vertex,
                                            VertexSet& restriction,
                                            VertexSet& inverse_restrictions_entry,
                                            const bool reversed)
{
    VertexSet to_remove;
    for (const auto& candidate : restriction)
    {
        const bool edge_exists = reversed ? graph.is_edge(candidate, graph_vertex)
                                          : graph.is_edge(graph_vertex, candidate);
        if (!edge_exists)
        {
            to_remove.insert(candidate);
        }
    }
    apply_removal(to_remove, restriction, inverse_restrictions_entry);
}

void SubgraphSearcher::filter_restriction_directed(const SearchContext& context,
                                                   const FilterParams& params,
                                                   VertexSet& restriction,
                                                   VertexSet& inverse_restrictions_entry)
{
    if (context.m_subgraph.is_edge(params.m_subgraph_vertex, params.m_subgraph_neighbor))
    {
        remove_non_neighbors(context.m_graph, params.m_graph_vertex, restriction,
                             inverse_restrictions_entry, false);
    }
    if (context.m_subgraph.is_edge(params.m_subgraph_neighbor, params.m_subgraph_vertex))
    {
        remove_non_neighbors(context.m_graph, params.m_graph_vertex, restriction,
                             inverse_restrictions_entry, true);
    }
}

void SubgraphSearcher::filter_restriction(const SearchContext& context, const FilterParams& params,
                                          VertexSet& restriction,
                                          VertexSet& inverse_restrictions_entry) const
{
    if (m_directed)
    {
        filter_restriction_directed(context, params, restriction, inverse_restrictions_entry);
        return;
    }
    remove_non_neighbors(context.m_graph, params.m_graph_vertex, restriction,
                         inverse_restrictions_entry, false);
}

void SubgraphSearcher::update_restriction_entry(SearchContext& context, const FilterParams& params,
                                                RestrictionMap& inverse,
                                                bool& any_candidate_set_empty) const
{
    const uint32_t neighbor = params.m_subgraph_neighbor;
    const bool was_new = context.m_restrictions.count(neighbor) == 0U;
    VertexSet& restriction = context.m_restrictions[neighbor];
    VertexSet& inverse_restrictions_entry = inverse[neighbor];
    if (was_new)
    {
        inverse_restrictions_entry.insert(INVALID_VERTEX_ID);
        restriction = compute_new_restriction(context, neighbor, params.m_subgraph_vertex,
                                              params.m_graph_vertex);
    }
    else
    {
        filter_restriction(context, params, restriction, inverse_restrictions_entry);
    }
    if (restriction.empty())
    {
        any_candidate_set_empty = true;
    }
}

SubgraphSearcher::VertexSet SubgraphSearcher::all_adjacent(const ColoredGraph& subgraph,
                                                           const uint32_t vertex) const
{
    VertexSet adjacent;
    const std::pair<std::vector<uint32_t>::const_iterator, std::vector<uint32_t>::const_iterator>
        out_range = subgraph.get_neighbours(vertex, false);
    for (std::vector<uint32_t>::const_iterator out_iter = out_range.first;
         out_iter != out_range.second; ++out_iter)
    {
        adjacent.insert(*out_iter);
    }
    if (m_directed)
    {
        const std::pair<std::vector<uint32_t>::const_iterator,
                        std::vector<uint32_t>::const_iterator>
            in_range = subgraph.get_neighbours(vertex, true);
        for (std::vector<uint32_t>::const_iterator in_iter = in_range.first;
             in_iter != in_range.second; ++in_iter)
        {
            adjacent.insert(*in_iter);
        }
    }
    return adjacent;
}

std::pair<SubgraphSearcher::RestrictionMap, bool>
SubgraphSearcher::update_restrictions(SearchContext& context, const uint32_t graph_vertex,
                                      const uint32_t subgraph_vertex) const
{
    RestrictionMap inverse;
    bool any_candidate_set_empty = false;
    const std::pair<std::vector<uint32_t>::const_iterator, std::vector<uint32_t>::const_iterator>
        out_range = context.m_subgraph.get_neighbours(subgraph_vertex, false);
    for (std::vector<uint32_t>::const_iterator out_neighbor_it = out_range.first;
         out_neighbor_it != out_range.second; ++out_neighbor_it)
    {
        if (context.m_chosen.count(*out_neighbor_it) == 0U)
        {
            const FilterParams params{*out_neighbor_it, subgraph_vertex, graph_vertex};
            update_restriction_entry(context, params, inverse, any_candidate_set_empty);
        }
    }
    if (m_directed)
    {
        const std::pair<std::vector<uint32_t>::const_iterator,
                        std::vector<uint32_t>::const_iterator>
            in_range = context.m_subgraph.get_neighbours(subgraph_vertex, true);
        for (std::vector<uint32_t>::const_iterator in_neighbor_it = in_range.first;
             in_neighbor_it != in_range.second; ++in_neighbor_it)
        {
            if (context.m_chosen.count(*in_neighbor_it) == 0U &&
                inverse.count(*in_neighbor_it) == 0U)
            {
                const FilterParams params{*in_neighbor_it, subgraph_vertex, graph_vertex};
                update_restriction_entry(context, params, inverse, any_candidate_set_empty);
            }
        }
    }
    return {std::move(inverse), any_candidate_set_empty};
}

void SubgraphSearcher::restore_restrictions(SearchContext& context, RestrictionMap& inverse)
{
    for (auto& entry : inverse)
    {
        const uint32_t subgraph_vertex = entry.first;
        const VertexSet& inverse_set = entry.second;
        if (inverse_set.count(INVALID_VERTEX_ID) != 0U)
        {
            context.m_restrictions.erase(subgraph_vertex);
        }
        else
        {
            for (const auto& candidate : inverse_set)
            {
                context.m_restrictions[subgraph_vertex].insert(candidate);
            }
        }
    }
}

SubgraphSearcher::VertexSet SubgraphSearcher::save_and_remove_restriction(SearchContext& context,
                                                                          const uint32_t vertex)
{
    const RestrictionMap::iterator restriction_iter = context.m_restrictions.find(vertex);
    if (restriction_iter == context.m_restrictions.end())
    {
        return {};
    }
    VertexSet saved = std::move(restriction_iter->second);
    context.m_restrictions.erase(restriction_iter);
    return saved;
}

//  NOLINTNEXTLINE(misc-no-recursion)
uint64_t SubgraphSearcher::recurse_candidates(SearchContext& context,
                                              const uint32_t next_vertex) const
{
    if (context.m_restrictions.count(next_vertex) == 0U)
    {
        return 0ULL;
    }
    const VertexSet candidates = context.m_restrictions.at(next_vertex);
    uint64_t count_matches = 0ULL;
    for (const auto& candidate : candidates)
    {
        count_matches += recursion_search(context, candidate, next_vertex);
    }
    return count_matches;
}

//  NOLINTNEXTLINE(misc-no-recursion)
uint64_t SubgraphSearcher::expand_if_feasible(SearchContext& context,
                                              const bool any_candidate_set_empty) const
{
    if (any_candidate_set_empty)
    {
        return 0ULL;
    }
    const uint32_t next_vertex =
        choose_next(context.m_restrictions, context.m_chosen, context.m_subgraph, context.m_prior);
    return recurse_candidates(context, next_vertex);
}

//  NOLINTNEXTLINE(misc-no-recursion)
uint64_t SubgraphSearcher::expand_search(SearchContext& context, const uint32_t graph_vertex,
                                         const uint32_t subgraph_vertex) const
{
    context.m_path[graph_vertex] = subgraph_vertex;
    context.m_chosen.insert(subgraph_vertex);

    // Remove subgraph_vertex's own restriction (it is now matched) and propagate
    // the new assignment to neighbours, narrowing their candidate sets.
    VertexSet saved = save_and_remove_restriction(context, subgraph_vertex);
    std::pair<RestrictionMap, bool> update_result =
        update_restrictions(context, graph_vertex, subgraph_vertex);
    update_result.first[subgraph_vertex] = std::move(saved);

    // Recurse, then roll back all changes for backtracking.
    const uint64_t match_count = expand_if_feasible(context, update_result.second);
    restore_restrictions(context, update_result.first);
    context.m_chosen.erase(subgraph_vertex);
    context.m_path.erase(graph_vertex);
    return match_count;
}

//  NOLINTNEXTLINE(misc-no-recursion)
uint64_t SubgraphSearcher::recursion_search(SearchContext& context, const uint32_t graph_vertex,
                                            const uint32_t subgraph_vertex) const
{
    if (m_stop.load(std::memory_order_relaxed))
    {
        return 0ULL;
    }
    if (context.m_path.count(graph_vertex) != 0U)
    {
        return 0ULL;
    }
    if (m_induced && !is_induced_valid(context, graph_vertex, subgraph_vertex))
    {
        return 0ULL;
    }
    if (context.m_chosen.size() + 1U == context.m_subgraph.vertex_count())
    {
        write_match(context, graph_vertex, subgraph_vertex);
        return 1ULL;
    }
    return expand_search(context, graph_vertex, subgraph_vertex);
}

uint64_t SubgraphSearcher::find_all(const ColoredGraph& graph, const ColoredGraph& subgraph,
                                    const bool stop_after_first) const
{
    m_stop = false;
    const PriorMap prior = calculate_prior(subgraph, graph, m_policy);
    const uint32_t start_vertex = choose_start(subgraph, prior);
    const uint32_t start_color = subgraph.get_vertex_color(start_vertex);
    std::atomic<uint64_t> counter{0ULL};
    std::vector<std::thread> threads;
    for (uint32_t vertex = 0; vertex < graph.vertex_count(); ++vertex)
    {
        if (stop_after_first && m_stop)
        {
            break;
        }
        if (graph.get_vertex_color(vertex) != start_color)
        {
            continue;
        }
        threads.emplace_back(
            [this, &graph, &subgraph, &prior, &counter, stop_after_first, vertex, start_vertex]()
            {
                SearchContext ctx{graph, subgraph, prior, {}, {}, {}, stop_after_first};
                try
                {
                    counter.fetch_add(recursion_search(ctx, vertex, start_vertex));
                }
                catch (const MatchFoundException&)
                {
                    m_stop = true;
                    counter.fetch_add(1ULL);
                }
            });
        if (threads.size() >= BATCH_SIZE)
        {
            join_all(threads);
        }
    }
    join_all(threads);
    return counter.load();
}

}  // namespace sgf
