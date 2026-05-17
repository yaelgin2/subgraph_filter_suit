#include "GeneralColorHist.h"

#include "LogLevel.h"

#include <algorithm>
#include <random>

namespace sgf
{

GeneralColorHist::GeneralColorHist(const uint32_t num_colors, LoggerHandler logger)
    : m_num_colors(num_colors)
    , m_logger(std::move(logger))
{
    m_logger.log(LogLevel::DEBUG, "Initating general hist with " + std::to_string(m_num_colors) + " colors.");
}

void GeneralColorHist::update_hist_increase_tree_count(
    const uint32_t pattern_depth,
    const uint32_t current_vertex_color)
{
    m_logger.log(LogLevel::DEBUG,
        "GeneralColorHist::increase_tree_count(depth=" + std::to_string(pattern_depth) +
        ", color=" + std::to_string(current_vertex_color) +
        ") hist_before: " + format_hist_matrix(m_number_of_trees));

    if (current_vertex_color >= m_num_colors)
    {
        m_logger.log(LogLevel::ERROR, "GeneralColorHist::increase_tree_count: color=" + std::to_string(current_vertex_color) + " out of range (num_colors=" + std::to_string(m_num_colors) + ")");
        throw PatternException("color index out of range");
    }
    if (pattern_depth > MAX_PATTERN_DEPTH)
    {
        m_logger.log(LogLevel::ERROR, "GeneralColorHist::increase_tree_count: depth=" + std::to_string(pattern_depth) + " exceeds maximum");
        throw PatternException("pattern depth exceeds maximum");
    }
    while (pattern_depth >= m_number_of_trees.size())
    {
        m_number_of_trees.push_back(std::vector<uint32_t>(m_num_colors, 0U));
    }

    ++m_number_of_trees[pattern_depth][current_vertex_color];

    m_logger.log(LogLevel::DEBUG,
        "GeneralColorHist::increase_tree_count hist_after: " +
        format_hist_matrix(m_number_of_trees));
}

void GeneralColorHist::update_hist_decrease_tree_count(
    const uint32_t pattern_depth,
    const uint32_t current_vertex_color)
{
    m_logger.log(LogLevel::DEBUG,
        "GeneralColorHist::decrease_tree_count(depth=" + std::to_string(pattern_depth) +
        ", color=" + std::to_string(current_vertex_color) +
        ") hist_before: " + format_hist_matrix(m_number_of_trees));

    if (pattern_depth >= static_cast<uint32_t>(m_number_of_trees.size()))
    {
        m_logger.log(LogLevel::ERROR, "GeneralColorHist::decrease_tree_count: depth=" + std::to_string(pattern_depth) + " not in histogram (size=" + std::to_string(m_number_of_trees.size()) + ")");
        throw PatternException("pattern depth not in histogram");
    }
    if (current_vertex_color >= m_num_colors)
    {
        m_logger.log(LogLevel::ERROR, "GeneralColorHist::decrease_tree_count: color=" + std::to_string(current_vertex_color) + " out of range (num_colors=" + std::to_string(m_num_colors) + ")");
        throw PatternException("color index out of range");
    }
    if (m_number_of_trees[pattern_depth][current_vertex_color] == 0U)
    {
        m_logger.log(LogLevel::ERROR, "GeneralColorHist::decrease_tree_count: cell [depth=" + std::to_string(pattern_depth) + "][color=" + std::to_string(current_vertex_color) + "] is already zero");
        throw PatternException("cannot decrement a zero cell");
    }
    --m_number_of_trees[pattern_depth][current_vertex_color];

    m_logger.log(LogLevel::DEBUG,
        "GeneralColorHist::decrease_tree_count hist_after: " +
        format_hist_matrix(m_number_of_trees));
}

std::tuple<int32_t, int32_t, uint32_t> GeneralColorHist::get_color_to_add(
    const uint32_t threshold,
    const bool is_random) const
{
    struct Candidate
    {
        int32_t color;
        int32_t depth_index;
        uint32_t weight;
    };

    std::vector<Candidate> candidates;
    uint32_t total_weight = 0U;

    for (uint32_t color = 0U; color < m_num_colors; ++color)
    {
        for (uint32_t depth_index = 0U;
             depth_index < static_cast<uint32_t>(m_number_of_trees.size());
             ++depth_index)
        {
            const uint32_t support = m_number_of_trees[depth_index][color];

            if (support == 0U || support < threshold)
            {
                continue;
            }

            candidates.push_back({
                static_cast<int32_t>(color),
                static_cast<int32_t>(depth_index),
                support
            });

            total_weight += support;
        }
    }

    if (candidates.empty())
    {
        return {NO_CANDIDATE_COLOR, NO_CANDIDATE_DEPTH, NO_CANDIDATE_WEIGHT};
    }

    if (!is_random)
    {
        return {candidates.back().color, candidates.back().depth_index, candidates.back().weight};
    }

    // Weighted random sampling: pick a candidate with probability proportional to its support.
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<double> dist(0.0, static_cast<double>(total_weight));

    const double random_value = dist(rng);
    double accumulated = 0.0;

    for (const Candidate& candidate : candidates)
    {
        accumulated += static_cast<double>(candidate.weight);
        if (random_value <= accumulated)
        {
            return {candidate.color, candidate.depth_index, candidate.weight};
        }
    }

    return {candidates.back().color, candidates.back().depth_index, candidates.back().weight};
}

}  // namespace sgf
