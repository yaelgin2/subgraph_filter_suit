#pragma once

#include "ColoredGraph.h"
#include "ICacheIOManager.h"
#include "IColoredGraphReader.h"
#include "IFilterIOManager.h"
#include "IPatternWriter.h"
#include "PriorPolicy.h"
#include "SingleGraphPatternPreprocessor.h"

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
    GRAPHML,      ///< GraphML (.graphml) — GraphmlGraphReader
    JSON,         ///< JSON (.json) — JsonGraphReader
    VERTEX_EDGE,  ///< Paired .vertex_indices/.edges files — VertexEdgeGraphReader
};

/**
 * @brief Selects the file format used for writing pattern graphs.
 */
enum class PatternWriterType
{
    GRAPHML,      ///< GraphML (.graphml) — GraphmlPatternWriter
    JSON,         ///< JSON (.json) — JsonPatternWriter
    VERTEX_EDGE,  ///< Paired vertex/edge files — VertexEdgePatternWriter
};

/**
 * @brief Selects the file format used for the enumeration cache.
 */
enum class CacheManagerType
{
    BINARY,  ///< Binary format — BinaryCacheIOManager
    CSV,     ///< CSV format — CSVCacheIOManager
};

/**
 * @brief Selects the file format used for the result output.
 */
enum class ResultOutputType
{
    JSON,  ///< Binary format — BinaryCacheIOManager
    CSV,   ///< CSV format — CSVCacheIOManager
};

/**
 * @brief Options controlling query graph enumeration caching in the filter stage.
 */
struct GraphEnumerationCacheConfig
{
    bool m_cache_enumeration{false};  ///< Write computed enumeration to cache after computing.
    std::string
        m_graph_cache_dir;  ///< Directory for cache (required when m_cache_enumeration is true).
    std::string
        m_graphs_motif_cache_path;  ///< Full path to existing motif cache; empty = compute fresh.
    std::string
        m_graphs_path_cache_path;  ///< Full path to existing path cache; empty = compute fresh.
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
     * @brief Run the enumeration preprocessing pipeline.
     * @param input_path        Directory containing the graph library.
     * @param is_directed       Treat graphs as directed.
     * @param reader_type       Format of the graph files.
     * @param output_path       Directory where enumeration caches are written.
     * @param output_type       Format of the enumeration cache.
     * @param log_file_path     Optional log file path.
     * @param preprocess_paths  Preprocess path signatures.
     * @param preprocess_motifs Preprocess motif signatures.
     */
    static std::vector<EnumerationResultVector>
    enumerator_preprocess_run(const std::string& input_path, bool is_directed,
                              GraphReaderType reader_type, std::string& output_path,
                              CacheManagerType output_type, const std::string& log_file_path,
                              bool preprocess_paths, bool preprocess_motifs);

    /**
     * @brief Run the enumeration filter stage.
     * @param graph_input_path   Directory containing query graphs.
     * @param is_directed        Treat graphs as directed.
     * @param reader_type        Format of the graph files.
     * @param motif_cache_file   Full path to the motif cache file to read.
     * @param path_cache_file    Full path to the path cache file to read.
     * @param cache_reader_type  Format of the cache files.
     * @param output_folder      Directory for filter result output.
     * @param output_type        Format of the filter results.
     * @param log_file_path      Optional log file path.
     * @param filter_paths       Filter by path signatures.
     * @param filter_motifs      Filter by motif signatures.
     */
    static std::vector<std::unordered_map<std::string, FilterResult>>
    enumerator_filter_run(const std::string& graph_input_path, bool is_directed,
                          GraphReaderType reader_type, const std::string& motif_cache_file,
                          const std::string& path_cache_file, CacheManagerType cache_reader_type,
                          std::string& output_folder, ResultOutputType output_type,
                          const std::string& log_file_path, bool filter_paths, bool filter_motifs,
                          const GraphEnumerationCacheConfig& graph_cache_config);

    /// @brief Run the pattern preprocessing stage.
    static void pattern_preprocess_run(
        const std::string& input_path, bool is_directed, GraphReaderType reader_type,
        std::string& output_path, PatternWriterType output_type, const std::string& log_file_path,
        uint32_t preprocess_multigraph, uint32_t multigraph_alive_percent,
        bool preprocess_singlegraph_results_file, const std::string& results_file_path,
        int64_t preprocess_singlegraph, ResultOutputType results_file_type,
        const std::string& background_graph_path, double score_threshold,
        const SingleGraphFinderConfig& config);

    /// @brief Run the pattern filter stage.
    static void pattern_filter_run();

    /// @brief Run the exact subgraph isomorphism stage.
    static uint64_t subgraph_isomorphism_run(const std::string& subgraph_path,
                                             const std::string& background_graph_path,
                                             GraphReaderType reader_type, bool is_output,
                                             std::string& output_path, bool is_directed,
                                             bool is_induced, PriorPolicy policy,
                                             bool stop_on_first_match,
                                             const std::string& log_file_path);

private:
    static constexpr const char* PATH_CACHE_BASE_NAME = "path_cache";
    static constexpr const char* MOTIF_CACHE_BASE_NAME = "motif_cache";

    /**
     * @brief Loads query graph enumeration from a cache file in the order
     *        returned by the cache, paired with the graph names.
     *
     * @param cache_manager  Cache manager pointing at the cache directory.
     * @param cache_path     Base filename (stem) of the cache file to read.
     * @return Pair of (graph names, enumerations) in cache-file order.
     */
    static std::pair<std::vector<std::string>, EnumerationResultVector>
    load_graph_enumeration(CacheManagerType manager_type, const std::string& cache_path,
                           LoggerHandler logger);

    /**
     * @brief Computes or loads the query graph enumeration for the filter stage.
     *
     * If @p load_from_cache is true, delegates to load_graph_enumeration.
     * Otherwise computes via @p preprocess_manager; when @p write_to_cache is
     * true the result is also written through @p cache_manager.
     *
     * @param write_to_cache     Persist computed enumeration to cache.
     * @param cache_base_name    Stem used for the cache file.
     * @param cache_manager      Cache manager (required when loading or writing).
     * @param preprocess_manager Used for fresh computation.
     * @param library            Provides graph names and library graphs.
     * @param factory            Creates the feature-specific preprocessor.
     * @param timestamp          Appended to cache_base_name when writing.
     * @param logger             Logger for diagnostics.
     * @return Pair of (graph names, enumerations).
     */
    static EnumerationResultVector
    get_graph_enumeration(bool write_to_cache, const std::string& cache_base_name,
                          const std::shared_ptr<ICacheIOManager>& cache_manager,
                          EnumerationPreprocessManager& preprocess_manager,
                          const LibraryData& library, const PreprocessorFactory& factory,
                          const std::string& timestamp);

    static std::unordered_map<std::string, FilterResult> run_enumeration_filter_stage(
        const std::string& result_file_base_name, const EnumerationResultVector& graphs_enumeration,
        const ICacheIOManager& lib_cache_manager, const std::string& lib_cache_path,
        IFilterIOManager& filter_results_writer, const LibraryData& library,
        const std::string& timestamp, const LoggerHandler& logger);

    /**
     * @brief Returns the current local time as a filename-safe string.
     *
     * Format: YYYY-MM-DD_HH-MM-SS
     *
     * @return Timestamp string.
     */
    static std::string generate_timestamp();

    /**
     * @brief Converts a filter map (filename → pruned) to a to_process vector indexed by library.
     *
     * @param graph_names  Ordered list of graph names from the library.
     * @param filter_map   Map from graph filename stem to pruned flag (true = pruned).
     * @return Vector of bools (true = should process = not pruned), in graph_names order.
     */
    static std::vector<bool>
    build_to_process(const std::vector<std::string>& graph_names,
                     const std::unordered_map<std::string, bool>& filter_map);

    /**
     * @brief Load all graphs from @p path using @p reader_type.
     * @param path        Directory containing the graph files.
     * @param reader_type Format of the graph files.
     * @return Loaded library data.
     */
    static LibraryData load_library(const std::string& path, GraphReaderType reader_type,
                                    bool is_directed, const LoggerHandler& logger);

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
    static std::shared_ptr<IPatternWriter> make_pattern_writer(PatternWriterType type);

    /**
     * @brief Construct a cache manager for the given format.
     * @param type    Desired cache format.
     * @param folder  Directory where the cache file will be written.
     * @param logger  Logger for read/write diagnostics.
     * @return Owning pointer to the concrete ICacheIOManager.
     */
    static std::shared_ptr<ICacheIOManager>
    make_cache_manager(CacheManagerType type, const std::string& folder, LoggerHandler logger);

    /**
     * @brief Construct a filter results writer for the given format.
     * @param type    Desired output format.
     * @param folder  Directory where the output file will be written.
     * @param logger  Logger for write diagnostics.
     * @return Owning pointer to the concrete IFilterIOManager.
     */
    static std::unique_ptr<IFilterIOManager>
    make_filter_results_io_manager(ResultOutputType type, const std::string& folder,
                                   LoggerHandler logger);

    /**
     * @brief Loads or computes graph enumeration for one stage, then runs the filter.
     *
     * @param library_cache_file   Full path to the library enumeration cache.
     * @param load_graph_cache     When true, loads query graph enumeration from cache.
     * @param graphs_cache_path    Path to the query graph cache (used when load_graph_cache is
     * true).
     * @param cache_base_name      Cache stem used when computing (e.g. "path_cache").
     * @param factory              Creates the stage-specific preprocessor.
     * @param cache_reader_type    Cache file format.
     * @param graphs_to_find_in    Query graph data; m_graph_names may be updated from cache.
     * @param preprocess_manager   Used to compute enumeration; must be non-null when
     * load_graph_cache is false.
     * @param filter_results_writer Output manager for filter results.
     * @param timestamp            Timestamp string used in output file names.
     * @param logger               Logger for diagnostics.
     */
    static std::unordered_map<std::string, FilterResult>
    enumerate_and_filter(const std::string& library_cache_file, bool load_graph_cache,
                         const std::string& graphs_cache_path,
                         const std::string& run_type_file_base_name,
                         const PreprocessorFactory& factory, CacheManagerType cache_reader_type,
                         const std::shared_ptr<ICacheIOManager>& graphs_cache_manager,
                         LibraryData& graphs_to_find_in,
                         const std::unique_ptr<EnumerationPreprocessManager>& preprocess_manager,
                         IFilterIOManager& filter_results_writer, const std::string& timestamp,
                         const LoggerHandler& logger);
};

}  // namespace sgf
