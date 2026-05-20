#pragma once

#include "SgfException.h"
#include "SgfReturnCode.h"

#include <string>

namespace sgf
{

/**
 * @brief Thrown when a required directory cannot be created.
 *
 * Distinct from SgfPathExistsException, which covers missing files.
 * This exception signals a filesystem permission or I/O error during
 * directory creation. CLI exit code: SgfReturnCode::DIRECTORY_CREATION_ERROR.
 */
class SgfDirectoryCreationException : public SgfException
{
public:
    /**
     * @brief Constructs an SgfDirectoryCreationException.
     * @param message Description of the failure, including the target path.
     */
    explicit SgfDirectoryCreationException(const std::string& message)
        : SgfException(message)
    {
    }

    /**
     * @brief Returns the CLI exit code for this exception type.
     * @return SgfReturnCode::DIRECTORY_CREATION_ERROR.
     */
    [[nodiscard]] SgfReturnCode return_code() const noexcept override
    {
        return SgfReturnCode::DIRECTORY_CREATION_ERROR;
    }
};

}  // namespace sgf
