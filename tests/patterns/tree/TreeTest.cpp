#include "patterns/tree/Tree.h"

#include "exceptions/AddNodeException.h"
#include "exceptions/DeleteNodeException.h"
#include "exceptions/PatternException.h"
#include "graph/ColoredGraph.h"
#include "patterns/histograms/GeneralColorHist.h"
#include "patterns/tree/Node.h"

#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

using namespace sgf;

// ── Graph factory helpers ────────────────────────────────────────────────────

namespace
{

/**
 * @brief Undirected path graph: 4 vertices (0-1-2-3), all color 0.
 */
ColoredGraph make_path_4()
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {1U, 2U}, {2U, 3U}};
    return ColoredGraph(4U, edges, {0U, 0U, 0U, 0U}, false);
}

/**
 * @brief Undirected star graph: 5 vertices, center=0, spokes to 1-4, all color 0.
 */
ColoredGraph make_star_5()
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {0U, 1U}, {0U, 2U}, {0U, 3U}, {0U, 4U}};
    return ColoredGraph(5U, edges, {0U, 0U, 0U, 0U, 0U}, false);
}

/**
 * @brief Undirected path graph: 3 vertices (0-1-2), colors {0, 1, 0}.
 */
ColoredGraph make_two_colors_path()
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {1U, 2U}};
    return ColoredGraph(3U, edges, {0U, 1U, 0U}, false);
}

/**
 * @brief Directed chain: 3 vertices, edges 0→1 and 1→2, all color 0.
 */
ColoredGraph make_directed_chain()
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {1U, 2U}};
    return ColoredGraph(3U, edges, {0U, 0U, 0U}, true);
}

/**
 * @brief Isolated single vertex, color 0.
 */
ColoredGraph make_isolated()
{
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    return ColoredGraph(1U, edges, {0U}, false);
}

}  // namespace

// ── Fixture ───────────────────────────────────────────────────────────────────

class TreeTest : public ::testing::Test
{
protected:
    static constexpr uint32_t NUM_COLORS = 2U;

    GeneralColorHist m_fwd_hist{NUM_COLORS};
    GeneralColorHist m_rev_hist{NUM_COLORS};
};

// ── Construction ──────────────────────────────────────────────────────────────

/**
 * @brief Constructing a directed tree without a reverse histogram throws PatternException.
 */
TEST_F(TreeTest, directed_without_reverse_hist_throws)
{
    EXPECT_THROW(Tree(0U, true, m_fwd_hist), PatternException);
}

/**
 * @brief Constructing an undirected tree for an isolated vertex succeeds.
 */
TEST_F(TreeTest, undirected_isolated_vertex_succeeds)
{
    Tree tree(0U, false, m_fwd_hist);
    EXPECT_FALSE(tree.is_empty());
    EXPECT_EQ(tree.get_root()->m_index, 0U);
    EXPECT_EQ(tree.get_root()->m_depth, 0U);
}

/**
 * @brief Constructing an undirected tree for a path graph succeeds.
 */
TEST_F(TreeTest, undirected_path_succeeds)
{
    EXPECT_NO_THROW(Tree(0U, false, m_fwd_hist));
}

/**
 * @brief Constructing a directed tree with a reverse histogram succeeds.
 */
TEST_F(TreeTest, directed_chain_with_reverse_hist_succeeds)
{
    EXPECT_NO_THROW(Tree(0U, true, m_fwd_hist,
                         std::optional<std::reference_wrapper<GeneralColorHist>>(m_rev_hist)));
}

// ── add_tree_level ────────────────────────────────────────────────────────────

/**
 * @brief Passing an empty index list returns an empty node vector.
 *
 * Graph: path_4.
 */
TEST_F(TreeTest, empty_new_indexes_returns_empty)
{
    const ColoredGraph graph = make_path_4();
    const std::vector<ColoredGraph> s_list = {graph};

    Tree tree(0U, false, m_fwd_hist);
    const std::vector<NodePtr> result =
        tree.add_tree_level({}, s_list);
    EXPECT_TRUE(result.empty());
}

/**
 * @brief Passing siblings of the same parent non-consecutively throws PatternException.
 *
 * Graph: star_5. root has children 1,2,3,4.
 * Sequence: (1, root), (2, root), (3, root) is fine; but (1, root), (3, child1), (2, root)
 * re-introduces root after it was closed.
 */
TEST_F(TreeTest, non_grouped_siblings_throws)
{
    const ColoredGraph graph = make_star_5();
    const std::vector<ColoredGraph> s_list = {graph};

    Tree tree(0U, false, m_fwd_hist);
    const NodePtr root = tree.get_root();
    const std::vector<NodePtr> first_children =
        tree.add_tree_level({{1U, root}, {2U, root}}, s_list);
    const NodePtr child_1 = first_children[0];

    EXPECT_THROW(
        tree.add_tree_level({{3U, root}, {4U, child_1}, {1U, root}}, s_list),
        AddNodeException);
}

/**
 * @brief Adding a single child under the root creates a depth-1 node.
 *
 * Graph: path_4.
 */
TEST_F(TreeTest, single_child_under_root)
{
    const ColoredGraph graph = make_path_4();
    const std::vector<ColoredGraph> s_list = {graph};

    Tree tree(0U, false, m_fwd_hist);
    const NodePtr root = tree.get_root();
    const std::vector<NodePtr> children = tree.add_tree_level({{1U, root}}, s_list);

    EXPECT_EQ(children.size(), 1U);
    EXPECT_EQ(children[0]->m_index, 1U);
    EXPECT_EQ(children[0]->m_depth, 1U);
    EXPECT_EQ(children[0]->m_parent.lock(), root);
}

/**
 * @brief Adding two children under the root creates two depth-1 nodes.
 *
 * Graph: star_5.
 */
TEST_F(TreeTest, multiple_children_same_parent)
{
    const ColoredGraph graph = make_star_5();
    const std::vector<ColoredGraph> s_list = {graph};

    Tree tree(0U, false, m_fwd_hist);
    const NodePtr root = tree.get_root();
    const std::vector<NodePtr> children =
        tree.add_tree_level({{1U, root}, {2U, root}}, s_list);

    EXPECT_EQ(children.size(), 2U);
    EXPECT_NE(root->m_son, nullptr);
}

/**
 * @brief Adding two levels produces a chain root→1→2 with correct depths.
 *
 * Graph: path_4.
 */
TEST_F(TreeTest, two_level_tree)
{
    const ColoredGraph graph = make_path_4();
    const std::vector<ColoredGraph> s_list = {graph};

    Tree tree(0U, false, m_fwd_hist);
    const NodePtr root = tree.get_root();
    const std::vector<NodePtr> level1 = tree.add_tree_level({{1U, root}}, s_list);
    const std::vector<NodePtr> level2 = tree.add_tree_level({{2U, level1[0]}}, s_list);

    EXPECT_EQ(level2[0]->m_depth, 2U);
    EXPECT_EQ(level2[0]->m_index, 2U);
}

// ── remove_node ───────────────────────────────────────────────────────────────

/**
 * @brief Removing the only child of the root clears the root's m_son.
 *
 * Graph: path_4.
 */
TEST_F(TreeTest, remove_only_child_clears_parent_son)
{
    const ColoredGraph graph = make_path_4();
    const std::vector<ColoredGraph> s_list = {graph};

    Tree tree(0U, false, m_fwd_hist);
    const NodePtr root = tree.get_root();
    const std::vector<NodePtr> children = tree.add_tree_level({{1U, root}}, s_list);

    tree.remove_node(children[0], s_list);

    EXPECT_EQ(root->m_son, nullptr);
}

/**
 * @brief Removing the deepest node in a chain root→1→2 backtracks all the way and
 *        empties the tree.
 *
 * Graph: path_4.
 */
TEST_F(TreeTest, remove_triggers_backtrack_to_root_empties_tree)
{
    const ColoredGraph graph = make_path_4();
    const std::vector<ColoredGraph> s_list = {graph};

    Tree tree(0U, false, m_fwd_hist);
    const NodePtr root = tree.get_root();
    const std::vector<NodePtr> level1 = tree.add_tree_level({{1U, root}}, s_list);
    const std::vector<NodePtr> level2 = tree.add_tree_level({{2U, level1[0]}}, s_list);

    tree.remove_node(level2[0], s_list);

    EXPECT_TRUE(tree.is_empty());
}

/**
 * @brief Removing one of two siblings leaves the other reachable via root's m_son.
 *
 * Graph: star_5.
 */
TEST_F(TreeTest, remove_one_sibling_leaves_other)
{
    const ColoredGraph graph = make_star_5();
    const std::vector<ColoredGraph> s_list = {graph};

    Tree tree(0U, false, m_fwd_hist);
    const NodePtr root = tree.get_root();
    const std::vector<NodePtr> children =
        tree.add_tree_level({{1U, root}, {2U, root}}, s_list);

    tree.remove_node(children[0], s_list);

    EXPECT_NE(root->m_son, nullptr);
}

/**
 * @brief Attempting to remove a non-leaf (internal) node throws PatternException.
 *
 * Graph: path_4. Node 1 has a child (node 2) so it is internal.
 */
TEST_F(TreeTest, remove_node_non_leaf_throws)
{
    const ColoredGraph graph = make_path_4();
    const std::vector<ColoredGraph> s_list = {graph};

    Tree tree(0U, false, m_fwd_hist);
    const NodePtr root = tree.get_root();
    const std::vector<NodePtr> level1 = tree.add_tree_level({{1U, root}}, s_list);
    tree.add_tree_level({{2U, level1[0]}}, s_list);

    EXPECT_THROW(tree.remove_node(level1[0], s_list), DeleteNodeException);
}

// ── get_node_by_depth ─────────────────────────────────────────────────────────

/**
 * @brief Walking from a depth-2 node to depth 1 returns the correct ancestor.
 *
 * Graph: path_4. Chain root(depth 0) → vertex 1 (depth 1) → vertex 2 (depth 2).
 */
TEST_F(TreeTest, found_at_depth_1)
{
    const ColoredGraph graph = make_path_4();
    const std::vector<ColoredGraph> s_list = {graph};

    Tree tree(0U, false, m_fwd_hist);
    const NodePtr root = tree.get_root();
    const std::vector<NodePtr> level1 = tree.add_tree_level({{1U, root}}, s_list);
    const std::vector<NodePtr> level2 = tree.add_tree_level({{2U, level1[0]}}, s_list);

    const NodePtr found = tree.get_node_by_depth(level2[0], 1U);

    EXPECT_NE(found, nullptr);
    EXPECT_EQ(found->m_index, 1U);
    EXPECT_EQ(found->m_depth, 1U);
}

/**
 * @brief Requesting a depth greater than the tree has returns nullptr.
 *
 * Graph: path_4. Tree has depth 1 only.
 */
TEST_F(TreeTest, depth_beyond_tree_returns_nullptr)
{
    const ColoredGraph graph = make_path_4();
    const std::vector<ColoredGraph> s_list = {graph};

    Tree tree(0U, false, m_fwd_hist);
    const NodePtr root = tree.get_root();
    tree.add_tree_level({{1U, root}}, s_list);

    const NodePtr found = tree.get_node_by_depth(root, 5U);
    EXPECT_EQ(found, nullptr);
}
