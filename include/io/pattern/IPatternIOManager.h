#pragma once

#include "BoostGraph.h"
#include "ColoredGraph.h"

#include <boost/graph/adjacency_list.hpp>
#include <cstdint>
#include <string>

namespace sgf
{

/**
 * @brief Interface for reading and writing pattern graphs.
 *
 * Implementors handle format-specific serialization and deserialization.
 * write() persists a BoostGraph to disk; read() loads a ColoredGraph from disk.
 * All I/O errors must be wrapped as SgfException subclasses — no raw standard
 * exceptions may propagate to the caller.
 */
class IPatternIOManager
{
public:
    /**
     * @brief Writes a pattern graph to a file.
     *
     * @param graph The pattern graph to serialize.
     * @param path Destination file path.
     * @throws SgfPathDoesntExistException if the path cannot be written.
     */
    virtual void write(const BoostGraph& graph, const std::string& path) const = 0;

    /**
     * @brief Reads a pattern graph from a file into a ColoredGraph.
     *
     * @param path Path to the file.
     * @return The parsed ColoredGraph.
     * @throws SgfPathDoesntExistException if the file cannot be opened.
     * @throws GraphConstructionException if the file is malformed or the
     *         graph structure is invalid.
     */
    virtual ColoredGraph read(const std::string& path) const = 0;

    /**
     * @brief Default virtual destructor.
     */
    virtual ~IPatternIOManager() = default;
};

}  // namespace sgf
