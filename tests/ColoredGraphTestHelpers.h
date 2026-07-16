#pragma once

#include "ColoredGraph.h"
#include "ILogger.h"
#include "LogLevel.h"

#include <algorithm>
#include <cstdint>
#include <gtest/gtest.h>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace test_helpers
{

/**
 * @brief ILogger implementation that captures all log messages into a vector.
 *
 * Thread-safe: multiple threads may call log() concurrently.
 */
class CapturingLogger : public sgf::ILogger
{
public:
    void log(const sgf::LogLevel level, const std::string& message) override
    {
        const std::lock_guard<std::mutex> lock{m_mutex};
        m_messages.emplace_back(level, message);
    }

    /**
     * @brief Returns a snapshot of all captured (level, message) pairs.
     * @return All entries in the order they were received.
     */
    [[nodiscard]] std::vector<std::pair<sgf::LogLevel, std::string>> messages() const
    {
        const std::lock_guard<std::mutex> lock{m_mutex};
        return m_messages;
    }

private:
    std::vector<std::pair<sgf::LogLevel, std::string>> m_messages;
    mutable std::mutex m_mutex;
};

/**
 * @brief Asserts that the neighbour list of @p vertex equals @p expected.
 *
 * Neighbours are returned in sorted order by the CSR structure, so @p expected
 * must also be sorted ascending.
 *
 * @param graph The graph under test.
 * @param vertex Vertex whose neighbours are checked.
 * @param expected Sorted list of expected neighbour IDs.
 * @param reversed If true, checks in-neighbours (directed graphs only).
 */
inline void assert_neighbours(const sgf::ColoredGraph& graph, const uint32_t vertex,
                              const std::vector<uint32_t>& expected, const bool reversed = false)
{
    const std::pair<std::vector<uint32_t>::const_iterator, std::vector<uint32_t>::const_iterator>
        range = graph.get_neighbours(vertex, reversed);
    const std::vector<uint32_t> actual(range.first, range.second);
    EXPECT_EQ(actual, expected) << "vertex " << vertex << (reversed ? " (reversed)" : "");
}

/**
 * @brief Asserts that the edge-color list parallel to the neighbour list of @p vertex
 *        equals @p expected.
 *
 * The order of colors matches the order returned by get_neighbours(), which is sorted
 * ascending by neighbour ID.
 *
 * @param graph The graph under test.
 * @param vertex Vertex whose edge colors are checked.
 * @param expected Edge colors in the same order as get_neighbours() output.
 * @param reversed If true, checks in-edge colors (directed graphs only).
 */
inline void assert_edge_colors(const sgf::ColoredGraph& graph, const uint32_t vertex,
                               const std::vector<uint32_t>& expected, const bool reversed = false)
{
    const std::pair<std::vector<uint32_t>::const_iterator, std::vector<uint32_t>::const_iterator>
        range = graph.get_neighbour_edge_colors(vertex, reversed);
    const std::vector<uint32_t> actual(range.first, range.second);
    EXPECT_EQ(actual, expected) << "edge colors for vertex " << vertex
                                << (reversed ? " (reversed)" : "");
}

}  // namespace test_helpers
