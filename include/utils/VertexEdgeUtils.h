#pragma once

#include "GraphConstructionException.h"
#include "SgfPathDoesntExistException.h"

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>

namespace sgf
{

/**
 * @brief Shared utilities and format constants for vertex-edge file I/O.
 *
 * Both the graph reader and the pattern writer use the vertex-edge plain-text
 * format, which encodes a vertex as "<id> <color>" and an edge as
 * "<src> <dst> [color]".  This class centralises the token-count constants
 * that describe that format and two helpers used by both components.
 */
class VertexEdgeUtils
{
public:
    /**
     * @brief Throws if @p stream still contains tokens after the expected ones
     *        have been consumed.
     *
     * @param stream         Stream positioned immediately after the expected tokens.
     * @param context        Human-readable context prefix for the error message.
     * @param line           Raw source line included verbatim in the error message.
     * @param expected_count Number of tokens that were expected on the line.
     * @throws GraphConstructionException if any additional token can be read.
     */
    static void throw_if_extra_tokens(std::istringstream& stream, const std::string& context,
                                      const std::string& line, uint32_t expected_count);

    /**
     * @brief Opens a file for writing, throwing if it cannot be opened.
     *
     * @param path Path of the file to create or overwrite.
     * @return An open output stream.
     * @throws SgfPathDoesntExistException if the file cannot be opened.
     */
    static std::ofstream open_file_for_writing(const std::string& path);
};

}  // namespace sgf
