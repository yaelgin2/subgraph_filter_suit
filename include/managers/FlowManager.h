#pragma once

#include "ColoredGraph.h"
#include "ICacheIOManager.h"
#include "IColoredGraphReader.h"
#include "IPatternWriter.h"

#include <memory>
#include <string>
#include <vector>

namespace sgf
{

/**
 * @brief Data loaded from a graph library directory.
 */
struct LibraryData
{
    std::vector<std::string> m_graph_names;
    std::vector<ColoredGraph> m_library;
};

/**
 * @brief Selects the file format used for reading graph files.
 */
enum class GraphReaderType
{
    GRAPHML,     ///< GraphML (.graphml) — GraphmlGraphReader
    JSON,        ///< JSON (.json) — JsonGraphReader
    VERTEX_EDGE, ///< Paired .vertex_indices/.edges files — VertexEdgeGraphReader
};

/**
 * @brief Selects the file format used for writing pattern graphs.
 */
enum class PatternWriterType
{
    GRAPHML,     ///< GraphML (.graphml) — GraphmlPatternWriter
    JSON,        ///< JSON (.json) — JsonPatternWriter
    VERTEX_EDGE, ///< Paired vertex/edge files — VertexEdgePatternWriter
};

/**
 * @brief Selects the file format used for the enumeration cache.
 */
enum class CacheManagerType
{
    BINARY, ///< Binary format — BinaryCacheIOManager
    CSV,    ///< CSV format — CSVCacheIOManager
};

/**
 * @brief Top-level pipeline orchestrator and IO factory.
 *
 * Factory methods create the concrete reader/writer/cache instance
 * corresponding to each type enum, keeping all #include dependencies
 * on concrete IO classes confined to FlowManager.cpp.
 */
class FlowManager
{
public:
    /**
     * @brief Construct a graph reader for the given format.
     * @param type Desired reader format.
     * @return Owning pointer to the concrete IColoredGraphReader.
     */
    static std::unique_ptr<IColoredGraphReader> make_graph_reader(GraphReaderType type);

    /**
     * @brief Construct a pattern writer for the given format.
     * @param type Desired writer format.
     * @return Owning pointer to the concrete IPatternWriter.
     */
    static std::unique_ptr<IPatternWriter> make_pattern_writer(PatternWriterType type);

    /**
     * @brief Construct a cache manager for the given format.
     * @param type           Desired cache format.
     * @param folder         Directory where the cache file will be written.
     * @param base_filename  File name without extension.
     * @return Owning pointer to the concrete ICacheIOManager.
     */
    static std::unique_ptr<ICacheIOManager> make_cache_manager(CacheManagerType type,
                                                               const std::string& folder);

    /**
     * @brief Run the enumeration preprocessing pipeline.
     * @param input_folder  Directory containing the graph library.
     * @param reader_type   Format of the graph files.
     * @param output_folder Directory to write the enumeration cache.
     * @param cache_type    Format of the enumeration cache.
     */
    static void enumerator_preprocess_run(const std::string& input_path, const bool is_directed, 
        const GraphReaderType reader_type, std::string& output_path, 
        CacheManagerType output_type, std::string log_file_path,
        bool preprocess_paths, bool preprocess_motifs);

    /// @brief Run the enumeration filter stage.
    static void enumerator_filter_run();

    /// @brief Run the pattern preprocessing stage.
    static void pattern_preprocess_run();

    /// @brief Run the pattern filter stage.
    static void pattern_filter_run();

    /// @brief Run the exact subgraph isomorphism stage.
    static void subgraph_isomorphism_run();

private:
    static constexpr const char* PATH_CACHE_BASE_NAME = "path_cache";
    static constexpr const char* MOTIF_CACHE_BASE_NAME = "motif_cache";

    /**
     * @brief Returns the current local time as a filename-safe string.
     *
     * Format: YYYY-MM-DD_HH-MM-SS
     *
     * @return Timestamp string.
     */
    static std::string generate_timestamp();

    /**
     * @brief Load all graphs from @p path using @p reader_type.
     * @param path        Directory containing the graph files.
     * @param reader_type Format of the graph files.
     * @return Loaded library data.
     */
    static LibraryData load_library(const std::string& path, const GraphReaderType reader_type,
        const bool is_directed, LoggerHandler logger);
};

}  // namespace sgf
