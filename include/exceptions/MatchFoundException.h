#pragma once

#include "SgfException.h"
#include "SgfReturnCode.h"

namespace sgf
{

/**
 * @brief Thrown internally to unwind the search stack when stop_after_first is
 *        active and a match has been found.
 *
 * Never propagates to the caller — caught in find_all's thread lambda.
 * CLI exit code: SgfReturnCode::MATCH_FOUND.
 */
class MatchFoundException : public SgfException
{
public:
    /**
     * @brief Constructs a MatchFoundException.
     */
    MatchFoundException()
        : SgfException("match found — early termination requested")
    {
    }

    /**
     * @brief Returns the CLI exit code for this exception type.
     * @return SgfReturnCode::MATCH_FOUND.
     */
    [[nodiscard]] SgfReturnCode return_code() const noexcept override
    {
        return SgfReturnCode::MATCH_FOUND;
    }
};

}  // namespace sgf
