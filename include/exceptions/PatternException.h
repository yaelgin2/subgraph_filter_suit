#pragma once

#include "SgfException.h"
#include "SgfReturnCode.h"

#include <string>

namespace sgf
{

/**
 * @brief Thrown when a pattern-subsystem operation receives an invalid argument
 *        or encounters an illegal state (histogram underflow, depth out of range,
 *        invalid tree structure).
 *
 * CLI exit code: SgfReturnCode::PATTERN_ERROR.
 */
class PatternException : public SgfException
{
public:
    /**
     * @brief Constructs a PatternException.
     * @param message Description of the pattern-subsystem error.
     */
    explicit PatternException(const std::string& message)
        : SgfException(message)
    {
    }

    /**
     * @brief Returns the CLI exit code for this exception type.
     * @return SgfReturnCode::PATTERN_ERROR.
     */
    [[nodiscard]] SgfReturnCode return_code() const noexcept override
    {
        return SgfReturnCode::PATTERN_ERROR;
    }
};

}  // namespace sgf
