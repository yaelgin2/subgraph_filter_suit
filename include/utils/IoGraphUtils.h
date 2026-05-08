#pragma once

#include "GraphConstructionException.h"
#include "SgfPathDoesntExistException.h"

#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_map>

namespace sgf
{

/**
 * @brief Shared I/O utilities for graph file readers.
 */
class IoGraphUtils
{
public:
    /**
     * @brief Opens a file for reading, throwing if it cannot be opened.
     * @param path Path to the file.
     * @return An open input stream.
     * @throws SgfPathDoesntExistException if the file cannot be opened.
     */
    static std::ifstream open_file(const std::string& path);

    /**
     * @brief Builds a deterministic remapping from original IDs to consecutive indices.
     *
     * Sorts keys ascending before assigning consecutive indices so the mapping
     * is reproducible regardless of unordered_map iteration order.
     *
     * @param color_by_id Map from original node ID to color value.
     * @return Map from original ID to consecutive zero-based index.
     * @throws GraphConstructionException if vertex count exceeds uint32_t maximum.
     */
    static std::unordered_map<uint32_t, uint32_t>
    build_consecutive_index_map(const std::unordered_map<uint32_t, uint32_t>& color_by_id);
};

}  // namespace sgf
