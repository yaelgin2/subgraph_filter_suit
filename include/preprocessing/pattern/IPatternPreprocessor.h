#pragma once

#include "BoostGraph.h"

#include <cstdint>
#include <unordered_set>
#include <utility>
#include <vector>

namespace sgf
{

/**
 * @brief Result of one pattern search: the found pattern and the set of library graph indices it
 * covers.
 */
using PatternPreprocessorResult = std::pair<BoostGraph, std::unordered_set<uint32_t>>;

/**
 * @class IPatternPreprocessor
 * @brief Interface for pattern preprocessors that mine representative patterns from a graph
 * library.
 *
 * Concrete implementations capture their strategy-specific parameters at construction
 * time. Callers invoke calculate() with no arguments and receive a uniform result type
 * regardless of which strategy is active.
 */
class IPatternPreprocessor
{
public:
    /**
     * @brief Mine patterns from the graph library.
     * @return Vector of mined patterns, each paired with the set of library graph indices it
     * covers.
     */
    virtual std::vector<PatternPreprocessorResult> calculate() = 0;

    /**
     * @brief Virtual destructor.
     */
    virtual ~IPatternPreprocessor() = default;

    IPatternPreprocessor(const IPatternPreprocessor&) = delete;
    IPatternPreprocessor& operator=(const IPatternPreprocessor&) = delete;
    IPatternPreprocessor(IPatternPreprocessor&&) = delete;
    IPatternPreprocessor& operator=(IPatternPreprocessor&&) = delete;

protected:
    IPatternPreprocessor() = default;
};

}  // namespace sgf
