#include "patterns/tree/Tree.h"

#include "FileLogger.h"
#include "LoggerHandler.h"
#include "exceptions/AddNodeException.h"
#include "exceptions/DeleteNodeException.h"
#include "graph/ColoredGraph.h"
#include "patterns/tree/Node.h"

#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <utility>
#include <vector>

using namespace sgf;

// ── Fixture ───────────────────────────────────────────────────────────────────

class TreeTest : public ::testing::Test
{
protected:
    /**
     * @brief Undirected path graph: 4 vertices (0-1-2-3), all color 0.
     */
    ColoredGraph make_path_4()
    {
        std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {1U, 2U}, {2U, 3U}};
        return ColoredGraph(4U, edges, {0U, 0U, 0U, 0U}, false);
    }

    /**
     * @brief Undirected path graph: 4 vertices (0-1-2-3), colors 0-3.
     */
    ColoredGraph make_path_4_colored()
    {
        std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {1U, 2U}, {2U, 3U}};
        return ColoredGraph(4U, edges, {0U, 1U, 2U, 3U}, false);
    }

    /**
     * @brief Undirected star graph: 5 vertices, center=0, spokes to 1-4, all color 0.
     */
    ColoredGraph make_star_5()
    {
        std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {0U, 2U}, {0U, 3U}, {0U, 4U}};
        return ColoredGraph(5U, edges, {0U, 0U, 0U, 0U, 0U}, false);
    }

    /**
     * @brief Isolated single vertex, color 0.
     */
    ColoredGraph make_isolated()
    {
        std::vector<std::pair<uint32_t, uint32_t>> edges;
        return ColoredGraph(1U, edges, {0U}, false);
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
 * @brief Constructing an undirected tree for an isolated vertex succeeds.
 */
TEST_F(TreeTest, undirected_isolated_vertex_succeeds)
{
    const ColoredGraph isolated_graph = make_isolated();
    Tree tree(0U, isolated_graph, null_logger());
    EXPECT_FALSE(tree.is_empty());
    EXPECT_EQ(tree.get_root()->m_index, 0U);
    EXPECT_EQ(tree.get_root()->m_depth, 0U);
}

/**
 * @brief Constructing an undirected tree for a path graph succeeds.
 */
TEST_F(TreeTest, undirected_path_succeeds)
{
    const ColoredGraph path_graph = make_path_4();
    EXPECT_NO_THROW(Tree(0U, path_graph, null_logger()));
}

/**
 * @brief Constructing a directed tree succeeds.
 */
TEST_F(TreeTest, directed_chain_succeeds)
{
    const ColoredGraph directed_graph = make_directed_chain();
    EXPECT_NO_THROW(Tree(0U, directed_graph, null_logger()));
}

// ── add_tree_level ────────────────────────────────────────────────────────────

/**
 * @brief Passing an empty index list returns an empty node vector.
 */
TEST_F(TreeTest, empty_new_indexes_returns_empty)
{
    const ColoredGraph graph = make_path_4();
    Tree tree(0U, graph, null_logger());
    const std::vector<NodePtr> result = tree.add_tree_level({});
    EXPECT_TRUE(result.empty());
}

/**
 * @brief Passing siblings of the same parent non-consecutively throws AddNodeException.
 */
TEST_F(TreeTest, non_grouped_siblings_throws)
{
    const ColoredGraph graph = make_star_5();
    Tree tree(0U, graph, null_logger());
    const NodePtr root = tree.get_root();
    const std::vector<NodePtr> first_children = tree.add_tree_level({{1U, root}, {2U, root}});
    const NodePtr child_1 = first_children[0];

    EXPECT_THROW(tree.add_tree_level({{3U, root}, {4U, child_1}, {1U, root}}), AddNodeException);
}

/**
 * @brief Adding a single child under the root creates a depth-1 node.
 */
TEST_F(TreeTest, single_child_under_root)
{
    const ColoredGraph graph = make_path_4();
    Tree tree(0U, graph, null_logger());
    const NodePtr root = tree.get_root();
    const std::vector<NodePtr> children = tree.add_tree_level({{1U, root}});

    EXPECT_EQ(children.size(), 1U);
    EXPECT_EQ(children[0]->m_index, 1U);
    EXPECT_EQ(children[0]->m_depth, 1U);
    EXPECT_EQ(children[0]->m_parent.lock(), root);
}

/**
 * @brief Adding two children under the root creates two depth-1 nodes.
 */
TEST_F(TreeTest, multiple_children_same_parent)
{
    const ColoredGraph graph = make_star_5();
    Tree tree(0U, graph, null_logger());
    const NodePtr root = tree.get_root();
    const std::vector<NodePtr> children = tree.add_tree_level({{1U, root}, {2U, root}});

    EXPECT_EQ(children.size(), 2U);
    EXPECT_NE(root->m_son, nullptr);
}

/**
 * @brief Adding two levels produces a chain root→1→2 with correct depths.
 */
TEST_F(TreeTest, two_level_tree)
{
    const ColoredGraph graph = make_path_4();
    Tree tree(0U, graph, null_logger());
    const NodePtr root = tree.get_root();
    const std::vector<NodePtr> level1 = tree.add_tree_level({{1U, root}});
    const std::vector<NodePtr> level2 = tree.add_tree_level({{2U, level1[0]}});

    EXPECT_EQ(level2[0]->m_depth, 2U);
    EXPECT_EQ(level2[0]->m_index, 2U);
}

// ── remove_node ───────────────────────────────────────────────────────────────

/**
 * @brief Removing the only child of the root clears the root's m_son.
 */
TEST_F(TreeTest, remove_only_child_clears_parent_son)
{
    const ColoredGraph graph = make_path_4();
    Tree tree(0U, graph, null_logger());
    const NodePtr root = tree.get_root();
    const std::vector<NodePtr> children = tree.add_tree_level({{1U, root}});

    tree.remove_node(children[0]);

    EXPECT_EQ(root->m_son, nullptr);
}

/**
 * @brief Removing the deepest node in a chain root→1→2 backtracks and empties the tree.
 */
TEST_F(TreeTest, remove_triggers_backtrack_to_root_empties_tree)
{
    const ColoredGraph graph = make_path_4();
    Tree tree(0U, graph, null_logger());
    const NodePtr root = tree.get_root();
    const std::vector<NodePtr> level1 = tree.add_tree_level({{1U, root}});
    const std::vector<NodePtr> level2 = tree.add_tree_level({{2U, level1[0]}});

    tree.remove_node(level2[0]);

    EXPECT_TRUE(tree.is_empty());
}

/**
 * @brief Removing one of two siblings leaves the other reachable via root's m_son.
 */
TEST_F(TreeTest, remove_one_sibling_leaves_other)
{
    const ColoredGraph graph = make_star_5();
    Tree tree(0U, graph, null_logger());
    const NodePtr root = tree.get_root();
    const std::vector<NodePtr> children = tree.add_tree_level({{1U, root}, {2U, root}});

    tree.remove_node(children[0]);

    EXPECT_NE(root->m_son, nullptr);
}

/**
 * @brief Attempting to remove an internal node throws DeleteNodeException.
 */
TEST_F(TreeTest, remove_node_non_leaf_throws)
{
    const ColoredGraph graph = make_path_4();
    Tree tree(0U, graph, null_logger());
    const NodePtr root = tree.get_root();
    const std::vector<NodePtr> level1 = tree.add_tree_level({{1U, root}});
    tree.add_tree_level({{2U, level1[0]}});

    EXPECT_THROW(tree.remove_node(level1[0]), DeleteNodeException);
}

// ── get_node_by_depth ─────────────────────────────────────────────────────────

/**
 * @brief Walking from a depth-2 node to depth 1 returns the correct ancestor.
 */
TEST_F(TreeTest, found_at_depth_1)
{
    const ColoredGraph graph = make_path_4();
    Tree tree(0U, graph, null_logger());
    const NodePtr root = tree.get_root();
    const std::vector<NodePtr> level1 = tree.add_tree_level({{1U, root}});
    const std::vector<NodePtr> level2 = tree.add_tree_level({{2U, level1[0]}});

    const NodePtr found = tree.get_node_by_depth(level2[0], 1U);

    EXPECT_NE(found, nullptr);
    EXPECT_EQ(found->m_index, 1U);
    EXPECT_EQ(found->m_depth, 1U);
}

/**
 * @brief Requesting a depth beyond the tree returns nullptr.
 */
TEST_F(TreeTest, depth_beyond_tree_returns_nullptr)
{
    const ColoredGraph graph = make_path_4();
    Tree tree(0U, graph, null_logger());
    const NodePtr root = tree.get_root();
    tree.add_tree_level({{1U, root}});

    const NodePtr found = tree.get_node_by_depth(root, 5U);
    EXPECT_EQ(found, nullptr);
}

// ── get_color_by_depth_neighbour_counts ──────────────────────────────────────

/**
 * @brief With one depth-1 leaf the function counts its unreached graph neighbours.
 *
 * Graph: path_4 (0-1-2-3, all color 0). Root = vertex 0.
 * Leaf = vertex 1 at depth 1. Path = [1]. Unreached neighbours of 1: {0, 2}
 * (both color 0). Expected: {color=0, pattern_index=0} → 2.
 */
TEST_F(TreeTest, counts_root_neighbours)
{
    const ColoredGraph graph = make_path_4();
    Tree tree(0U, graph, null_logger());
    const NodePtr root = tree.get_root();
    const std::vector<NodePtr> leaves = tree.add_tree_level({{1U, root}});

    CountsMap counts;
    tree.get_color_by_depth_neighbour_counts(leaves, counts);

    EXPECT_EQ(counts.at(std::make_tuple(0U, 0U, false)), 2U);
    EXPECT_EQ(counts.size(), 1U);
}

/**
 * @brief With a depth-2 leaf both path vertices contribute their unreached neighbours.
 *
 * Graph: path_4 (0-1-2-3, all color 0). Root=0, leaf path = [1→2].
 * Vertex 1 (pattern_index=0): neighbours {0, 2}; 2 is in path → only 0 counts.
 * Vertex 2 (pattern_index=1): neighbours {1, 3}; 1 is in path → only 3 counts.
 * Expected: {color=0, 0} → 1, {color=0, 1} → 1.
 */
TEST_F(TreeTest, counts_include_child_neighbours)
{
    const ColoredGraph graph = make_path_4();
    Tree tree(0U, graph, null_logger());
    const NodePtr root = tree.get_root();
    const std::vector<NodePtr> level1 = tree.add_tree_level({{1U, root}});
    const std::vector<NodePtr> level2 = tree.add_tree_level({{2U, level1[0]}});

    CountsMap counts;
    tree.get_color_by_depth_neighbour_counts(level2, counts);

    EXPECT_EQ(counts.at(std::make_tuple(0U, 0U, false)), 1U);
    EXPECT_EQ(counts.at(std::make_tuple(0U, 1U, false)), 1U);
    EXPECT_EQ(counts.size(), 2U);
}

// ── Node/byte accounting & memory safety ────────────────────────────────────────


/**
 * @brief Destroying a Tree that still holds sibling nodes releases all of them.
 *
 * The sibling ring is a circular doubly-linked shared_ptr structure, so simply
 * dropping the tree's root reference is not enough to free it — the ring's own
 * members keep each other alive unless explicitly broken. This is a regression
 * test for that leak (confirmed independently with valgrind).
 */
TEST_F(TreeTest, destroying_tree_with_siblings_releases_all_nodes)
{
    const ColoredGraph graph = make_star_5();
    std::weak_ptr<Node> child_1_observer;
    std::weak_ptr<Node> child_2_observer;

    {
        Tree tree(0U, graph, null_logger());
        const NodePtr root = tree.get_root();
        const std::vector<NodePtr> children = tree.add_tree_level({{1U, root}, {2U, root}});
        child_1_observer = children[0];
        child_2_observer = children[1];
    }

    EXPECT_TRUE(child_1_observer.expired());
    EXPECT_TRUE(child_2_observer.expired());
}

/**
 * @brief With a colored path the neighbour colors are reflected in the count keys.
 *
 * Graph: path_4_colored (colors 0,1,2,3). Root=0(color0).
 * Leaf = vertex 1(color1) at depth 1. Path = [1]. path_set = {1}.
 * Unreached neighbours of 1: vertex 0 (color 0) and vertex 2 (color 2).
 * Expected: {0,0}→1, {2,0}→1.
 */
TEST_F(TreeTest, colored_path_counts_correct_colors)
{
    const ColoredGraph graph = make_path_4_colored();
    Tree tree(0U, graph, null_logger());
    const NodePtr root = tree.get_root();
    const std::vector<NodePtr> leaves = tree.add_tree_level({{1U, root}});

    CountsMap counts;
    tree.get_color_by_depth_neighbour_counts(leaves, counts);

    EXPECT_EQ(counts.at(std::make_tuple(0U, 0U, false)), 1U);
    EXPECT_EQ(counts.at(std::make_tuple(2U, 0U, false)), 1U);
    EXPECT_EQ(counts.size(), 2U);
}
