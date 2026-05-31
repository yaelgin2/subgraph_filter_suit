#include "SubgraphSearcher.h"
#include "ColoredGraph.h"
#include "LoggerHandler.h"
#include "MatchOutputWriter.h"
#include "PriorPolicy.h"
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <mutex>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <chrono>
#include <iostream>
#include <set>
using namespace sgf;

// ── Graph-building helpers ────────────────────────────────────────────────────

namespace
{



std::vector<uint32_t> make_random_colors(
    const uint32_t vertex_count,
    const uint32_t min_color,
    const uint32_t max_color,
    const uint32_t seed = 42U)
{
    if (min_color > max_color)
    {
        throw std::invalid_argument(
            "min_color must be smaller than or equal to max_color");
    }

    std::mt19937 generator(seed);

    std::uniform_int_distribution<uint32_t> color_distribution(
        min_color,
        max_color
    );

    std::vector<uint32_t> colors;
    colors.reserve(vertex_count);

    for (uint32_t vertex = 0U; vertex < vertex_count; ++vertex)
    {
        colors.push_back(color_distribution(generator));
    }

    return colors;
}

/**
 * @brief Creates a new graph by planting several disjoint copies of a pattern
 *        inside an existing host graph.
 *
 * Each planted copy receives its own separate range of vertex IDs, so the
 * copies never overlap with one another or with the original host graph.
 *
 * Example:
 *   host_size = 1000
 *   pattern_size = 8
 *   number_of_copies = 5
 *
 * Vertex ranges:
 *   original host: 0    - 999
 *   copy 0:        1000 - 1007
 *   copy 1:        1008 - 1015
 *   copy 2:        1016 - 1023
 *   copy 3:        1024 - 1031
 *   copy 4:        1032 - 1039
 *
 * @param host_size Number of vertices in the original host graph.
 * @param host_edges Edges of the original host graph.
 * @param host_colors Colors of the original host graph vertices.
 * @param pattern_edges Edges of the small graph that should be planted.
 * @param pattern_colors Colors of the small graph vertices.
 * @param number_of_copies Number of disjoint copies to plant.
 * @param directed Whether the resulting graph should be treated as directed.
 *
 * @return A new ColoredGraph containing the original host graph and all
 *         planted copies.
 */
ColoredGraph plant_graph_copies(
    const uint32_t host_size,
    std::vector<std::pair<uint32_t, uint32_t>> host_edges,
    std::vector<uint32_t> host_colors,
    const std::vector<std::pair<uint32_t, uint32_t>>& pattern_edges,
    const std::vector<uint32_t>& pattern_colors,
    const uint32_t number_of_copies,
    const bool directed)
{
    const uint32_t pattern_size =
        static_cast<uint32_t>(pattern_colors.size());

    for (uint32_t copy = 0U; copy < number_of_copies; ++copy)
    {
        // Each planted copy receives a separate range of vertex IDs.
        //
        // Example:
        // host_size = 1000
        // pattern_size = 8
        //
        // copy 0: vertices 1000 - 1007
        // copy 1: vertices 1008 - 1015
        // copy 2: vertices 1016 - 1023
        const uint32_t offset =
            host_size + copy * pattern_size;

        // Add the vertex colors of the current planted copy.
        host_colors.insert(
            host_colors.end(),
            pattern_colors.begin(),
            pattern_colors.end()
        );

        // Add the edges of the planted pattern.
        // The offset shifts the local vertex IDs of the pattern
        // into the current copy's separate range.
        for (const auto& [source, target] : pattern_edges)
        {
            host_edges.emplace_back(
                offset + source,
                offset + target
            );
        }

        // Connect the planted copy to the original host graph.
        //
        // copy 0 is connected to host vertex 0.
        // copy 1 is connected to host vertex 1.
        // copy 2 is connected to host vertex 2.
        //
        // The first vertex of the planted pattern is offset.
        host_edges.emplace_back(
            copy % host_size,
            offset
        );
    }

    // Total number of vertices after adding all planted copies.
    const uint32_t total_vertices =
        host_size + number_of_copies * pattern_size;

    return {
        total_vertices,
        host_edges,
        host_colors,
        directed
    };
}

/**
 * @brief Builds sparse Erdos-Renyi G(n,p) edges efficiently.
 *
 * Instead of checking every possible pair of vertices, the function skips
 * absent edges using a geometric distribution.
 */
std::vector<std::pair<uint32_t, uint32_t>>
make_sparse_gnp_edges(
    const uint32_t vertex_count,
    const double expected_average_degree,
    const bool directed,
    const uint32_t seed = 42U)
{
    if (vertex_count < 2U)
    {
        throw std::invalid_argument(
            "vertex_count must be at least 2");
    }

    const double edge_probability =
        expected_average_degree /
        static_cast<double>(vertex_count - 1U);

    if (edge_probability < 0.0 || edge_probability > 1.0)
    {
        throw std::invalid_argument(
            "invalid expected average degree");
    }

    std::mt19937 generator(seed);

    std::geometric_distribution<uint64_t> skipped_non_edges(
        edge_probability
    );

    const uint64_t expected_edge_count =
        directed
            ? static_cast<uint64_t>(
                  vertex_count * expected_average_degree
              )
            : static_cast<uint64_t>(
                  vertex_count * expected_average_degree / 2.0
              );

    std::vector<std::pair<uint32_t, uint32_t>> edges;
    edges.reserve(expected_edge_count);

    if (!directed)
    {
        for (uint32_t source = 0U;
             source < vertex_count;
             ++source)
        {
            uint64_t target =
                static_cast<uint64_t>(source) + 1ULL;

            while (target < vertex_count)
            {
                target += skipped_non_edges(generator);

                if (target < vertex_count)
                {
                    edges.emplace_back(
                        source,
                        static_cast<uint32_t>(target)
                    );

                    ++target;
                }
            }
        }

        return edges;
    }

    const uint64_t possible_targets =
        static_cast<uint64_t>(vertex_count) - 1ULL;

    for (uint32_t source = 0U;
         source < vertex_count;
         ++source)
    {
        uint64_t candidate = 0ULL;

        while (candidate < possible_targets)
        {
            candidate += skipped_non_edges(generator);

            if (candidate < possible_targets)
            {
                const uint32_t target =
                    candidate < source
                        ? static_cast<uint32_t>(candidate)
                        : static_cast<uint32_t>(candidate + 1ULL);

                edges.emplace_back(source, target);

                ++candidate;
            }
        }
    }

    return edges;
}

/**
 * @brief Builds a directed graph with a single edge 0→1.
 * @return Directed single-edge graph, both vertices colored 0.
 */
ColoredGraph make_directed_single_edge()
{
    std::vector<std::pair<uint32_t, uint32_t>> edges{{0U, 1U}};
    const std::vector<uint32_t> colors{0U, 0U};
    return {2U, edges, colors, true};
}

/**
 * @brief MatchOutputWriter that captures written matches into an in-memory string.
 *
 * Used in tests to inspect match output without touching the filesystem.
 */
class StringMatchOutputWriter : public MatchOutputWriter
{
public:
    StringMatchOutputWriter() = default;

    /**
     * @brief Appends @p match and a newline to the internal capture buffer.
     * @param match Formatted match string.
     */
    void write_match(const std::string& match)
    {
        const std::lock_guard<std::mutex> lock{m_capture_mutex};
        m_captured += match;
        m_captured += "\n";
    }

    /**
     * @brief Returns the full captured output.
     * @return All match lines concatenated.
     */
    [[nodiscard]] std::string captured() const
    {
        const std::lock_guard<std::mutex> lock{m_capture_mutex};
        return m_captured;
    }

private:
    std::string m_captured;
    mutable std::mutex m_capture_mutex;
};

/**
 * @brief Counts newlines in @p text as a proxy for match count.
 * @param text Output string from the searcher.
 * @return Number of matches (newlines).
 */
uint64_t count_matches(const std::string& text)
{
    uint64_t count = 0ULL;
    for (const char ch : text)
    {
        if (ch == '\n')
        {
            ++count;
        }
    }
    return count;
}

/**
 * @brief Creates a null MatchOutputWriter for tests that only check match count.
 * @return unique_ptr to a StringMatchOutputWriter (output ignored).
 */
std::unique_ptr<MatchOutputWriter> make_null_writer()
{
    return std::make_unique<StringMatchOutputWriter>();
}


TEST(
    LargeGraphSearcherTest,
    five_planted_colored_patterns_induced_in_large_directed_gnp_graph)
{
    constexpr uint32_t host_size = 1000000U;
    constexpr uint32_t pattern_size = 3000U;
    constexpr uint32_t number_of_copies = 5U;

    const std::vector<std::pair<uint32_t, uint32_t>> host_edges =
    make_sparse_gnp_edges(
        host_size,
        10.0,
        true,  // directed
        42U
    );

    // All background vertices have color 0.
    std::vector<uint32_t> host_colors(host_size, 0U);

    // Create a small directed colored graph.
    std::vector<std::pair<uint32_t, uint32_t>> pattern_edges;

    for (uint32_t source = 0U; source < pattern_size; ++source)
    {
        for (uint32_t target = source + 1U;
             target < pattern_size;
             ++target)
        {
            pattern_edges.emplace_back(source, target);
        }
    }

    const std::vector<uint32_t> pattern_colors =
        make_random_colors(
            pattern_size,
            1000U,
            9999U,
            123U
        );

    const ColoredGraph graph =
        plant_graph_copies(
            host_size,
            host_edges,
            host_colors,
            pattern_edges,
            pattern_colors,
            number_of_copies,
            true  // directed
        );

    ColoredGraph subgraph{
        pattern_size,
        pattern_edges,
        pattern_colors,
        true  // directed
    };

    const SubgraphSearcher searcher{
        PriorPolicy::SUBGRAPH_DEGREE,
        true,   // directed
        true,  // induced
        make_null_writer(),
        LoggerHandler::null()
    };

    EXPECT_EQ(searcher.find_all(graph, subgraph), 5ULL);
}


TEST(
    LargeGraphSearcherTest,
    five_planted_colored_patterns_in_large_undirected_gnp_graph)
{
    constexpr uint32_t host_size = 1000000U;
    constexpr uint32_t pattern_size = 6000U;
    constexpr uint32_t number_of_copies = 3U;

    const auto start_gnp =
    std::chrono::steady_clock::now();

    const std::vector<std::pair<uint32_t, uint32_t>> host_edges =
        make_sparse_gnp_edges(
            host_size,
            10.0,
            false,  // undirected
            42U
        );

    const auto end_gnp =
        std::chrono::steady_clock::now();

    std::cout
        << "GNP generation: "
        << std::chrono::duration_cast<std::chrono::seconds>(
            end_gnp - start_gnp
        ).count()
        << " seconds\n";

    // All background vertices have color 0.
    std::vector<uint32_t> host_colors(host_size, 0U);

    std::vector<std::pair<uint32_t, uint32_t>> pattern_edges=
    make_sparse_gnp_edges(
        pattern_size,
        10.0,
        false,  // undirected
        777U
    );

    const std::vector<uint32_t> pattern_colors =
    make_random_colors(
        pattern_size,
        0U,
        20U,
        123U
    );


    const ColoredGraph graph =
        plant_graph_copies(
            host_size,
            host_edges,
            host_colors,
            pattern_edges,
            pattern_colors,
            number_of_copies,
            false  // undirected
        );

    ColoredGraph subgraph{
        pattern_size,
        pattern_edges,
        pattern_colors,
        false  // undirected
    };

    const SubgraphSearcher searcher{
        PriorPolicy::SUBGRAPH_DEGREE,
        false,  // undirected
        false,  // non-induced
        make_null_writer(),
        LoggerHandler::null()
    };

    const auto start_search =
    std::chrono::steady_clock::now();

const uint64_t matches =
    searcher.find_all(graph, subgraph);

const auto end_search =
    std::chrono::steady_clock::now();

std::cout
    << "Search: "
    << std::chrono::duration_cast<std::chrono::seconds>(
           end_search - start_search
       ).count()
    << " seconds\n";

EXPECT_EQ(matches, 3ULL);
}

}  // namespace
