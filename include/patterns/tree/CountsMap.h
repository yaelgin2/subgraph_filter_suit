#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <tuple>
#include <unordered_map>

namespace sgf
{

/**
 * @brief Hash for std::tuple<uint32_t, uint32_t, bool> keys.
 *
 * Packs colour and depth into a uint64_t, then XOR-combines with the direction bit.
 */
struct TupleHash
{
    /**
     * @brief Compute hash from colour, depth, and direction components.
     * @param key The tuple to hash.
     * @return Hash value.
     */
    size_t operator()(const std::tuple<uint32_t, uint32_t, bool>& key) const noexcept
    {
        static constexpr uint32_t UPPER_HALF_SHIFT = 32U;
        const size_t color_depth_hash = std::hash<uint64_t>{}(
            (static_cast<uint64_t>(std::get<0>(key)) << UPPER_HALF_SHIFT) | std::get<1>(key));
        return color_depth_hash ^ (std::hash<bool>{}(std::get<2>(key)) << 1U);
    }
};

/**
 * @brief Map from {colour, depth, is_reversed} to neighbour count, using a fast tuple hash.
 *
 * is_reversed=false: out-edge direction (or undirected).
 * is_reversed=true:  in-edge direction (directed graphs only).
 */
using CountsMap = std::unordered_map<std::tuple<uint32_t, uint32_t, bool>, uint32_t, TupleHash>;

}  // namespace sgf
