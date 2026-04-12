#pragma once

#include "SgfException.h"
#include "SgfReturnCode.h"

#include <string>

namespace sgf
{

/**
 * @brief Thrown when a requested file path does not exist or cannot be opened.
 *
 * Distinct from GraphConstructionException so callers can distinguish
 * "file not found" from "file found but malformed".
 * CLI exit code: SgfReturnCode::PATH_DOESNT_EXIST.
 */
class SgfPathDoesntExistException : public SgfException
{
public:
    /**
     * @brief Constructs an SgfPathDoesntExistException.
     * @param message Description of the missing or inaccessible path.
     */
    explicit SgfPathDoesntExistException(const std::string& message)
        : SgfException(message)
    {
    }

    /**
     * @brief Returns the CLI exit code for this exception type.
     * @return SgfReturnCode::PATH_DOESNT_EXIST.
     */
    SgfReturnCode return_code() const noexcept override
    {
        return SgfReturnCode::PATH_DOESNT_EXIST;
    }
};

}  // namespace sgf
