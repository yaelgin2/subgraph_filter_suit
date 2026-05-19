#pragma once

#include "ColoredGraph.h"
#include "LoggerHandler.h"

#include <memory>
#include <string>

namespace sgf
{

class ILogger;

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
     * @param is_directed Whether the graph should be treated as directed.
     * @param logger Optional logger for diagnostics. Pass nullptr to suppress logging.
     * @return The parsed ColoredGraph.
     * @throws GraphConstructionException if the file cannot be opened, parsed,
     *         or if the graph structure is invalid.
     */
    virtual ColoredGraph read(const std::string& path, bool is_directed,
                              const LoggerHandler& logger) const = 0;

    /**
     * @brief Default virtual destructor.
     */
    virtual ~IColoredGraphReader() = default;

    IColoredGraphReader() = default;
    IColoredGraphReader(const IColoredGraphReader&) = delete;
    IColoredGraphReader& operator=(const IColoredGraphReader&) = delete;
    IColoredGraphReader(IColoredGraphReader&&) = delete;
    IColoredGraphReader& operator=(IColoredGraphReader&&) = delete;
};

}  // namespace sgf
