#pragma once

#include "BoostGraph.h"
#include "ColoredGraph.h"

#include <boost/graph/adjacency_list.hpp>
#include <cstdint>
#include <string>

namespace sgf
{

/**
 * @brief Interface for writing pattern graphs.
 *
 * Implementors handle format-specific serialization of a BoostGraph to disk.
 * All I/O errors must be wrapped as SgfException subclasses — no raw standard
 * exceptions may propagate to the caller.
 */
class IPatternWriter
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

    IPatternWriter() = default;

    /**
     * @brief Default virtual destructor.
     */
    virtual ~IPatternWriter() = default;


    IPatternWriter(const IPatternWriter&) = default;
    IPatternWriter& operator=(const IPatternWriter&) = default;
    IPatternWriter(IPatternWriter&&) = default;
    IPatternWriter& operator=(IPatternWriter&&) = default;
};

}  // namespace sgf
