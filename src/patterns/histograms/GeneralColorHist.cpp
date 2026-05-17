#include "GeneralColorHist.h"

#include "LogLevel.h"
#include "LoggerHandler.h"
#include "PatternException.h"

#include <cstdint>
#include <random>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace sgf
{

GeneralColorHist::GeneralColorHist(const uint32_t num_colors, LoggerHandler logger)
    : m_num_colors(num_colors)
    , m_logger(std::move(logger))
{
}

void GeneralColorHist::update_hist_increase_tree_count(const uint32_t pattern_depth,
                                                       const uint32_t current_vertex_color)
{

    if (current_vertex_color >= m_num_colors)
    {
        m_logger.log(
            LogLevel::ERROR,
            "GeneralColorHist::increase_tree_count: color=" + std::to_string(current_vertex_color) +
                " out of range (num_colors=" + std::to_string(m_num_colors) + ")");
        throw PatternException("color index out of range");
    }
    if (pattern_depth > MAX_PATTERN_DEPTH)
    {
        m_logger.log(LogLevel::ERROR, "GeneralColorHist::increase_tree_count: depth=" +
                                          std::to_string(pattern_depth) + " exceeds maximum");
        throw PatternException("pattern depth exceeds maximum");
    }
    while (pattern_depth >= m_number_of_trees.size())
    {
        m_number_of_trees.emplace_back(m_num_colors, 0U);
    }

    ++m_number_of_trees[pattern_depth][current_vertex_color];
}

void GeneralColorHist::update_hist_decrease_tree_count(const uint32_t pattern_depth,
                                                       const uint32_t current_vertex_color)
{

    if (pattern_depth >= static_cast<uint32_t>(m_number_of_trees.size()))
    {
        m_logger.log(
            LogLevel::ERROR,
            "GeneralColorHist::decrease_tree_count: depth=" + std::to_string(pattern_depth) +
                " not in histogram (size=" + std::to_string(m_number_of_trees.size()) + ")");
        throw PatternException("pattern depth not in histogram");
    }
    if (current_vertex_color >= m_num_colors)
    {
        m_logger.log(
            LogLevel::ERROR,
            "GeneralColorHist::decrease_tree_count: color=" + std::to_string(current_vertex_color) +
                " out of range (num_colors=" + std::to_string(m_num_colors) + ")");
        throw PatternException("color index out of range");
    }
    if (m_number_of_trees[pattern_depth][current_vertex_color] == 0U)
    {
        m_logger.log(
            LogLevel::ERROR,
            "GeneralColorHist::decrease_tree_count: cell [depth=" + std::to_string(pattern_depth) +
                "][color=" + std::to_string(current_vertex_color) + "] is already zero");
        throw PatternException("cannot decrement a zero cell");
    }
    --m_number_of_trees[pattern_depth][current_vertex_color];
}

namespace
{

/**
 * @brief Internal candidate for get_color_to_add weighted selection.
 */
struct Candidate
{
    int32_t m_color;        ///< Color index of this candidate.
    int32_t m_depth_index;  ///< Depth index of this candidate.
    uint32_t m_weight;      ///< Support weight of this candidate.
};

/**
 * @brief Build a list of candidates above the given threshold.
 * @param histogram The [depth][color] tree-count matrix.
 * @param num_colors Number of distinct vertex colors.
 * @param threshold Minimum tree count for a cell to be considered.
 * @param total_weight Output: sum of all candidate weights.
 * @return Vector of candidates meeting the threshold.
 */
std::vector<Candidate> build_candidates(const std::vector<std::vector<uint32_t>>& histogram,
                                        const uint32_t num_colors, const uint32_t threshold,
                                        uint32_t& total_weight)
{
    std::vector<Candidate> candidates;
    for (uint32_t color = 0U; color < num_colors; ++color)
    {
        for (uint32_t depth_index = 0U; depth_index < static_cast<uint32_t>(histogram.size());
             ++depth_index)
        {
            const uint32_t support = histogram[depth_index][color];
            if (support == 0U || support < threshold)
            {
                continue;
            }
            candidates.push_back(
                {static_cast<int32_t>(color), static_cast<int32_t>(depth_index), support});
            total_weight += support;
        }
    }
    return candidates;
}

/**
 * @brief Select a candidate using weighted random sampling.
 * @param candidates Non-empty list of candidates.
 * @param total_weight Sum of all candidate weights.
 * @return The selected candidate.
 */
Candidate select_weighted_random(const std::vector<Candidate>& candidates,
                                 const uint32_t total_weight)
{
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<double> dist(0.0, static_cast<double>(total_weight));

    const double random_value = dist(rng);
    double accumulated = 0.0;

    for (const Candidate& candidate : candidates)
    {
        accumulated += static_cast<double>(candidate.m_weight);
        if (random_value <= accumulated)
        {
            return candidate;
        }
    }

    return candidates.back();
}

}  // namespace

std::tuple<int32_t, int32_t, uint32_t>
GeneralColorHist::get_color_to_add(const uint32_t threshold, const bool is_random) const
{
    uint32_t total_weight = 0U;
    const std::vector<Candidate> candidates =
        build_candidates(m_number_of_trees, m_num_colors, threshold, total_weight);

    if (candidates.empty())
    {
        return {NO_CANDIDATE_COLOR, NO_CANDIDATE_DEPTH, NO_CANDIDATE_WEIGHT};
    }

    if (!is_random)
    {
        return {candidates.back().m_color, candidates.back().m_depth_index,
                candidates.back().m_weight};
    }

    const Candidate selected = select_weighted_random(candidates, total_weight);
    return {selected.m_color, selected.m_depth_index, selected.m_weight};
}

}  // namespace sgf
