#pragma once

#include "SgfException.h"
#include "SgfReturnCode.h"

#include <string>

namespace sgf
{

/**
 * @brief Thrown when a CLI flag or library argument is invalid.
 *
 * Used by CLI tools to report missing or malformed arguments.
 * CLI exit code: SgfReturnCode::INVALID_ARGUMENT.
 */
class SgfInvalidArgumentException : public SgfException
{
public:
    /**
     * @brief Constructs an SgfInvalidArgumentException.
     * @param message Description of the invalid argument.
     */
    explicit SgfInvalidArgumentException(const std::string& message)
        : SgfException(message)
    {
    }

    /**
     * @brief Returns the CLI exit code for this exception type.
     * @return SgfReturnCode::INVALID_ARGUMENT.
     */
    [[nodiscard]] SgfReturnCode return_code() const noexcept override
    {
        return SgfReturnCode::INVALID_ARGUMENT;
    }
};

}  // namespace sgf
