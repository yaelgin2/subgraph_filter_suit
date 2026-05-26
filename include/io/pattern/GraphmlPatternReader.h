#pragma once

#include "BoostGraph.h"
#include "IOConstants.h"
#include "IPatternReader.h"
#include "LoggerHandler.h"

#include <string>

namespace sgf
{

/**
 * @brief Reads pattern graphs from GraphML files.
 *
 * Parses via an intermediate IOConstants::GraphmlDirectedBoostGraph with
 * string-typed color properties, then converts to a BoostGraph with uint32_t colors.
 */
class GraphmlPatternReader : public IPatternReader
{
public:
    /**
     * @brief Default constructor.
     */
    GraphmlPatternReader() = default;

    /**
     * @brief Reads a pattern graph from a GraphML file at @p path.
     *
     * @param path   Source file path.
     * @param logger Optional logger for diagnostics.
     * @return The parsed BoostGraph.
     * @throws GraphConstructionException if the file cannot be opened or parsed.
     */
    BoostGraph read(const std::string& path, const LoggerHandler& logger) const override;

private:
    /**
     * @brief Reads the GraphML file into a string-color intermediate graph.
     *
     * @param path Source file path.
     * @return Intermediate Boost graph with string color properties.
     * @throws GraphConstructionException if the file cannot be opened or parsed.
     */
    static IOConstants::GraphmlDirectedBoostGraph read_into_string_graph(const std::string& path);

    /**
     * @brief Converts a string-color intermediate graph to a BoostGraph.
     *
     * @param string_graph Intermediate graph with string color properties.
     * @return BoostGraph with uint32_t colors.
     * @throws GraphConstructionException if any color string is not a valid integer.
     */
    static BoostGraph
    convert_to_boost_graph(const IOConstants::GraphmlDirectedBoostGraph& string_graph);
};

}  // namespace sgf
