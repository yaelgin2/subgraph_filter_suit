#pragma once

#include <cstdint>
#include <limits>

namespace sgf
{

class SgfConstants
{
public:
    /// Maximum vertex color that can be stored in ColoredGraph's int32_t array.
    static constexpr uint32_t MAX_VERTEX_COLOR = static_cast<uint32_t>((1 << 24) - 1);
};

}  // namespace sgf
