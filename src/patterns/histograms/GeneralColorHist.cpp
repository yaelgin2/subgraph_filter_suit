#include "GeneralColorHist.h"
#include <algorithm>
#include <random>
#include <iostream>
#include <limits>


/**
 * @brief Constructor initializes the histogram matrix to zeros.
 */
GeneralColorHist::GeneralColorHist(int32_t num_colors): C(num_colors)
{
}

void GeneralColorHist::update_hist_increase_tree_count(
    const uint32_t pattern_depth,
    const uint32_t current_vertex_color)
{
    
    while(pattern_depth >= m_number_of_trees.size())
    {
         this->m_number_of_trees.push_back(std::vector<uint32_t>(C, 0));
    }
   
    ++m_number_of_trees[pattern_depth][current_vertex_color];

}

void GeneralColorHist::update_hist_decrease_tree_count(
    const uint32_t pattern_depth,
    const uint32_t current_vertex_color)
{
    --m_number_of_trees[pattern_depth][current_vertex_color];
}

std::tuple<int32_t, int32_t, uint32_t> GeneralColorHist::get_color_to_add(uint32_t threshold, bool is_random)
{
    struct Candidate {
        int32_t color;
        int32_t node;
        uint32_t  weight;
    };

    std::vector<Candidate> candidates;
    uint32_t total_weight = 0.0;

    // 1. Collect all legal candidates
    for (uint32_t c = 0; c < static_cast<uint32_t>(C); ++c) {
        for (uint32_t d = 0; d < m_number_of_trees.size(); ++d) {

            uint32_t support = m_number_of_trees[d][c];
            
            if (support < threshold || support == 0)
                continue;

            candidates.push_back({
                static_cast<int32_t>(c),
                static_cast<int32_t>(d),
                support
            });

            total_weight += support;
        }
    }

    if (candidates.empty()) {
        return {-1, -1, 0};
    }

    if (!is_random) {
        // Pick the highest-weight candidate deterministically.
        return {candidates.back().color, candidates.back().node, candidates.back().weight};
    }

    // 2. Weighted random sampling.
    //
    // Imagine placing all candidates end-to-end on a number line from 0 to
    // total_weight, each occupying a segment whose length equals its support:
    //
    //   |-- A (10) --|------ B (30) ------|---------------- C (60) ----------------|
    //   0           10                   40                                       100
    //
    // A uniform random value r is drawn from [0, total_weight). We then walk
    // through the candidates, accumulating their weights. The first candidate
    // whose cumulative sum reaches or exceeds r is selected. Because each
    // candidate's segment is proportional to its support, the probability of
    // landing in it equals support / total_weight — exactly the desired bias.
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<double> dist(0.0, static_cast<double>(total_weight));

    double r = dist(rng);
    double acc = 0.0;

    for (const auto& cand : candidates) {
        acc += cand.weight;
        if (r <= acc) {
            return {cand.color, cand.node, cand.weight};
        }
    }

    // Fallback for numerical safety: floating-point rounding can push r just
    // above total_weight, so return the last candidate rather than failing.
    return {candidates.back().color, candidates.back().node, candidates.back().weight};
}

std::vector<double> GeneralColorHist::compute_softmax(const std::vector<uint32_t>& input) const
{
    std::vector<double> softmax_values(input.size());
    double max_val = *std::max_element(input.begin(), input.end());
    double sum_exp = 0.0;

    for (const auto& val : input) {
        sum_exp += std::exp(static_cast<double>(val) - max_val);
    }

    for (size_t i = 0; i < input.size(); ++i) {
        softmax_values[i] = std::exp(static_cast<double>(input[i]) - max_val) / sum_exp;
    }

    return softmax_values;
}