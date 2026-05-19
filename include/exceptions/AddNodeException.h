#pragma once

#include "SgfException.h"
#include "SgfReturnCode.h"

#include <string>

namespace sgf
{

/**
 * @brief Thrown when a node cannot be added to the tree (e.g. null parent).
 *
 * CLI exit code: SgfReturnCode::ADD_NODE_ERROR.
 */
class AddNodeException : public SgfException
{
public:
    /**
     * @brief Constructs an AddNodeException.
     * @param message Description of the add-node error.
     */
    explicit AddNodeException(const std::string& message)
        : SgfException(message)
    {
    }

    /**
     * @brief Returns the CLI exit code for this exception type.
     * @return SgfReturnCode::ADD_NODE_ERROR.
     */
    [[nodiscard]] SgfReturnCode return_code() const noexcept override
    {
        return SgfReturnCode::ADD_NODE_ERROR;
    }
};

}  // namespace sgf
