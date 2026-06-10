#pragma once

#include "FilteringUtils.h"
#include "FlowManager.h"
#include "IPatternPreprocessor.h"
#include "PriorPolicy.h"
#include "SingleGraphPatternPreprocessor.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace sgf
{

// ── enumerate_library ─────────────────────────────────────────────────────────

/**
 * @brief Parameters for enumerate_library().
 *
 * Required fields must be non-empty/valid; optional fields default to off/empty.
 * At least one of @c m_preprocess_motifs or @c m_preprocess_paths must be true.
 */
struct EnumerateLibraryParams
{
    std::string m_library_path;             ///< [required] Directory containing the graph library.
    GraphReaderType m_reader_type;          ///< [required] Graph file format.
    std::string m_output_path;              ///< [required] Directory where caches are written.
    CacheManagerType m_cache_type;          ///< [required] Cache file format.
    bool m_preprocess_motifs{false};        ///< Enumerate motif signatures.
    bool m_preprocess_paths{false};         ///< Enumerate path signatures.
    bool m_is_directed{false};              ///< Treat graphs as directed.
    std::optional<std::string> m_log_file;  ///< Log file path; absent = no logging.
};

/**
 * @brief Preprocess a graph library into motif/path enumeration caches.
 *
 * Validates all required fields in @p params. At least one of
 * @c m_preprocess_motifs or @c m_preprocess_paths must be true.
 *
 * @param params Preprocessing options.
 * @return Enumeration result vectors, one per enabled feature.
 * @throws InvalidArgumentException if required fields are empty or both feature flags are false.
 */
std::vector<EnumerationResultVector> enumerate_library(const EnumerateLibraryParams& params);

// ── filter_with_enumeration ───────────────────────────────────────────────────

/**
 * @brief Parameters for filter_with_enumeration().
 *
 * @c m_motif_cache_file is required when @c m_filter_motifs is true.
 * @c m_path_cache_file  is required when @c m_filter_paths  is true.
 */
struct FilterWithEnumerationParams
{
    std::string m_query_graph_path;  ///< [required] Directory containing query graphs.
    GraphReaderType m_reader_type;   ///< [required] Query graph file format.
    std::string m_output_folder;     ///< [required] Directory for filter result output.
    ResultOutputType m_result_type;  ///< [required] Filter result file format.
    CacheManagerType m_cache_type;   ///< [required] Cache file format.
    bool m_filter_motifs{false};     ///< Filter by motif signatures.
    bool m_filter_paths{false};      ///< Filter by path signatures.
    std::optional<std::string> m_motif_cache_file;  ///< Required when m_filter_motifs is true.
    std::optional<std::string> m_path_cache_file;   ///< Required when m_filter_paths is true.
    bool m_is_directed{false};                      ///< Treat graphs as directed.
    bool m_non_induced{false};  ///< Expand motif counts via inclusion DAG (non-induced).
    std::optional<std::string> m_log_file;         ///< Log file path; absent = no logging.
    GraphEnumerationCacheConfig m_cache_config{};  ///< Query graph enumeration caching options.
};

/**
 * @brief Filter query graphs against library enumeration caches.
 *
 * At least one of @c m_filter_motifs or @c m_filter_paths must be true.
 * The corresponding cache file must be provided for each enabled feature.
 *
 * @param params Filter options.
 * @return Per-feature filter results: map from library graph name to filter flags.
 * @throws InvalidArgumentException if required fields are empty, no feature is enabled,
 *         or a required cache file is absent.
 */
std::vector<std::unordered_map<std::string, FilterResult>>
filter_with_enumeration(const FilterWithEnumerationParams& params);

// ── preprocess_patterns ───────────────────────────────────────────────────────

/**
 * @brief Parameters for preprocess_patterns().
 *
 * Single-graph mode is activated when @c m_single_graph_index is set OR when
 * @c m_results_file_path is set. In both cases @c m_background_graph_path is required.
 * The two single-graph activation modes are mutually exclusive.
 */
struct PreprocessPatternsParams
{
    std::string m_library_path;        ///< [required] Directory containing the graph library.
    GraphReaderType m_reader_type;     ///< [required] Graph file format.
    std::string m_output_path;         ///< [required] Directory where pattern files are written.
    PatternWriterType m_pattern_type;  ///< [required] Pattern file format.
    bool m_is_directed{false};         ///< Treat graphs as directed.
    std::optional<int64_t> m_single_graph_index;     ///< Process one graph by library index.
    std::optional<std::string> m_results_file_path;  ///< Derive graph index from results file.
    ResultOutputType m_results_file_type{ResultOutputType::JSON};  ///< Results file format.
    std::optional<std::string>
        m_background_graph_path;    ///< Background graph (required in single-graph mode).
    double m_score_threshold{0.0};              ///< Pattern score cutoff.
    SingleGraphFinderConfig m_finder_config{};  ///< Beam search tuning parameters.
    std::optional<std::string> m_log_file;      ///< Log file path; absent = no logging.
    uint32_t m_preprocess_multigraph{0U};       ///< Number of multigraph patterns; 0 = disabled.
    double m_multigraph_alive_percent{0.5};     ///< Fraction of library graphs pattern must appear in.
};

/**
 * @brief Extract pattern subgraphs from a graph library.
 *
 * When @c m_single_graph_index or @c m_results_file_path is set (single-graph mode),
 * @c m_background_graph_path is required and the two activation modes are mutually exclusive.
 *
 * @param params Pattern preprocessing options.
 * @return Extracted pattern results.
 * @throws InvalidArgumentException if required fields are empty, both single-graph modes
 *         are set simultaneously, or single-graph mode is active without a background graph.
 */
std::vector<PatternPreprocessorResult> preprocess_patterns(const PreprocessPatternsParams& params);

// ── filter_with_patterns ──────────────────────────────────────────────────────

/**
 * @brief Parameters for filter_with_patterns().
 */
struct FilterWithPatternsParams
{
    std::string m_pattern_cache_path;       ///< [required] Full path to the pattern cache.
    PatternWriterType m_pattern_type;       ///< [required] Pattern cache file format.
    std::string m_background_graph_folder;  ///< [required] Background graph directory.
    GraphReaderType m_reader_type;          ///< [required] Graph file format.
    std::string m_output_path;              ///< [required] Directory for filter result output.
    ResultOutputType m_result_type;         ///< [required] Filter result file format.
    PriorPolicy m_prior_policy;             ///< [required] Vertex ordering heuristic.
    bool m_is_directed{false};              ///< Treat graphs as directed.
    bool m_is_induced{false};               ///< Search for induced subgraph matches.
    std::optional<std::string> m_log_file;  ///< Log file path; absent = no logging.
};

/**
 * @brief Filter background graphs against a pre-built pattern cache.
 *
 * @param params Filter options.
 * @return Per-background-graph filter results: map from graph name to filter flags.
 * @throws InvalidArgumentException if any required string field is empty.
 */
std::vector<std::unordered_map<std::string, FilterResult>>
filter_with_patterns(const FilterWithPatternsParams& params);

// ── find_subgraph ─────────────────────────────────────────────────────────────

/**
 * @brief Parameters for find_subgraph().
 */
struct FindSubgraphParams
{
    std::string m_subgraph_path;               ///< [required] Path to the query subgraph file.
    std::string m_background_path;             ///< [required] Path to the background graph file.
    GraphReaderType m_reader_type;             ///< [required] Graph file format.
    PriorPolicy m_prior_policy;                ///< [required] Vertex ordering heuristic.
    bool m_is_directed{false};                 ///< Treat graphs as directed.
    bool m_is_induced{false};                  ///< Search for induced subgraph matches.
    bool m_stop_on_first_match{false};         ///< Stop after the first match is found.
    std::optional<std::string> m_output_path;  ///< Write match mappings here; absent = no output.
    std::optional<std::string> m_log_file;     ///< Log file path; absent = no logging.
};

/**
 * @brief Run exact subgraph isomorphism between a query subgraph and a background graph.
 *
 * **Note:** Only **connected** subgraphs are supported. If the query subgraph contains
 * two or more disconnected components, the result will be 0 regardless of the background graph.
 *
 * @param params Search options.
 * @return Number of matching subgraph instances found in the background graph.
 * @throws InvalidArgumentException if @c m_subgraph_path, @c m_background_path,
 *         or @c m_reader_type are unset/empty.
 */
uint64_t find_subgraph(const FindSubgraphParams& params);

}  // namespace sgf
