#include "patterns/histograms/GeneralColorHist.h"

#include "exceptions/PatternException.h"

#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <tuple>

using namespace sgf;

class GeneralColorHistTest : public ::testing::Test
{
};

// ── Construction ──────────────────────────────────────────────────────────────

/**
 * @brief Constructing with 0 colors succeeds and reports the correct count.
 */
TEST_F(GeneralColorHistTest, create_0_color_hist)
{
    const GeneralColorHist hist(0U);
    EXPECT_EQ(hist.get_color_count(), 0U);
}

/**
 * @brief Constructing with 1 color succeeds and reports the correct count.
 */
TEST_F(GeneralColorHistTest, create_1_color_hist)
{
    const GeneralColorHist hist(1U);
    EXPECT_EQ(hist.get_color_count(), 1U);
}

// ── Exception: update_hist_increase_tree_count ────────────────────────────────

/**
 * @brief Adding color index 1 to a 0-color histogram throws PatternException.
 */
TEST_F(GeneralColorHistTest, increase_color_1_on_0_color_hist_throws)
{
    GeneralColorHist hist(0U);
    EXPECT_THROW(hist.update_hist_increase_tree_count(0U, 1U), PatternException);
}

/**
 * @brief Adding color index 2 to a 1-color histogram throws PatternException.
 */
TEST_F(GeneralColorHistTest, increase_color_2_on_1_color_hist_throws)
{
    GeneralColorHist hist(1U);
    EXPECT_THROW(hist.update_hist_increase_tree_count(0U, 2U), PatternException);
}

// ── Exception: update_hist_decrease_tree_count ────────────────────────────────

/**
 * @brief Decreasing color index 1 on a 0-color histogram throws PatternException.
 */
TEST_F(GeneralColorHistTest, decrease_color_1_on_0_color_hist_throws)
{
    GeneralColorHist hist(0U);
    EXPECT_THROW(hist.update_hist_decrease_tree_count(0U, 1U), PatternException);
}

/**
 * @brief Decreasing color index 2 on a 1-color histogram throws PatternException.
 */
TEST_F(GeneralColorHistTest, decrease_color_2_on_1_color_hist_throws)
{
    GeneralColorHist hist(1U);
    hist.update_hist_increase_tree_count(0U, 0U);
    EXPECT_THROW(hist.update_hist_decrease_tree_count(0U, 2U), PatternException);
}

/**
 * @brief Decreasing a depth that has no row in the histogram throws PatternException.
 */
TEST_F(GeneralColorHistTest, decrease_depth_not_in_hist_throws)
{
    GeneralColorHist hist(1U);
    EXPECT_THROW(hist.update_hist_decrease_tree_count(0U, 0U), PatternException);
}

/**
 * @brief Decreasing a cell that exists but was never incremented (still 0) throws.
 */
TEST_F(GeneralColorHistTest, decrease_zero_cell_never_increased_throws)
{
    GeneralColorHist hist(2U);
    hist.update_hist_increase_tree_count(0U, 0U);  // row 0 now exists; [0][1] is still 0
    EXPECT_THROW(hist.update_hist_decrease_tree_count(0U, 1U), PatternException);
}

/**
 * @brief Decrementing a cell that was increased then decreased to 0 again throws.
 */
TEST_F(GeneralColorHistTest, decrease_cell_after_reaching_zero_throws)
{
    GeneralColorHist hist(1U);
    hist.update_hist_increase_tree_count(0U, 0U);
    hist.update_hist_decrease_tree_count(0U, 0U);
    EXPECT_THROW(hist.update_hist_decrease_tree_count(0U, 0U), PatternException);
}

// ── Correct: update_hist_increase_tree_count ─────────────────────────────────

/**
 * @brief Adding at depth 1 to an empty histogram grows rows 0 and 1; only [1][0] is 1.
 */
TEST_F(GeneralColorHistTest, increase_depth_1_on_empty_hist_grows_rows)
{
    GeneralColorHist hist(1U);
    hist.update_hist_increase_tree_count(1U, 0U);
    const std::tuple<int32_t, int32_t, uint32_t> result = hist.get_color_to_add(0U, false);
    EXPECT_EQ(std::get<0>(result), 0);
    EXPECT_EQ(std::get<1>(result), 1);
    EXPECT_EQ(std::get<2>(result), 1U);
}

/**
 * @brief Adding at depth 2 when only row 0 exists grows the histogram to 3 rows.
 */
TEST_F(GeneralColorHistTest, increase_depth_2_on_1_row_hist)
{
    GeneralColorHist hist(1U);
    hist.update_hist_increase_tree_count(0U, 0U);
    hist.update_hist_increase_tree_count(2U, 0U);
    const std::tuple<int32_t, int32_t, uint32_t> result = hist.get_color_to_add(0U, false);
    EXPECT_EQ(std::get<2>(result), 1U);
}

/**
 * @brief Adding color 0 to a 1-color histogram increments only [0][0].
 */
TEST_F(GeneralColorHistTest, increase_color_0_on_1_color_hist)
{
    GeneralColorHist hist(1U);
    hist.update_hist_increase_tree_count(0U, 0U);
    const std::tuple<int32_t, int32_t, uint32_t> result = hist.get_color_to_add(0U, false);
    EXPECT_EQ(std::get<0>(result), 0);
    EXPECT_EQ(std::get<1>(result), 0);
    EXPECT_EQ(std::get<2>(result), 1U);
}

/**
 * @brief Adding color 2 to a 3-color histogram leaves colors 0 and 1 at zero.
 */
TEST_F(GeneralColorHistTest, increase_color_2_on_3_color_hist)
{
    GeneralColorHist hist(3U);
    hist.update_hist_increase_tree_count(0U, 2U);
    const std::tuple<int32_t, int32_t, uint32_t> result = hist.get_color_to_add(0U, false);
    EXPECT_EQ(std::get<0>(result), 2);
    EXPECT_EQ(std::get<1>(result), 0);
    EXPECT_EQ(std::get<2>(result), 1U);
}

/**
 * @brief Adding at multiple depths and colors stores each increment correctly.
 */
TEST_F(GeneralColorHistTest, increase_multiple_depths)
{
    GeneralColorHist hist(2U);
    hist.update_hist_increase_tree_count(0U, 0U);
    hist.update_hist_increase_tree_count(1U, 1U);
    hist.update_hist_increase_tree_count(2U, 0U);
    hist.update_hist_increase_tree_count(2U, 0U);
    const std::tuple<int32_t, int32_t, uint32_t> heaviest = hist.get_color_to_add(2U, false);
    EXPECT_EQ(std::get<0>(heaviest), 0);
    EXPECT_EQ(std::get<1>(heaviest), 2);
    EXPECT_EQ(std::get<2>(heaviest), 2U);
}

/**
 * @brief A valid first increase succeeds; a subsequent out-of-bound color throws and
 *        leaves the first cell intact.
 */
TEST_F(GeneralColorHistTest, first_level_ok_then_out_of_bounds_throws)
{
    GeneralColorHist hist(1U);
    hist.update_hist_increase_tree_count(0U, 0U);
    EXPECT_THROW(hist.update_hist_increase_tree_count(0U, 1U), PatternException);
    const std::tuple<int32_t, int32_t, uint32_t> result = hist.get_color_to_add(0U, false);
    EXPECT_EQ(std::get<2>(result), 1U);
}

// ── Correct: update_hist_decrease_tree_count ──────────────────────────────────

/**
 * @brief Decreasing a cell with count 2 leaves it at 1.
 */
TEST_F(GeneralColorHistTest, decrease_nonzero_cell_decrements)
{
    GeneralColorHist hist(1U);
    hist.update_hist_increase_tree_count(0U, 0U);
    hist.update_hist_increase_tree_count(0U, 0U);
    hist.update_hist_decrease_tree_count(0U, 0U);
    const std::tuple<int32_t, int32_t, uint32_t> result = hist.get_color_to_add(0U, false);
    EXPECT_EQ(std::get<2>(result), 1U);
}

/**
 * @brief Decreasing a cell from 1 to 0 succeeds; the cell is no longer a candidate.
 */
TEST_F(GeneralColorHistTest, decrease_to_zero_succeeds)
{
    GeneralColorHist hist(1U);
    hist.update_hist_increase_tree_count(0U, 0U);
    hist.update_hist_decrease_tree_count(0U, 0U);
    const std::tuple<int32_t, int32_t, uint32_t> result = hist.get_color_to_add(0U, false);
    EXPECT_EQ(std::get<0>(result), -1);
}

// ── get_color_to_add ──────────────────────────────────────────────────────────

/**
 * @brief An empty histogram returns the sentinel {-1, -1, 0}.
 */
TEST_F(GeneralColorHistTest, get_color_empty_hist_returns_sentinel)
{
    const GeneralColorHist hist(2U);
    const std::tuple<int32_t, int32_t, uint32_t> result = hist.get_color_to_add();
    EXPECT_EQ(std::get<0>(result), -1);
    EXPECT_EQ(std::get<1>(result), -1);
    EXPECT_EQ(std::get<2>(result), 0U);
}

/**
 * @brief A histogram with all zero cells returns the sentinel.
 */
TEST_F(GeneralColorHistTest, get_color_all_zero_cells_returns_sentinel)
{
    GeneralColorHist hist(2U);
    hist.update_hist_increase_tree_count(0U, 0U);
    hist.update_hist_decrease_tree_count(0U, 0U);
    const std::tuple<int32_t, int32_t, uint32_t> result = hist.get_color_to_add();
    EXPECT_EQ(std::get<0>(result), -1);
}

/**
 * @brief In non-random mode the last candidate scanned is returned (highest depth×color).
 */
TEST_F(GeneralColorHistTest, get_color_not_random_returns_last_scanned)
{
    GeneralColorHist hist(3U);
    hist.update_hist_increase_tree_count(0U, 0U);
    hist.update_hist_increase_tree_count(0U, 1U);
    hist.update_hist_increase_tree_count(0U, 2U);
    const std::tuple<int32_t, int32_t, uint32_t> result = hist.get_color_to_add(0U, false);
    EXPECT_EQ(std::get<0>(result), 2);
    EXPECT_EQ(std::get<1>(result), 0);
}

/**
 * @brief Threshold filtering excludes candidates below the threshold.
 */
TEST_F(GeneralColorHistTest, get_color_threshold_filters_low_weight)
{
    GeneralColorHist hist(3U);
    hist.update_hist_increase_tree_count(0U, 0U);
    hist.update_hist_increase_tree_count(0U, 1U);
    hist.update_hist_increase_tree_count(0U, 1U);
    hist.update_hist_increase_tree_count(0U, 2U);
    hist.update_hist_increase_tree_count(0U, 2U);
    hist.update_hist_increase_tree_count(0U, 2U);
    // color 0 has weight 1 (below threshold), colors 1 and 2 are above
    const std::tuple<int32_t, int32_t, uint32_t> result = hist.get_color_to_add(2U, false);
    EXPECT_NE(std::get<0>(result), 0);
    EXPECT_GE(std::get<2>(result), 2U);
}

/**
 * @brief Weighted random sampling selects the dominant cell most of the time.
 */
TEST_F(GeneralColorHistTest, get_color_one_dominant_sampled_more)
{
    GeneralColorHist hist(2U);
    for (uint32_t idx = 0U; idx < 1000U; ++idx)
    {
        hist.update_hist_increase_tree_count(0U, 0U);
    }
    hist.update_hist_increase_tree_count(0U, 1U);

    uint32_t dominant_count = 0U;
    constexpr uint32_t SAMPLE_COUNT = 500U;
    for (uint32_t idx = 0U; idx < SAMPLE_COUNT; ++idx)
    {
        const std::tuple<int32_t, int32_t, uint32_t> result = hist.get_color_to_add(0U, true);
        if (std::get<0>(result) == 0)
        {
            ++dominant_count;
        }
    }
    EXPECT_GE(dominant_count, 490U);
}
