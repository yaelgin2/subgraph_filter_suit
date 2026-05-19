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
    /// Growing pattern subgraph (compact colour IDs; edges doubled for undirected).
    BoostGraph                            pattern;

    /// Histogram that tracks candidates and their scores relative to this match.
    std::unique_ptr<SingleGraphHistogram> hist;

    /// match_path[depth] = S-graph vertex absorbed at that depth.
    /// Used to map pattern node indices back to S-graph vertices when adding edges.
    std::vector<uint32_t>                 match_path;

    /// Reserved for future use; not currently updated by the finder.
    double                                beam_score = 0.0;

    /** Σ log(p[color(v)]) over all vertices currently in the pattern.
     *  Maintained incrementally in SingleGraphPatternFinder::expand_one_state
     *  to avoid re-iterating the pattern on every PatternScorer::score call. */
    double                                pattern_vertex_color_log_prob = 0.0;
};

} // namespace sgf
