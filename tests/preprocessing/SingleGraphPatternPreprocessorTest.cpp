#include "SingleGraphPatternPreprocessor.h"

#include "ColoredGraph.h"
#include "InvalidArgumentException.h"

#include <algorithm>
#include <cstdint>
#include <gtest/gtest.h>
#include <unordered_set>
#include <vector>

using namespace sgf;

namespace
{

constexpr double NO_THRESHOLD = 0.0;

/**
 * @brief Build a zero-vertex ColoredGraph.
 */
ColoredGraph make_empty_graph()
{
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    std::vector<uint32_t> colors;
    return ColoredGraph(0U, edges, colors, false);
}

/**
 * @brief Build a 4-vertex path: 0–1–2–3, all color 0.
 */
ColoredGraph make_path_4()
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {1U, 2U}, {2U, 3U}};
    std::vector<uint32_t> colors = {0U, 0U, 0U, 0U};
    return ColoredGraph(4U, edges, colors, false);
}

/**
 * @brief Build a 5-vertex path: 0–1–2–3–4, all color 0.
 */
ColoredGraph make_path_5()
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {1U, 2U}, {2U, 3U}, {3U, 4U}};
    std::vector<uint32_t> colors = {0U, 0U, 0U, 0U, 0U};
    return ColoredGraph(5U, edges, colors, false);
}

/**
 * @brief Build a to_process vector of size @p total with the first @p count entries true.
 */
std::vector<bool> make_to_process(const std::size_t total, const uint32_t count)
{
    std::vector<bool> to_process(total, false);
    const std::size_t limit = std::min(static_cast<std::size_t>(count), total);
    std::fill_n(to_process.begin(), limit, true);
    return to_process;
}

/**
 * @brief Return true if @p results contains at least one entry with source index @p index.
 */
bool has_source_index(const std::vector<PatternPreprocessorResult>& results, const uint32_t index)
{
    return std::any_of(results.cbegin(), results.cend(),
                       [index](const PatternPreprocessorResult& result)
                       {
                           const std::unordered_set<uint32_t>& indices = result.second;
                           return indices.count(index) > 0U;
                       });
}

/**
 * @brief Return true if every source index in @p results is less than @p upper_bound.
 */
bool all_indices_in_range(const std::vector<PatternPreprocessorResult>& results,
                          const uint32_t upper_bound)
{
    return std::all_of(results.cbegin(), results.cend(),
                       [upper_bound](const PatternPreprocessorResult& result)
                       {
                           const std::unordered_set<uint32_t>& indices = result.second;
                           return std::all_of(indices.cbegin(), indices.cend(),
                                              [upper_bound](const uint32_t idx)
                                              {
                                                  return idx < upper_bound;
                                              });
                       });
}

}  // namespace

// ── Empty/degenerate inputs ───────────────────────────────────────────────────

/**
 * @brief Empty background graph must throw InvalidArgumentException.
 */
TEST(SingleGraphPatternPreprocessorTest, empty_background_graph_throws)
{
    std::vector<ColoredGraph> graphs;
    graphs.push_back(make_path_4());
    const std::vector<bool> to_process = make_to_process(graphs.size(), 1U);
    SingleGraphPatternPreprocessor preprocessor(graphs, false, make_empty_graph(), to_process,
                                                NO_THRESHOLD);
    EXPECT_THROW(preprocessor.calculate(), InvalidArgumentException);
}

/**
 * @brief Empty library graph (zero vertices) must throw InvalidArgumentException.
 */
TEST(SingleGraphPatternPreprocessorTest, empty_library_graph_throws)
{
    std::vector<ColoredGraph> graphs;
    graphs.push_back(make_empty_graph());
    const std::vector<bool> to_process = make_to_process(graphs.size(), 1U);
    SingleGraphPatternPreprocessor preprocessor(graphs, false, make_path_4(), to_process,
                                                NO_THRESHOLD);
    EXPECT_THROW(preprocessor.calculate(), InvalidArgumentException);
}

/**
 * @brief pattern_number of zero processes no graphs and returns an empty result.
 */
TEST(SingleGraphPatternPreprocessorTest, zero_pattern_number_returns_empty)
{
    std::vector<ColoredGraph> graphs;
    graphs.push_back(make_path_4());
    const std::vector<bool> to_process = make_to_process(graphs.size(), 0U);
    SingleGraphPatternPreprocessor preprocessor(graphs, false, make_path_4(), to_process,
                                                NO_THRESHOLD);
    const std::vector<PatternPreprocessorResult> results = preprocessor.calculate();
    EXPECT_TRUE(results.empty());
}

// ── Single library graph ──────────────────────────────────────────────────────

/**
 * @brief One library graph processed: every result must carry source index 0.
 */
TEST(SingleGraphPatternPreprocessorTest, one_graph_results_carry_index_zero)
{
    std::vector<ColoredGraph> graphs;
    graphs.push_back(make_path_4());
    const std::vector<bool> to_process = make_to_process(graphs.size(), 1U);
    SingleGraphPatternPreprocessor preprocessor(graphs, false, make_path_4(), to_process,
                                                NO_THRESHOLD);
    const std::vector<PatternPreprocessorResult> results = preprocessor.calculate();
    ASSERT_FALSE(results.empty());
    EXPECT_TRUE(has_source_index(results, 0U));
    EXPECT_TRUE(all_indices_in_range(results, 1U));
}

/**
 * @brief One graph processed: no source index other than 0 should appear.
 */
TEST(SingleGraphPatternPreprocessorTest, one_graph_no_indices_above_zero)
{
    std::vector<ColoredGraph> graphs;
    graphs.push_back(make_path_4());
    const std::vector<bool> to_process = make_to_process(graphs.size(), 1U);
    SingleGraphPatternPreprocessor preprocessor(graphs, false, make_path_4(), to_process,
                                                NO_THRESHOLD);
    const std::vector<PatternPreprocessorResult> results = preprocessor.calculate();
    EXPECT_TRUE(all_indices_in_range(results, 1U));
}

// ── Two library graphs ────────────────────────────────────────────────────────

/**
 * @brief Two graphs, pattern_number=1: only graph 0 is processed; index 1 must not appear.
 */
TEST(SingleGraphPatternPreprocessorTest, pattern_number_limits_graphs_processed)
{
    std::vector<ColoredGraph> graphs;
    graphs.push_back(make_path_4());
    graphs.push_back(make_path_5());
    const std::vector<bool> to_process = make_to_process(graphs.size(), 1U);
    SingleGraphPatternPreprocessor preprocessor(graphs, false, make_path_4(), to_process,
                                                NO_THRESHOLD);
    const std::vector<PatternPreprocessorResult> results = preprocessor.calculate();
    ASSERT_FALSE(results.empty());
    EXPECT_TRUE(has_source_index(results, 0U));
    EXPECT_TRUE(all_indices_in_range(results, 1U));
}

/**
 * @brief Two graphs, pattern_number=2: both graphs are processed; index 1 must appear.
 */
TEST(SingleGraphPatternPreprocessorTest, two_graphs_both_indices_present)
{
    std::vector<ColoredGraph> graphs;
    graphs.push_back(make_path_4());
    graphs.push_back(make_path_4());
    const std::vector<bool> to_process = make_to_process(graphs.size(), 2U);
    SingleGraphPatternPreprocessor preprocessor(graphs, false, make_path_4(), to_process,
                                                NO_THRESHOLD);
    const std::vector<PatternPreprocessorResult> results = preprocessor.calculate();
    ASSERT_FALSE(results.empty());
    EXPECT_TRUE(has_source_index(results, 0U));
    EXPECT_TRUE(has_source_index(results, 1U));
    EXPECT_TRUE(all_indices_in_range(results, 2U));
}

// ── Three library graphs ──────────────────────────────────────────────────────

/**
 * @brief Three graphs, pattern_number=3: all three source indices must appear.
 */
TEST(SingleGraphPatternPreprocessorTest, three_graphs_all_indices_present)
{
    std::vector<ColoredGraph> graphs;
    graphs.push_back(make_path_4());
    graphs.push_back(make_path_4());
    graphs.push_back(make_path_4());
    const std::vector<bool> to_process = make_to_process(graphs.size(), 3U);
    SingleGraphPatternPreprocessor preprocessor(graphs, false, make_path_4(), to_process,
                                                NO_THRESHOLD);
    const std::vector<PatternPreprocessorResult> results = preprocessor.calculate();
    ASSERT_FALSE(results.empty());
    EXPECT_TRUE(has_source_index(results, 0U));
    EXPECT_TRUE(has_source_index(results, 1U));
    EXPECT_TRUE(has_source_index(results, 2U));
    EXPECT_TRUE(all_indices_in_range(results, 3U));
}

/**
 * @brief Three graphs, pattern_number=2: index 2 must not appear.
 */
TEST(SingleGraphPatternPreprocessorTest, three_graphs_partial_processing)
{
    std::vector<ColoredGraph> graphs;
    graphs.push_back(make_path_4());
    graphs.push_back(make_path_4());
    graphs.push_back(make_path_4());
    const std::vector<bool> to_process = make_to_process(graphs.size(), 2U);
    SingleGraphPatternPreprocessor preprocessor(graphs, false, make_path_4(), to_process,
                                                NO_THRESHOLD);
    const std::vector<PatternPreprocessorResult> results = preprocessor.calculate();
    EXPECT_TRUE(all_indices_in_range(results, 2U));
}

// ── Config ────────────────────────────────────────────────────────────────────

/**
 * @brief Custom config with minimal beam width does not throw.
 */
TEST(SingleGraphPatternPreprocessorTest, custom_config_does_not_throw)
{
    std::vector<ColoredGraph> graphs;
    graphs.push_back(make_path_4());
    constexpr uint32_t SMALL_BEAM = 1U;
    const SingleGraphFinderConfig config{SMALL_BEAM, 0.5, 0.5};
    const std::vector<bool> to_process = make_to_process(graphs.size(), 1U);
    EXPECT_NO_THROW(SingleGraphPatternPreprocessor(graphs, false, make_path_4(), to_process,
                                                   NO_THRESHOLD, config)
                        .calculate());
}

/**
 * @brief Default config produces the same result as an explicit default config.
 */
TEST(SingleGraphPatternPreprocessorTest, default_config_matches_explicit_defaults)
{
    std::vector<ColoredGraph> graphs;
    graphs.push_back(make_path_4());
    const std::vector<bool> to_process = make_to_process(graphs.size(), 1U);

    SingleGraphPatternPreprocessor preprocessor_default(graphs, false, make_path_4(), to_process,
                                                        NO_THRESHOLD);
    SingleGraphPatternPreprocessor preprocessor_explicit(graphs, false, make_path_4(), to_process,
                                                         NO_THRESHOLD, SingleGraphFinderConfig{});

    const std::vector<PatternPreprocessorResult> results_default = preprocessor_default.calculate();
    const std::vector<PatternPreprocessorResult> results_explicit =
        preprocessor_explicit.calculate();

    EXPECT_EQ(results_default.size(), results_explicit.size());
}
