#include "patterns/histograms/IndividualColorHist.h"

#include "exceptions/PatternException.h"
#include "patterns/histograms/GeneralColorHist.h"

#include <cstdint>
#include <gtest/gtest.h>
#include <tuple>
#include <vector>

using namespace sgf;

// ── Fixture ───────────────────────────────────────────────────────────────────

class IndividualColorHistTest : public ::testing::Test
{
protected:
    /**
     * @brief Number of distinct colors used across all tests.
     */
    static constexpr uint32_t NUM_COLORS = 3U;

    GeneralColorHist m_general_hist{NUM_COLORS};
    IndividualColorHist m_hist{m_general_hist};

    /**
     * @brief Returns true if the general histogram has any candidate for the given
     *        depth and color (i.e. the tree count is non-zero).
     * @param depth Pattern depth to query.
     * @param color Color to query.
     */
    bool general_hist_has_candidate(const uint32_t depth, const uint32_t color) const
    {
        GeneralColorHist probe(NUM_COLORS);
        IndividualColorHist probe_ind(probe);
        probe_ind.update_neighbours_add_node_add_neighbours_to_hist(depth, {color});
        const std::tuple<int32_t, int32_t, uint32_t> result =
            m_general_hist.get_color_to_add(0U, false);
        return std::get<0>(result) != -1;
    }

    /**
     * @brief Creates a no-op LoggerHandler for use in tests.
     * @return LoggerHandler backed by an expired weak_ptr (all log calls are no-ops).
     */
    static LoggerHandler null_logger()
    {
        return LoggerHandler{std::weak_ptr<ILogger>{}};
    }
};

// ── Construction ──────────────────────────────────────────────────────────────

/**
 * @brief IndividualColorHist inherits the color count from the linked GeneralColorHist.
 */
TEST_F(IndividualColorHistTest, construction_inherits_color_count)
{
    GeneralColorHist gen(5U);
    const IndividualColorHist ind(gen);
    EXPECT_EQ(gen.get_color_count(), 5U);
}

// ── update_neighbours_add_node_add_neighbours_to_hist ────────────────────────

/**
 * @brief Adding a single colour at depth 0 propagates to the general histogram.
 */
TEST_F(IndividualColorHistTest, add_node_zero_to_one_propagates_to_general)
{
    m_hist.update_neighbours_add_node_add_neighbours_to_hist(0U, {0U});
    const std::tuple<int32_t, int32_t, uint32_t> result =
        m_general_hist.get_color_to_add(0U, false);
    EXPECT_EQ(std::get<0>(result), 0);
    EXPECT_EQ(std::get<1>(result), 0);
    EXPECT_EQ(std::get<2>(result), 1U);
}

/**
 * @brief Adding the same colour a second time does NOT increment the general histogram again.
 */
TEST_F(IndividualColorHistTest, add_node_above_one_no_propagation_to_general)
{
    m_hist.update_neighbours_add_node_add_neighbours_to_hist(0U, {0U});
    const std::tuple<int32_t, int32_t, uint32_t> after_first =
        m_general_hist.get_color_to_add(0U, false);
    const uint32_t weight_after_first = std::get<2>(after_first);

    m_hist.update_neighbours_add_node_add_neighbours_to_hist(0U, {0U});
    const std::tuple<int32_t, int32_t, uint32_t> after_second =
        m_general_hist.get_color_to_add(0U, false);
    EXPECT_EQ(std::get<2>(after_second), weight_after_first);
}

/**
 * @brief Adding at depth 3 creates the necessary rows without affecting lower depths.
 */
TEST_F(IndividualColorHistTest, add_node_grows_rows_to_depth)
{
    m_hist.update_neighbours_add_node_add_neighbours_to_hist(3U, {0U});
    const std::tuple<int32_t, int32_t, uint32_t> result =
        m_general_hist.get_color_to_add(0U, false);
    EXPECT_EQ(std::get<1>(result), 3);
}

/**
 * @brief An empty colours vector is a no-op on both histograms.
 */
TEST_F(IndividualColorHistTest, add_empty_colors_is_noop)
{
    m_hist.update_neighbours_add_node_add_neighbours_to_hist(0U, {});
    const std::tuple<int32_t, int32_t, uint32_t> result =
        m_general_hist.get_color_to_add(0U, false);
    EXPECT_EQ(std::get<0>(result), -1);
}

// ── update_hist_decrease_from_neighbours ────────────────────────────────────

/**
 * @brief Decreasing once after two adds leaves the neighbour count at 1 (no general
 *        histogram propagation because the count did not reach 0).
 */
TEST_F(IndividualColorHistTest, decrease_from_neighbours_above_zero_no_propagation)
{
    m_hist.update_neighbours_add_node_add_neighbours_to_hist(0U, {0U});
    m_hist.update_neighbours_add_node_add_neighbours_to_hist(0U, {0U});
    m_hist.update_hist_decrease_from_neighbours(0U, {0U});
    const std::tuple<int32_t, int32_t, uint32_t> result =
        m_general_hist.get_color_to_add(0U, false);
    EXPECT_EQ(std::get<2>(result), 1U);
}

/**
 * @brief Decreasing from 1 to 0 propagates to the general histogram (cell becomes 0).
 */
TEST_F(IndividualColorHistTest, decrease_from_neighbours_one_to_zero_propagates_to_general)
{
    m_hist.update_neighbours_add_node_add_neighbours_to_hist(0U, {0U});
    m_hist.update_hist_decrease_from_neighbours(0U, {0U});
    const std::tuple<int32_t, int32_t, uint32_t> result =
        m_general_hist.get_color_to_add(0U, false);
    EXPECT_EQ(std::get<0>(result), -1);
}

/**
 * @brief Decreasing a cell that was never incremented (count 0) throws PatternException.
 *
 * Depth row 0 exists because color 1 was added; cell [0][2] is 0.
 */
TEST_F(IndividualColorHistTest, decrease_from_neighbours_zero_cell_throws)
{
    m_hist.update_neighbours_add_node_add_neighbours_to_hist(0U, {1U});
    EXPECT_THROW(m_hist.update_hist_decrease_from_neighbours(2U, {0U}), PatternException);
}

/**
 * @brief After an add-then-decrease-to-zero sequence, a second decrease throws.
 */
TEST_F(IndividualColorHistTest, decrease_from_neighbours_increased_decreased_then_again_throws)
{
    m_hist.update_neighbours_add_node_add_neighbours_to_hist(0U, {0U});
    m_hist.update_hist_decrease_from_neighbours(0U, {0U});
    EXPECT_THROW(m_hist.update_hist_decrease_from_neighbours(0U, {0U}), PatternException);
}

/**
 * @brief An empty depths vector is a no-op on both histograms.
 */
TEST_F(IndividualColorHistTest, decrease_empty_depths_is_noop)
{
    m_hist.update_neighbours_add_node_add_neighbours_to_hist(0U, {0U});
    m_hist.update_hist_decrease_from_neighbours(0U, {});
    const std::tuple<int32_t, int32_t, uint32_t> result =
        m_general_hist.get_color_to_add(0U, false);
    EXPECT_EQ(std::get<2>(result), 1U);
}

// ── update_neighbours_remove_node_decrease_neighbours_from_hist ──────────────

/**
 * @brief Removing a node decrements its colour's count; zero-crossing propagates.
 */
TEST_F(IndividualColorHistTest, remove_node_zero_crossing_propagates_to_general)
{
    m_hist.update_neighbours_add_node_add_neighbours_to_hist(0U, {0U});
    m_hist.update_neighbours_remove_node_decrease_neighbours_from_hist(0U, {0U});
    const std::tuple<int32_t, int32_t, uint32_t> result =
        m_general_hist.get_color_to_add(0U, false);
    EXPECT_EQ(std::get<0>(result), -1);
}

/**
 * @brief Removing a colour that was never added (count 0) throws PatternException.
 */
TEST_F(IndividualColorHistTest, remove_node_zero_cell_throws)
{
    m_hist.update_neighbours_add_node_add_neighbours_to_hist(0U, {1U});
    EXPECT_THROW(m_hist.update_neighbours_remove_node_decrease_neighbours_from_hist(0U, {2U}),
                 PatternException);
}

/**
 * @brief Add then remove the same colours leaves the general histogram unchanged.
 */
TEST_F(IndividualColorHistTest, add_remove_roundtrip_leaves_general_unchanged)
{
    const std::tuple<int32_t, int32_t, uint32_t> before =
        m_general_hist.get_color_to_add(0U, false);

    m_hist.update_neighbours_add_node_add_neighbours_to_hist(0U, {0U, 1U});
    m_hist.update_neighbours_remove_node_decrease_neighbours_from_hist(0U, {0U, 1U});

    const std::tuple<int32_t, int32_t, uint32_t> after = m_general_hist.get_color_to_add(0U, false);

    EXPECT_EQ(std::get<0>(before), std::get<0>(after));
    EXPECT_EQ(std::get<1>(before), std::get<1>(after));
    EXPECT_EQ(std::get<2>(before), std::get<2>(after));
}
