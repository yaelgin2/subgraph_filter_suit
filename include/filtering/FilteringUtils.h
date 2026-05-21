#pragma once

#include <vector>

namespace sgf
{

/**
 * @brief Result of a filter pass: one bool per library graph.
 *
 * true  — candidate is pruned (query cannot be isomorphic to this library graph).
 * false — candidate survives and must be checked further.
 */
using FilterResult = std::vector<bool>;

}  // namespace sgf
