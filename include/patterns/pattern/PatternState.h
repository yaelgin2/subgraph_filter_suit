#pragma once

#include "SingleGraphHistogram.h"
#include "BoostGraph.h"

#include <cstdint>
#include <memory>
#include <unordered_set>
#include <vector>

namespace sgf
{

/**
 * @brief One candidate pattern inside the beam.
 *
 * Each PatternState tracks a single match: one specific mapping of
 * pattern vertices to S-graph vertices.  The histogram maintains
 * candidate caches relative to this single match's vertex set.
 *
 * pattern_vertex_color_log_prob is maintained incrementally: whenever a
 * vertex of colour c is added to the pattern, the caller adds log(p[c])
 * to this field.  This avoids iterating over all pattern vertices on every
 * PatternScorer::score call.
 */
struct PatternState
{
    BoostGraph                            pattern;
    std::unique_ptr<SingleGraphHistogram> hist;         ///< Owned histogram.
    std::vector<uint32_t>                 match_path;   ///< S-vertex at each pattern depth.
    double                                beam_score         = 0.0;

    /** Σ log(p[color(v)]) over all vertices currently in the pattern.
     *  Updated in SingleGraphPatternFinder immediately after add_vertex. */
    double                                pattern_vertex_color_log_prob = 0.0;
};

} // namespace sgf
