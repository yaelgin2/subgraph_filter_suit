#pragma once

#include "BoostGraph.h"
#include "ColoredGraph.h"
#include "LoggerHandler.h"

#include <boost/graph/adjacency_list.hpp>
#include <cstdint>
#include <string>

namespace sgf
{

/**
 * @brief Interface for writing pattern graphs.
 *
 * The base class owns directory creation so that concrete implementations only
 * handle format-specific serialization. write() creates the parent directory
 * using IOUtils::create_directory_if_needed then delegates to do_write().
 */
class IPatternWriter
{
public:
    IPatternWriter() = default;

    /**
     * @brief Default virtual destructor.
     */
    virtual ~IPatternWriter() = default;

    IPatternWriter(const IPatternWriter&) = default;
    IPatternWriter& operator=(const IPatternWriter&) = default;
    IPatternWriter(IPatternWriter&&) = default;
    IPatternWriter& operator=(IPatternWriter&&) = default;

    /**
     * @brief Creates the parent directory if absent, then writes @p graph to @p path.
     *
     * Directory creation is delegated to IOUtils::create_directory_if_needed;
     * format-specific serialization is delegated to do_write().
     *
     * @param graph  The pattern graph to serialize.
     * @param path   Destination file path.
     * @param logger Optional logger for diagnostics.
     * @throws SgfPathExistsException if directory creation or file writing fails.
     */
    void write(const BoostGraph& graph, const std::string& path,
               const LoggerHandler& logger = LoggerHandler::null()) const;

    /**
     * @brief Returns the file extension this writer appends, without a leading dot.
     *
     * For single-file formats (GraphML, JSON) this is the format extension
     * (e.g. "graphml", "json"). For multi-file formats that append their own
     * suffixes to the base path (e.g. VertexEdge), this returns an empty string.
     *
     * @return Extension string, or empty if the writer manages its own suffixes.
     */
    [[nodiscard]] virtual std::string get_file_extension() const = 0;

private:
    /**
     * @brief Writes @p graph to @p path in the concrete format.
     *
     * Called by write() after the parent directory has been created.
     *
     * @param graph The pattern graph to serialize.
     * @param path Destination file path.
     * @throws SgfPathExistsException if the file cannot be opened or written.
     */
    virtual void do_write(const BoostGraph& graph, const std::string& path) const = 0;
};

}  // namespace sgf
