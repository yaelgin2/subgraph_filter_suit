#pragma once

#include <cstdint>

namespace sgf {

/**
 * @brief CLI process exit codes for each SgfException subclass.
 *
 * Every concrete exception type maps to exactly one value in this enum.
 * CLI tools cast the result of SgfException::return_code() to int and
 * pass it to exit(). Values must be unique and non-zero.
 */
enum class SgfReturnCode : int32_t
{
    INVALID_ARGUMENT = 2,
    GRAPH_CONSTRUCTION_ERROR = 3,
};

}  // namespace sgf
