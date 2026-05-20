#include "MultiGraphPatternPreprocessor.h"

#include "ColoredGraph.h"

#include <cstdint>
#include <gtest/gtest.h>
#include <stdexcept>
#include <unordered_set>
#include <vector>

using namespace sgf;

namespace
{

constexpr uint32_t NO_THRESHOLD = 0U;

ColoredGraph make_empty_graph()
{
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    std::vector<uint32_t> colors;
    return ColoredGraph(0U, edges, colors, false);
}

ColoredGraph make_path_4()
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {1U, 2U}, {2U, 3U}};
    std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    return ColoredGraph(4U, edges, colors, false);
}

static GraphSpec make_path_4_with_1_added_vertex()
{
    return {{{0U, 1U}, {2U, 1U}, {2U, 3U}, {1U, 4U}}, {0U, 0U, 0U, 0U, 0U}, false};
}

static GraphSpec make_path_4_with_2_added_vertex()
{
    return {{{0U, 1U}, {2U, 1U}, {2U, 3U}, {1U, 4U}, {4U, 5U}}, {0U, 0U, 0U, 0U, 0U, 0U}, false};
}

/**
 * @brief Return true if every result's alive-set covers at least @p alive_precent percent
 *        of @p total_graphs.
 */
bool all_results_cover_fraction(const std::vector<MultiGraphPatternResult>& results,
                                const uint32_t total_graphs, const uint32_t alive_precent)
{
    const uint32_t min_alive =
        (alive_precent * total_graphs + 99U) / 100U;  // ceil(alive_precent% * total)
    for (const MultiGraphPatternResult& result : results)
    {
        if (static_cast<uint32_t>(result.second.size()) < min_alive)
        {
            return false;
        }
    }
    return true;
}

}  // namespace

// ── Empty / degenerate libraries ─────────────────────────────────────────────

/**
 * @brief Empty library (0 graphs) must throw.
 */
TEST(MultiGraphPatternPreprocessorTest, empty_library_throws)
{
    std::vector<ColoredGraph> graphs;
    MultiGraphPatternPreprocessor preprocessor(graphs, false);
    EXPECT_THROW(preprocessor.calculate(1U, NO_THRESHOLD), std::runtime_error);
}

/**
 * @brief Library with a single empty graph must throw.
 */
TEST(MultiGraphPatternPreprocessorTest, one_empty_graph_throws)
{
    std::vector<ColoredGraph> graphs;
    graphs.push_back(make_empty_graph());
    MultiGraphPatternPreprocessor preprocessor(graphs, false);
    EXPECT_THROW(preprocessor.calculate(1U, NO_THRESHOLD), std::runtime_error);
}

/**
 * @brief Library with two empty graphs must throw.
 */
TEST(MultiGraphPatternPreprocessorTest, two_empty_graphs_throws)
{
    std::vector<ColoredGraph> graphs;
    graphs.push_back(make_empty_graph());
    graphs.push_back(make_empty_graph());
    MultiGraphPatternPreprocessor preprocessor(graphs, false);
    EXPECT_THROW(preprocessor.calculate(1U, NO_THRESHOLD), std::runtime_error);
}

// ── Single non-empty graph ────────────────────────────────────────────────────

/**
 * @brief One non-empty graph: one pattern requested, alive set must cover 100 % (graph 0).
 */
TEST(MultiGraphPatternPreprocessorTest, one_graph_one_pattern_covers_all)
{
    std::vector<ColoredGraph> graphs;
    graphs.push_back(make_path_4());

    MultiGraphPatternPreprocessor preprocessor(graphs, false);
    const std::vector<MultiGraphPatternResult> results = preprocessor.calculate(1U, NO_THRESHOLD);

    ASSERT_EQ(results.size(), 1U);
    EXPECT_TRUE(all_results_cover_fraction(results, static_cast<uint32_t>(graphs.size()), 100U));
}

// ── Two identical non-empty graphs ───────────────────────────────────────────

/**
 * @brief Two identical graphs: one pattern must appear in both (100 %).
 */
TEST(MultiGraphPatternPreprocessorTest, two_graphs_one_pattern_covers_all)
{
    std::vector<ColoredGraph> graphs;
    graphs.push_back(make_path_4());
    graphs.push_back(make_path_4());

    MultiGraphPatternPreprocessor preprocessor(graphs, false);
    const std::vector<MultiGraphPatternResult> results = preprocessor.calculate(1U, NO_THRESHOLD);

    ASSERT_EQ(results.size(), 1U);
    EXPECT_TRUE(all_results_cover_fraction(results, static_cast<uint32_t>(graphs.size()), 100U));
}

/**
 * @brief Two identical graphs: two patterns requested, each must appear in all graphs.
 */
TEST(MultiGraphPatternPreprocessorTest, two_graphs_two_patterns_covers_all)
{
    std::vector<ColoredGraph> graphs;
    graphs.push_back(make_path_4());
    graphs.push_back(make_path_4());

    MultiGraphPatternPreprocessor preprocessor(graphs, false);
    const std::vector<MultiGraphPatternResult> results = preprocessor.calculate(2U, NO_THRESHOLD);

    ASSERT_EQ(results.size(), 2U);
    EXPECT_TRUE(all_results_cover_fraction(results, static_cast<uint32_t>(graphs.size()), 100U));
}

// ── Three identical non-empty graphs ─────────────────────────────────────────

/**
 * @brief Three identical graphs: one pattern must appear in all three (100 %).
 */
TEST(MultiGraphPatternPreprocessorTest, three_graphs_one_pattern_covers_all)
{
    std::vector<ColoredGraph> graphs;
    graphs.push_back(make_path_4());
    graphs.push_back(make_path_4());
    graphs.push_back(make_path_4());

    MultiGraphPatternPreprocessor preprocessor(graphs, false);
    const std::vector<MultiGraphPatternResult> results = preprocessor.calculate(1U, NO_THRESHOLD);

    ASSERT_EQ(results.size(), 1U);
    EXPECT_TRUE(all_results_cover_fraction(results, static_cast<uint32_t>(graphs.size()), 100U));
}

/**
 * @brief Three identical graphs: three patterns requested, each must cover all graphs.
 */
TEST(MultiGraphPatternPreprocessorTest, three_graphs_three_patterns_each_covers_all)
{
    std::vector<ColoredGraph> graphs;
    graphs.push_back(make_path_4());
    graphs.push_back(make_path_4());
    graphs.push_back(make_path_4());

    MultiGraphPatternPreprocessor preprocessor(graphs, false);
    const std::vector<MultiGraphPatternResult> results = preprocessor.calculate(3U, NO_THRESHOLD);

    ASSERT_EQ(results.size(), 3U);
    EXPECT_TRUE(all_results_cover_fraction(results, static_cast<uint32_t>(graphs.size()), 100U));
}

/**
 * @brief Three identical graphs: three patterns requested, each must cover all graphs.
 */
TEST(MultiGraphPatternPreprocessorTest, three_graphs_three_patterns_each_covers_all)
{
    std::vector<ColoredGraph> graphs;
    graphs.push_back(make_path_4());
    graphs.push_back(make_path_4_with_1_added_vertex());
    graphs.push_back(make_path_4_with_2_added_vertex());

    MultiGraphPatternPreprocessor preprocessor(graphs, false);
    const std::vector<MultiGraphPatternResult> results = preprocessor.calculate(3U, NO_THRESHOLD);

    ASSERT_EQ(results.size(), 3U);
    EXPECT_TRUE(all_results_cover_fraction(results, static_cast<uint32_t>(graphs.size()), 100U));
}
