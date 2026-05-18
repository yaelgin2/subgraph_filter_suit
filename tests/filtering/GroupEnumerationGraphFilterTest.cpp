#include "GroupEnumerationGraphFilter.h"

#include "IGraphPreprocessor.h"
#include "Int128.h"

#include <cstdint>
#include <gtest/gtest.h>
#include <unordered_map>
#include <vector>

using namespace sgf;

namespace
{

class GroupEnumerationGraphFilterTest : public ::testing::Test
{
protected:
    /**
     * @brief Constructs a UInt128 motif key from a plain integer.
     * @param value Integer to wrap.
     * @return UInt128 key for use in EnumerationResult maps.
     */
    static UInt128 key(const uint64_t value)
    {
        return UInt128{value};
    }

    /**
     * @brief Builds an EnumerationResult from a list of (key, count) pairs.
     * @param entries Motif key and occurrence count pairs.
     * @return Populated EnumerationResult map.
     */
    static EnumerationResult make_result(const std::vector<std::pair<UInt128, uint32_t>>& entries)
    {
        EnumerationResult result;
        for (const auto& [motif_key, count] : entries)
        {
            result[motif_key] = count;
        }
        return result;
    }
};

// ── Group 1: Key presence — single library graph ──────────────────────────────

TEST_F(GroupEnumerationGraphFilterTest, library_has_key_query_does_not_library_is_filterable)
{
    EnumerationData library = {make_result({{key(1U), 1U}, {key(2U), 1U}})};
    GroupEnumerationGraphFilter filter_obj(std::move(library));

    const EnumerationResult query = make_result({{key(1U), 1U}});

    const FilterResult result = filter_obj.filter(query);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_TRUE(result[0]);
}

TEST_F(GroupEnumerationGraphFilterTest, query_has_key_library_does_not_library_is_not_filterable)
{
    EnumerationData library = {make_result({{key(1U), 1U}})};
    GroupEnumerationGraphFilter filter_obj(std::move(library));

    const EnumerationResult query = make_result({{key(1U), 1U}, {key(2U), 1U}});

    const FilterResult result = filter_obj.filter(query);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_FALSE(result[0]);
}

TEST_F(GroupEnumerationGraphFilterTest, both_have_key_once_library_is_not_filterable)
{
    EnumerationData library = {make_result({{key(1U), 1U}})};
    GroupEnumerationGraphFilter filter_obj(std::move(library));

    const EnumerationResult query = make_result({{key(1U), 1U}});

    const FilterResult result = filter_obj.filter(query);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_FALSE(result[0]);
}

TEST_F(GroupEnumerationGraphFilterTest, both_have_key_once_library_is_filterable)
{
    EnumerationData library = {make_result({{key(1U), 5U}})};
    GroupEnumerationGraphFilter filter_obj(std::move(library));

    const EnumerationResult query = make_result({{key(1U), 1U}});

    const FilterResult result = filter_obj.filter(query);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_TRUE(result[0]);
}

// ── Group 2: Count comparison — all keys in both, single library graph ────────

TEST_F(GroupEnumerationGraphFilterTest, all_smaller_single_graph_library_is_filterable)
{
    EnumerationData library = {make_result({{key(1U), 5U}, {key(2U), 4U}, {key(3U), 3U}})};
    GroupEnumerationGraphFilter filter_obj(std::move(library));

    const EnumerationResult query = make_result({{key(1U), 2U}, {key(2U), 1U}, {key(3U), 0U}});

    const FilterResult result = filter_obj.filter(query);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_TRUE(result[0]);
}

TEST_F(GroupEnumerationGraphFilterTest, one_smaller_single_graph_library_is_filterable)
{
    EnumerationData library = {make_result({{key(1U), 3U}, {key(2U), 3U}, {key(3U), 3U}})};
    GroupEnumerationGraphFilter filter_obj(std::move(library));

    const EnumerationResult query = make_result({{key(1U), 3U}, {key(2U), 3U}, {key(3U), 2U}});

    const FilterResult result = filter_obj.filter(query);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_TRUE(result[0]);
}

TEST_F(GroupEnumerationGraphFilterTest, all_larger_single_graph_library_is_not_filterable)
{
    EnumerationData library = {make_result({{key(1U), 2U}, {key(2U), 2U}})};
    GroupEnumerationGraphFilter filter_obj(std::move(library));

    const EnumerationResult query = make_result({{key(1U), 5U}, {key(2U), 5U}});

    const FilterResult result = filter_obj.filter(query);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_FALSE(result[0]);
}

TEST_F(GroupEnumerationGraphFilterTest, all_equal_single_graph_library_is_not_filterable)
{
    EnumerationData library = {make_result({{key(1U), 3U}, {key(2U), 3U}})};
    GroupEnumerationGraphFilter filter_obj(std::move(library));

    const EnumerationResult query = make_result({{key(1U), 3U}, {key(2U), 3U}});

    const FilterResult result = filter_obj.filter(query);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_FALSE(result[0]);
}

TEST_F(GroupEnumerationGraphFilterTest,
       some_larger_some_equal_single_graph_library_is_not_filterable)
{
    EnumerationData library = {make_result({{key(1U), 2U}, {key(2U), 3U}, {key(3U), 1U}})};
    GroupEnumerationGraphFilter filter_obj(std::move(library));

    const EnumerationResult query = make_result({{key(1U), 5U}, {key(2U), 3U}, {key(3U), 4U}});

    const FilterResult result = filter_obj.filter(query);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_FALSE(result[0]);
}

// ── Group 3: Multi-library — mix and match ────────────────────────────────────

TEST_F(GroupEnumerationGraphFilterTest, two_graphs_both_filterable_library_has_key_query_does_not)
{
    EnumerationData library = {make_result({{key(1U), 3U}}), make_result({{key(2U), 2U}})};
    GroupEnumerationGraphFilter filter_obj(std::move(library));

    const EnumerationResult query = make_result({{key(1U), 1U}, {key(2U), 1U}});

    const FilterResult result = filter_obj.filter(query);

    ASSERT_EQ(result.size(), 2U);
    EXPECT_TRUE(result[0]);
    EXPECT_TRUE(result[1]);
}

TEST_F(GroupEnumerationGraphFilterTest, two_graphs_neither_filterable_all_larger)
{
    EnumerationData library = {make_result({{key(1U), 2U}}), make_result({{key(1U), 1U}})};
    GroupEnumerationGraphFilter filter_obj(std::move(library));

    const EnumerationResult query = make_result({{key(1U), 5U}});

    const FilterResult result = filter_obj.filter(query);

    ASSERT_EQ(result.size(), 2U);
    EXPECT_FALSE(result[0]);
    EXPECT_FALSE(result[1]);
}

TEST_F(GroupEnumerationGraphFilterTest,
       two_graphs_first_filterable_all_smaller_second_not_all_equal)
{
    EnumerationData library = {make_result({{key(1U), 5U}, {key(2U), 4U}}),
                               make_result({{key(1U), 2U}, {key(2U), 2U}})};
    GroupEnumerationGraphFilter filter_obj(std::move(library));

    const EnumerationResult query = make_result({{key(1U), 2U}, {key(2U), 2U}});

    const FilterResult result = filter_obj.filter(query);

    ASSERT_EQ(result.size(), 2U);
    EXPECT_TRUE(result[0]);
    EXPECT_FALSE(result[1]);
}

TEST_F(GroupEnumerationGraphFilterTest,
       two_graphs_first_not_filterable_some_larger_second_filterable_one_smaller)
{
    EnumerationData library = {make_result({{key(1U), 2U}, {key(2U), 1U}}),
                               make_result({{key(1U), 2U}, {key(2U), 5U}})};
    GroupEnumerationGraphFilter filter_obj(std::move(library));

    const EnumerationResult query = make_result({{key(1U), 2U}, {key(2U), 3U}});

    const FilterResult result = filter_obj.filter(query);

    ASSERT_EQ(result.size(), 2U);
    EXPECT_FALSE(result[0]);
    EXPECT_TRUE(result[1]);
}

TEST_F(GroupEnumerationGraphFilterTest, three_graphs_mixed_key_presence_and_count_relationships)
{
    // Graph 0: library key missing from query → filterable
    // Graph 1: all equal → not filterable
    // Graph 2: all larger → not filterable
    EnumerationData library = {make_result({{key(3U), 1U}}),
                               make_result({{key(1U), 2U}, {key(2U), 2U}}),
                               make_result({{key(1U), 1U}, {key(2U), 1U}})};
    GroupEnumerationGraphFilter filter_obj(std::move(library));

    const EnumerationResult query = make_result({{key(1U), 2U}, {key(2U), 2U}});

    const FilterResult result = filter_obj.filter(query);

    ASSERT_EQ(result.size(), 3U);
    EXPECT_TRUE(result[0]);
    EXPECT_FALSE(result[1]);
    EXPECT_FALSE(result[2]);
}

TEST_F(GroupEnumerationGraphFilterTest, four_graphs_all_scenarios_covered)
{
    // Graph 0: one smaller → filterable
    // Graph 1: query has key library does not → not filterable
    // Graph 2: all equal → not filterable
    // Graph 3: all smaller → filterable
    EnumerationData library = {make_result({{key(1U), 3U}, {key(2U), 3U}, {key(3U), 3U}}),
                               make_result({}), make_result({{key(1U), 2U}, {key(2U), 1U}}),
                               make_result({{key(1U), 9U}, {key(2U), 9U}})};
    GroupEnumerationGraphFilter filter_obj(std::move(library));

    const EnumerationResult query = make_result({{key(1U), 2U}, {key(2U), 1U}, {key(3U), 3U}});

    const FilterResult result = filter_obj.filter(query);

    ASSERT_EQ(result.size(), 4U);
    EXPECT_TRUE(result[0]);
    EXPECT_FALSE(result[1]);
    EXPECT_FALSE(result[2]);
    EXPECT_TRUE(result[3]);
}

TEST_F(GroupEnumerationGraphFilterTest, empty_library_returns_empty_result)
{
    EnumerationData library;
    GroupEnumerationGraphFilter filter_obj(std::move(library));

    const EnumerationResult query = make_result({{key(1U), 5U}});

    const FilterResult result = filter_obj.filter(query);

    EXPECT_TRUE(result.empty());
}

// ── Group 4: Empty enumeration results ────────────────────────────────────────

TEST_F(GroupEnumerationGraphFilterTest, one_empty_library_graph_nonempty_query_is_not_filterable)
{
    // No motifs in library graph means no constraint can be violated.
    EnumerationData library = {make_result({})};
    GroupEnumerationGraphFilter filter_obj(std::move(library));

    const EnumerationResult query = make_result({{key(1U), 3U}, {key(2U), 1U}});

    const FilterResult result = filter_obj.filter(query);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_FALSE(result[0]);
}

TEST_F(GroupEnumerationGraphFilterTest, two_empty_library_graphs_nonempty_query_neither_filterable)
{
    EnumerationData library = {make_result({}), make_result({})};
    GroupEnumerationGraphFilter filter_obj(std::move(library));

    const EnumerationResult query = make_result({{key(1U), 3U}, {key(2U), 1U}});

    const FilterResult result = filter_obj.filter(query);

    ASSERT_EQ(result.size(), 2U);
    EXPECT_FALSE(result[0]);
    EXPECT_FALSE(result[1]);
}

// ── Group 5: Empty query ──────────────────────────────────────────────────────

TEST_F(GroupEnumerationGraphFilterTest, empty_query_single_nonempty_library_graph_is_filterable)
{
    // Query has zero occurrences of every motif; any non-empty library graph is pruned.
    EnumerationData library = {make_result({{key(1U), 1U}})};
    GroupEnumerationGraphFilter filter_obj(std::move(library));

    const EnumerationResult query = make_result({});

    const FilterResult result = filter_obj.filter(query);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_TRUE(result[0]);
}

TEST_F(GroupEnumerationGraphFilterTest, empty_query_two_nonempty_library_graphs_both_filterable)
{
    EnumerationData library = {make_result({{key(1U), 2U}}), make_result({{key(2U), 1U}})};
    GroupEnumerationGraphFilter filter_obj(std::move(library));

    const EnumerationResult query = make_result({});

    const FilterResult result = filter_obj.filter(query);

    ASSERT_EQ(result.size(), 2U);
    EXPECT_TRUE(result[0]);
    EXPECT_TRUE(result[1]);
}

TEST_F(GroupEnumerationGraphFilterTest,
       empty_query_mixed_library_empty_graph_survives_nonempty_pruned)
{
    // Graph 0: no motif entries → no constraint violated → survives.
    // Graph 1: has a motif; query has zero → pruned.
    EnumerationData library = {make_result({}), make_result({{key(1U), 1U}})};
    GroupEnumerationGraphFilter filter_obj(std::move(library));

    const EnumerationResult query = make_result({});

    const FilterResult result = filter_obj.filter(query);

    ASSERT_EQ(result.size(), 2U);
    EXPECT_FALSE(result[0]);
    EXPECT_TRUE(result[1]);
}

// ── Group 6: All empty simultaneously ────────────────────────────────────────

TEST_F(GroupEnumerationGraphFilterTest, empty_library_empty_query_returns_empty_result)
{
    EnumerationData library;
    GroupEnumerationGraphFilter filter_obj(std::move(library));

    const EnumerationResult query = make_result({});

    const FilterResult result = filter_obj.filter(query);

    EXPECT_TRUE(result.empty());
}

TEST_F(GroupEnumerationGraphFilterTest, one_empty_library_graph_empty_query_is_not_filterable)
{
    // Neither side has any motifs; no constraint can be violated.
    EnumerationData library = {make_result({})};
    GroupEnumerationGraphFilter filter_obj(std::move(library));

    const EnumerationResult query = make_result({});

    const FilterResult result = filter_obj.filter(query);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_FALSE(result[0]);
}

TEST_F(GroupEnumerationGraphFilterTest, two_empty_library_graphs_empty_query_neither_filterable)
{
    EnumerationData library = {make_result({}), make_result({})};
    GroupEnumerationGraphFilter filter_obj(std::move(library));

    const EnumerationResult query = make_result({});

    const FilterResult result = filter_obj.filter(query);

    ASSERT_EQ(result.size(), 2U);
    EXPECT_FALSE(result[0]);
    EXPECT_FALSE(result[1]);
}

}  // namespace
