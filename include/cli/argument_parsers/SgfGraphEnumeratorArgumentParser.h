#pragma once

#include "Constants.h"
#include "FlowManager.h"

// NOLINTNEXTLINE(misc-include-cleaner)
#include <boost/program_options.hpp>
#include <cstdint>
#include <optional>
#include <string>

namespace sgf
{

/**
 * @brief Arguments consumed by the preprocessing stage.
 */
struct PreprocessArgs
{
    std::string m_library_dir;                                ///< Graph library directory.
    GraphReaderType m_reader_type{GraphReaderType::GRAPHML};  ///< Graph file format.
    std::string m_cache_dir;                                  ///< Cache library output directory.
    CacheManagerType m_cache_type{CacheManagerType::BINARY};  ///< Cache format.
    std::string m_log_file_path;                              ///< Optional log file path.
    std::string m_graphml_color_map_path;  ///< Path to initial GraphML color map CSV; empty = none.
};

/**
 * @brief Arguments consumed by the filter stage.
 */
struct FilterArgs
{
    std::string m_motif_cache_file;                                ///< Motif cache file path.
    std::string m_path_cache_file;                                 ///< Path cache file path.
    CacheManagerType m_cache_type{CacheManagerType::BINARY};       ///< Cache format.
    std::string m_graph_dir;                                       ///< Query graphs directory.
    GraphReaderType m_graph_input_type{GraphReaderType::GRAPHML};  ///< Graph file format.
    std::string m_result_folder;                                   ///< Results output directory.
    ResultOutputType m_result_type{ResultOutputType::JSON};        ///< Results format.
    std::string m_log_file_path;                                   ///< Optional log file path.
    bool m_cache_graph_enumeration{false};      ///< Cache query graph enumeration after computing.
    std::string m_graph_cache_dir;              ///< Directory for enumeration cache.
    std::string m_load_motif_graph_cache_path;  ///< Existing motif cache path; empty = compute.
    std::string m_load_path_graph_cache_path;   ///< Existing path cache path; empty = compute.
    std::string m_graphml_color_map_path;  ///< Path to initial GraphML color map CSV; empty = none.
};

/**
 * @brief All parsed CLI arguments.
 */
struct CliArgs
{
    bool m_run_preprocess{false};  ///< Run the preprocessing stage.
    bool m_run_filter{false};      ///< Run the filter stage.
    bool m_use_motifs{false};      ///< Enable motif-based processing.
    bool m_use_paths{false};       ///< Enable path-based processing.
    bool m_is_directed{false};     ///< Treat graphs as directed.
    bool m_non_induced{false};     ///< Expand induced motif counts via DAG before filtering.
    uint32_t m_thread_number{
        SgfConstants::DEFAULT_THREAD_NUMBER};  ///< Maximum number of threads for preprocessing.
    PreprocessArgs m_preprocess{};             ///< Preprocessing-stage arguments.
    FilterArgs m_filter{};                     ///< Filter-stage arguments.
};

/**
 * @brief Parses and validates command-line arguments for sgf-graph-enumerator.
 *
 * All argument parsing, validation, and help-display logic is encapsulated here.
 * The single public entry point is parse(), which returns std::nullopt when the
 * caller should exit with success (i.e. help was displayed).
 */
class SgfGraphEnumeratorArgumentParser
{
public:
    /**
     * @brief Parse and validate command-line arguments.
     *
     * Prints help to stdout and returns std::nullopt when --help is given or
     * no arguments are provided.
     *
     * @param argc Argument count from main.
     * @param argv Argument values from main.
     * @return Validated CliArgs, or std::nullopt if help was displayed.
     * @throws SgfInvalidArgumentException on invalid or missing arguments.
     */
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
    static std::optional<CliArgs> parse(int argc, char* argv[]);

private:
    static constexpr const char* KEY_HELP = "help";
    static constexpr const char* KEY_PREPROCESS = "preprocess";
    static constexpr const char* KEY_FILTER = "filter";
    static constexpr const char* KEY_MOTIFS = "motifs";
    static constexpr const char* KEY_PATHS = "paths";
    static constexpr const char* KEY_IS_DIRECTED = "is-directed";
    static constexpr const char* KEY_NON_INDUCED = "non-induced";
    static constexpr const char* KEY_THREAD_NUMBER = "thread-number";
    static constexpr const char* KEY_LIBRARY_DIR = "library-dir";
    static constexpr const char* KEY_READER_TYPE = "reader-type";
    static constexpr const char* KEY_CACHE_DIR = "cache-dir";
    static constexpr const char* KEY_MOTIF_CACHE_FILE = "motif-cache-file";
    static constexpr const char* KEY_PATH_CACHE_FILE = "path-cache-file";
    static constexpr const char* KEY_CACHE_TYPE = "cache-type";
    static constexpr const char* KEY_LOG_FILE_PATH = "log-file-path";
    static constexpr const char* KEY_GRAPH_DIR = "graph-dir";
    static constexpr const char* KEY_GRAPH_INPUT_TYPE = "graph-input-type";
    static constexpr const char* KEY_RESULT_FOLDER = "result-folder";
    static constexpr const char* KEY_RESULT_TYPE = "result-type";
    static constexpr const char* KEY_CACHE_ENUMERATION = "cache-enumeration";
    static constexpr const char* KEY_GRAPH_CACHE_DIR = "graph-cache-dir";
    static constexpr const char* KEY_LOAD_MOTIF_GRAPH_CACHE = "load-motif-graph-cache";
    static constexpr const char* KEY_LOAD_PATH_GRAPH_CACHE = "load-path-graph-cache";
    static constexpr const char* KEY_GRAPHML_COLOR_MAP_PATH = "graphml-color-map-path";

    static constexpr const char* READER_GRAPHML = "graphml";
    static constexpr const char* READER_JSON = "json";
    static constexpr const char* READER_VERTEX_EDGE = "vertex-edge";
    static constexpr const char* CACHE_BINARY = "binary";
    static constexpr const char* CACHE_CSV = "csv";
    static constexpr const char* RESULT_JSON = "json";
    static constexpr const char* RESULT_CSV = "csv";

    /**
     * @brief Builds the full options descriptor for all CLI flags.
     * @return Combined options_description covering all modes.
     */
    static boost::program_options::options_description build_options();

    /**
     * @brief Builds the options descriptor for preprocessing-stage flags.
     * @return options_description for --library-dir, --reader-type, --cache-dir.
     */
    static boost::program_options::options_description build_preprocess_options();

    /**
     * @brief Builds the options descriptor for filter-stage flags.
     * @return options_description for all --filter related flags.
     */
    static boost::program_options::options_description build_filter_options();

    /**
     * @brief Parses argc/argv via Boost.PO, wrapping any parse error.
     * @param argc Argument count.
     * @param argv Argument values.
     * @param desc Options descriptor.
     * @return Populated variables_map.
     * @throws SgfInvalidArgumentException on unknown option or bad syntax.
     */
    // NOLINTBEGIN(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
    static boost::program_options::variables_map
    parse_raw(int argc, char* argv[], const boost::program_options::options_description& desc);
    // NOLINTEND(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)

    /**
     * @brief Constructs a CliArgs from the parsed variables map.
     * @param variables_map Parsed variables map.
     * @return Populated CliArgs.
     */
    static CliArgs build_cli_args(const boost::program_options::variables_map& variables_map);

    /**
     * @brief Populates preprocessing-stage args from the parsed map.
     * @param variables_map Parsed variables map.
     * @return Populated PreprocessArgs.
     * @throws SgfInvalidArgumentException for missing required flags.
     */
    static PreprocessArgs
    parse_preprocess_args(const boost::program_options::variables_map& variables_map);

    /**
     * @brief Populates filter-stage args from the parsed map.
     * @param variables_map Parsed variables map.
     * @return Populated FilterArgs.
     * @throws SgfInvalidArgumentException for missing required flags.
     */
    static FilterArgs parse_filter_args(const boost::program_options::variables_map& variables_map);

    /**
     * @brief Validates that mode/feature flags are consistent.
     * @param args Parsed CLI arguments.
     * @throws SgfInvalidArgumentException if validation fails.
     */
    static void validate_mode_flags(const CliArgs& args);

    /**
     * @brief Validates that exactly one of --preprocess / --filter is set.
     * @param args Parsed CLI arguments.
     * @throws SgfInvalidArgumentException if validation fails.
     */
    static void validate_mode_selection(const CliArgs& args);

    /**
     * @brief Validates that at least one of --motifs / --paths is set.
     * @param args Parsed CLI arguments.
     * @throws SgfInvalidArgumentException if validation fails.
     */
    static void validate_feature_selection(const CliArgs& args);

    /**
     * @brief Validates filter-specific flag combinations.
     * @param filter     Parsed filter arguments.
     * @param use_motifs Whether --motifs was specified.
     * @param use_paths  Whether --paths was specified.
     * @throws SgfInvalidArgumentException if validation fails.
     */
    static void validate_filter_args(const FilterArgs& filter, bool use_motifs, bool use_paths);

    /**
     * @brief Validates that --cache-enumeration flags are mutually consistent.
     * @param filter Parsed filter arguments.
     * @throws SgfInvalidArgumentException if validation fails.
     */
    static void validate_cache_enumeration_flags(const FilterArgs& filter);

    /**
     * @brief Throws if a feature is enabled but its required cache file is empty.
     * @param enabled   Whether the feature flag is set.
     * @param path      The cache file path (empty if not provided).
     * @param flag_name CLI flag name shown in the error message.
     * @throws SgfInvalidArgumentException if enabled and path is empty.
     */
    static void require_cache_file(bool enabled, const std::string& path, const char* flag_name);

    /**
     * @brief Returns the string value of a required option, throwing if absent.
     * @param variables_map Parsed variables map.
     * @param key Option key name.
     * @return Non-empty value string.
     * @throws SgfInvalidArgumentException if the option was not provided.
     */
    static std::string
    get_required_string(const boost::program_options::variables_map& variables_map,
                        const std::string& key);

    /**
     * @brief Returns the string value of an optional option, or empty string.
     * @param variables_map Parsed variables map.
     * @param key Option key name.
     * @return Value string, or empty if not provided.
     */
    static std::string
    get_optional_string(const boost::program_options::variables_map& variables_map,
                        const std::string& key);

    /**
     * @brief Parses a uint32_t from a string, throwing on invalid input.
     * @param val       String value to parse.
     * @param flag_name CLI flag name shown in the error message.
     * @return Parsed uint32_t value.
     * @throws SgfInvalidArgumentException for non-numeric or out-of-range values.
     */
    static uint32_t parse_uint32_value(const std::string& val, const char* flag_name);

    /**
     * @brief Parses a graph reader type string.
     * @param type_str One of "graphml", "json", "vertex-edge".
     * @return Corresponding GraphReaderType.
     * @throws SgfInvalidArgumentException for unrecognised values.
     */
    static GraphReaderType parse_reader_type(const std::string& type_str);

    /**
     * @brief Parses a cache type string.
     * @param type_str One of "binary", "csv".
     * @return Corresponding CacheManagerType.
     * @throws SgfInvalidArgumentException for unrecognised values.
     */
    static CacheManagerType parse_cache_type(const std::string& type_str);

    /**
     * @brief Parses a result output type string.
     * @param type_str One of "json", "csv".
     * @return Corresponding ResultOutputType.
     * @throws SgfInvalidArgumentException for unrecognised values.
     */
    static ResultOutputType parse_result_type(const std::string& type_str);
};

}  // namespace sgf
