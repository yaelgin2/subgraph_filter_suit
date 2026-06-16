#pragma once

#include <cstdint>

namespace sgf
{

/**
 * @brief Project-wide compile-time constants.
 */
class SgfConstants
{
public:
    static constexpr uint8_t BITS_PER_COLOR = 24;
    /// Maximum vertex color that can be stored in ColoredGraph's uint32_t array.
    static constexpr uint32_t MAX_VERTEX_COLOR = static_cast<uint32_t>((1U << BITS_PER_COLOR) - 1);
    static constexpr uint8_t MOTIF_SIZE = 4;
    /// Default number of worker threads used by preprocessing and filtering stages.
    static constexpr uint32_t DEFAULT_THREAD_NUMBER = 10U;
};

}  // namespace sgf
