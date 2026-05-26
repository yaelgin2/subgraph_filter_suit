#pragma once

#include "SgfException.h"
#include "SgfReturnCode.h"

#include <string>

namespace sgf
{

/**
 * @brief Thrown when a required filesystem path does not exist.
 *
 * Distinct from SgfDirectoryCreationException, which covers creation failures.
 * This exception signals that a path expected to exist (e.g. a directory
 * passed to get_files_in_directory) is absent or is not the expected type.
 * CLI exit code: SgfReturnCode::PATH_DOESNT_EXIST.
 */
class SgfPathDoesntExistException : public SgfException
{
public:
    /**
     * @brief Constructs an SgfPathDoesntExistException.
     * @param message Description of the failure, including the missing path.
     */
    explicit SgfPathDoesntExistException(const std::string& message)
        : SgfException(message)
    {
    }

    /**
     * @brief Returns the CLI exit code for this exception type.
     * @return SgfReturnCode::PATH_DOESNT_EXIST.
     */
    [[nodiscard]] SgfReturnCode return_code() const noexcept override
    {
        return SgfReturnCode::PATH_DOESNT_EXIST;
    }
};

}  // namespace sgf
