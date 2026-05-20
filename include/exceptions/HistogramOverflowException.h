#pragma once

#include "SgfException.h"
#include "SgfReturnCode.h"

#include <string>

namespace sgf
{

/**
 * @brief Thrown when a histogram score or log-probability sum reaches ±infinity.
 *
 * Raised during pattern-matching when floating-point arithmetic on log-
 * probability accumulators overflows to +inf or underflows to -inf.
 * Indicates the graph density or colour distribution is outside the
 * representable range for the current scoring model.
 * CLI exit code: SgfReturnCode::HISTOGRAM_OVERFLOW.
 */
class HistogramOverflowException : public SgfException
{
public:
    /**
     * @brief Constructs a HistogramOverflowException.
     * @param message Description of the overflow or underflow condition.
     */
    explicit HistogramOverflowException(const std::string& message)
        : SgfException(message)
    {
    }

    /**
     * @brief Returns the CLI exit code for this exception type.
     * @return SgfReturnCode::HISTOGRAM_OVERFLOW.
     */
    [[nodiscard]] SgfReturnCode return_code() const noexcept override
    {
        return SgfReturnCode::HISTOGRAM_OVERFLOW;
    }
};

}  // namespace sgf
