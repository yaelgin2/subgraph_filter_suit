#pragma once

#include "SgfException.h"
#include "SgfReturnCode.h"

#include <string>

namespace sgf
{

/**
 * @brief Thrown when a motif or path occurrence count exceeds uint32_t capacity.
 *
 * Raised during enumeration preprocessing when incrementing a count in the
 * result map would overflow uint32_t::max. Indicates the input graph is too
 * dense for the current counter representation.
 * CLI exit code: SgfReturnCode::ENUMERATION_OVERFLOW.
 */
class EnumerationOverflowException : public SgfException
{
public:
    /**
     * @brief Constructs an EnumerationOverflowException.
     * @param message Description of the overflow condition.
     */
    explicit EnumerationOverflowException(const std::string& message)
        : SgfException(message)
    {
    }

    /**
     * @brief Returns the CLI exit code for this exception type.
     * @return SgfReturnCode::ENUMERATION_OVERFLOW.
     */
    [[nodiscard]] SgfReturnCode return_code() const noexcept override
    {
        return SgfReturnCode::ENUMERATION_OVERFLOW;
    }
};

}  // namespace sgf
