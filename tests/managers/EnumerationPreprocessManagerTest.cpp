#include "EnumerationPreprocessManager.h"

#include "ColoredGraph.h"
#include "IGraphPreprocessor.h"
#include "LoggerHandler.h"

#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <unordered_map>
#include <vector>

using namespace sgf;

namespace
{

/**
 * @brief Returns a no-op LoggerHandler backed by an expired weak_ptr.
 * @return LoggerHandler that silently discards all log calls.
 */
LoggerHandler null_logger()
{
    return LoggerHandler{std::weak_ptr<ILogger>{}};
}

/**
 * @brief Stub preprocessor that returns a fixed map regardless of the graph.
 */
class StubPreprocessor : public IGraphPreprocessor
{
public:
    /**
     * @brief Construct a stub returning the given fixed result.
     * @param result Map returned by every calculate() call.
     */
    explicit StubPreprocessor(std::unordered_map<UInt128, uint32_t, UInt128Hash> result)
        : m_result(std::move(result))
    {
    }

    /**
     * @brief Return the fixed result supplied at construction.
     * @return Preconfigured frequency-signature map.
     */
    std::unordered_map<UInt128, uint32_t, UInt128Hash> calculate() override
    {
        return m_result;
    }

private:
    std::unordered_map<UInt128, uint32_t, UInt128Hash> m_result;
};

/**
 * @brief Stub preprocessor that records how many times calculate() was called.
 */
class CallCountPreprocessor : public IGraphPreprocessor
{
public:
    /**
     * @brief Pointer to an external counter incremented on each calculate() call.
     * @param call_count Shared counter incremented by calculate().
     */
    explicit CallCountPreprocessor(uint32_t& call_count)
        : m_call_count(call_count)
    {
    }

    /**
     * @brief Increment the external counter and return an empty map.
     * @return Empty frequency-signature map.
     */
    std::unordered_map<UInt128, uint32_t, UInt128Hash> calculate() override
    {
        ++m_call_count;
        return {};
    }

private:
    uint32_t& m_call_count;
};

/**
 * @brief Builds a minimal single-vertex graph with no edges.
 * @param color Vertex color to assign.
 * @return Isolated one-vertex ColoredGraph.
 */
ColoredGraph make_single_vertex_graph(const uint32_t color)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    const std::vector<uint32_t> colors{color};
    return ColoredGraph{1U, edges, colors, false};
}

}  // namespace

// ── Empty library ─────────────────────────────────────────────────────────────

TEST(EnumerationPreprocessManagerTest, empty_library_returns_empty_results)
{
    EnumerationPreprocessManager manager{std::vector<ColoredGraph>{}, null_logger()};

    const std::vector<std::unordered_map<UInt128, uint32_t, UInt128Hash>> results =
        manager.preprocess(
            [](const ColoredGraph& /*graph*/, LoggerHandler /*logger*/)
            {
                return std::make_unique<StubPreprocessor>(
                    std::unordered_map<UInt128, uint32_t, UInt128Hash>{});
            });

    EXPECT_TRUE(results.empty());
}

// ── Single graph ──────────────────────────────────────────────────────────────

TEST(EnumerationPreprocessManagerTest, single_graph_returns_one_result)
{
    std::vector<ColoredGraph> library;
    library.push_back(make_single_vertex_graph(0U));
    EnumerationPreprocessManager manager{std::move(library), null_logger()};

    const std::unordered_map<UInt128, uint32_t, UInt128Hash> expected_map{{UInt128{1U, 0U}, 5U}};

    const std::vector<std::unordered_map<UInt128, uint32_t, UInt128Hash>> results =
        manager.preprocess(
            [&expected_map](const ColoredGraph& /*graph*/, LoggerHandler /*logger*/)
            {
                return std::make_unique<StubPreprocessor>(expected_map);
            });

    ASSERT_EQ(results.size(), 1U);
    EXPECT_EQ(results[0], expected_map);
}

// ── Multiple graphs ───────────────────────────────────────────────────────────

TEST(EnumerationPreprocessManagerTest, multiple_graphs_return_index_aligned_results)
{
    std::vector<ColoredGraph> library;
    library.push_back(make_single_vertex_graph(0U));
    library.push_back(make_single_vertex_graph(1U));
    library.push_back(make_single_vertex_graph(2U));
    EnumerationPreprocessManager manager{std::move(library), null_logger()};

    const std::vector<std::unordered_map<UInt128, uint32_t, UInt128Hash>> fixed_maps{
        {{UInt128{1U, 0U}, 1U}},
        {{UInt128{2U, 0U}, 2U}},
        {{UInt128{3U, 0U}, 3U}},
    };

    uint32_t call_index = 0U;
    const std::vector<std::unordered_map<UInt128, uint32_t, UInt128Hash>> results =
        manager.preprocess(
            [&fixed_maps, &call_index](const ColoredGraph& /*graph*/, LoggerHandler /*logger*/)
            {
                return std::make_unique<StubPreprocessor>(fixed_maps[call_index++]);
            });

    ASSERT_EQ(results.size(), 3U);
    EXPECT_EQ(results[0], fixed_maps[0]);
    EXPECT_EQ(results[1], fixed_maps[1]);
    EXPECT_EQ(results[2], fixed_maps[2]);
}

// ── calculate() call count ────────────────────────────────────────────────────

TEST(EnumerationPreprocessManagerTest, calculate_called_once_per_library_graph)
{
    std::vector<ColoredGraph> library;
    library.push_back(make_single_vertex_graph(0U));
    library.push_back(make_single_vertex_graph(0U));
    library.push_back(make_single_vertex_graph(0U));
    EnumerationPreprocessManager manager{std::move(library), null_logger()};

    uint32_t call_count = 0U;
    manager.preprocess(
        [&call_count](const ColoredGraph& /*graph*/, LoggerHandler /*logger*/)
        {
            return std::make_unique<CallCountPreprocessor>(call_count);
        });

    EXPECT_EQ(call_count, 3U);
}

// ── Factory receives correct graph ────────────────────────────────────────────

TEST(EnumerationPreprocessManagerTest, factory_receives_graphs_in_library_order)
{
    std::vector<ColoredGraph> library;
    library.push_back(make_single_vertex_graph(10U));
    library.push_back(make_single_vertex_graph(20U));
    EnumerationPreprocessManager manager{std::move(library), null_logger()};

    std::vector<uint32_t> received_colors;
    manager.preprocess(
        [&received_colors](const ColoredGraph& graph, LoggerHandler /*logger*/)
        {
            received_colors.push_back(graph.get_vertex_color(0U));
            return std::make_unique<StubPreprocessor>(
                std::unordered_map<UInt128, uint32_t, UInt128Hash>{});
        });

    ASSERT_EQ(received_colors.size(), 2U);
    EXPECT_EQ(received_colors[0], 10U);
    EXPECT_EQ(received_colors[1], 20U);
}

// ── Result contains empty map from preprocessor ───────────────────────────────

TEST(EnumerationPreprocessManagerTest, empty_preprocessor_result_propagated)
{
    std::vector<ColoredGraph> library;
    library.push_back(make_single_vertex_graph(0U));
    EnumerationPreprocessManager manager{std::move(library), null_logger()};

    const std::vector<std::unordered_map<UInt128, uint32_t, UInt128Hash>> results =
        manager.preprocess(
            [](const ColoredGraph& /*graph*/, LoggerHandler /*logger*/)
            {
                return std::make_unique<StubPreprocessor>(
                    std::unordered_map<UInt128, uint32_t, UInt128Hash>{});
            });

    ASSERT_EQ(results.size(), 1U);
    EXPECT_TRUE(results[0].empty());
}
