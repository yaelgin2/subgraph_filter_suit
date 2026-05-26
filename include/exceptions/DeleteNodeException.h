#pragma once

#include "SgfException.h"
#include "SgfReturnCode.h"

#include <string>

namespace sgf
{

/**
 * @brief Thrown when a node cannot be deleted from the tree (e.g. non-leaf node).
 *
 * CLI exit code: SgfReturnCode::DELETE_NODE_ERROR.
 */
class DeleteNodeException : public SgfException
{
public:
    /**
     * @brief Constructs a DeleteNodeException.
     * @param message Description of the delete-node error.
     */
    explicit DeleteNodeException(const std::string& message)
        : SgfException(message)
    {
    }

    /**
     * @brief Returns the CLI exit code for this exception type.
     * @return SgfReturnCode::DELETE_NODE_ERROR.
     */
    [[nodiscard]] SgfReturnCode return_code() const noexcept override
    {
        return SgfReturnCode::DELETE_NODE_ERROR;
    }
};

}  // namespace sgf
