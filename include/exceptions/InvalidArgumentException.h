#pragma once

#include "SgfException.h"
#include "SgfReturnCode.h"

#include <string>

namespace sgf {

/**
 * @brief Thrown when a function receives an invalid argument.
 *
 * Examples: mismatched vector sizes, vertex IDs out of range.
 * CLI exit code: SgfReturnCode::INVALID_ARGUMENT.
 */
class InvalidArgumentException : public SgfException {
public:
    /**
     * @brief Constructs an InvalidArgumentException.
     * @param message Description of the invalid argument.
     */
    explicit InvalidArgumentException(const std::string& message) : SgfException(message)
    {
    }

    /**
     * @brief Returns the CLI exit code for this exception type.
     * @return SgfReturnCode::INVALID_ARGUMENT.
     */
    SgfReturnCode return_code() const noexcept override
    {
        return SgfReturnCode::INVALID_ARGUMENT;
    }
};

}  // namespace sgf
