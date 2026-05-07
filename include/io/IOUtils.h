#pragma once

#include <exception>
#include <fstream>
#include <string>

namespace sgf
{
namespace graphml_io_utils
{

/**
 * @brief Opens a file for reading.
 * @param path Path to the file.
 * @return An open input stream.
 * @throws SgfPathDoesntExistException if the file cannot be opened.
 */
std::ifstream open_file(const std::string& path);

/**
 * @brief Opens a file for writing.
 * @param path Destination file path.
 * @return An open output stream.
 * @throws SgfPathDoesntExistException if the file cannot be opened.
 */
std::ofstream open_output_file(const std::string& path);

/**
 * @brief Detects whether a GraphML file declares directed edges.
 *
 * Scans the file for the @c edgedefault attribute. If absent, defaults to directed.
 *
 * @param path Path to the GraphML file.
 * @return True if the graph is directed or no @c edgedefault was found.
 * @throws SgfPathDoesntExistException if the file cannot be opened.
 */
bool detect_is_directed(const std::string& path);

/**
 * @brief Wraps @p exc in a GraphConstructionException and throws it.
 * @param path The file path associated with the failure.
 * @param exc The original exception.
 */
[[noreturn]] void rethrow_as_construction_error(const std::string& path,
                                                const std::exception& exc);

}  // namespace graphml_io_utils
}  // namespace sgf
