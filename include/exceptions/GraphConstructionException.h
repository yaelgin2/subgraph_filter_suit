#pragma once

#include "SgfException.h"
#include "SgfReturnCode.h"

#include <string>

namespace sgf
{

/**
 * @brief Thrown when a graph cannot be constructed from the given data.
 *
 * Examples: structurally inconsistent edge lists that cannot form a valid graph.
 * CLI exit code: SgfReturnCode::GRAPH_CONSTRUCTION_ERROR.
 */
class GraphConstructionException : public SgfException
{
public:
    /**
     * @brief Constructs a GraphConstructionException.
     * @param message Description of the construction failure.
     */
    explicit GraphConstructionException(const std::string& message)
        : SgfException(message)
    {
    }

    /**
     * @brief Returns the CLI exit code for this exception type.
     * @return SgfReturnCode::GRAPH_CONSTRUCTION_ERROR.
     */
    [[nodiscard]] SgfReturnCode return_code() const noexcept override
    {
        return SgfReturnCode::GRAPH_CONSTRUCTION_ERROR;
    }
};

}  // namespace sgf
