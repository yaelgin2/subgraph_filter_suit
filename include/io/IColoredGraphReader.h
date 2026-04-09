#pragma once

#include "ColoredGraph.h"

#include <string>

namespace sgf
{

/**
 * @brief Interface for reading a ColoredGraph from a file.
 *
 * Implementors must map file-format-specific graph representations into a
 * ColoredGraph and wrap all internal errors as GraphConstructionException.
 */
class IColoredGraphReader
{
public:
    /**
     * @brief Reads a graph from the given file path.
     *
     * @param path Path to the file to read.
     * @return The parsed ColoredGraph.
     * @throws GraphConstructionException if the file cannot be opened, parsed,
     *         or if the graph structure is invalid.
     */
    virtual ColoredGraph read(const std::string& path) const = 0;

    /**
     * @brief Default virtual destructor.
     */
    virtual ~IColoredGraphReader() = default;
};

}  // namespace sgf
