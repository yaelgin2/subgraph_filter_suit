#pragma once

#include "FlowManager.h"
#include "PriorPolicy.h"
#include "SingleGraphPatternPreprocessor.h"

// NOLINTNEXTLINE(misc-include-cleaner)
#include <boost/program_options.hpp>
#include <cstdint>
#include <optional>
#include <string>

namespace sgf
{

/**
 * @brief Arguments consumed by the pattern preprocessing stage.
 */
struct PatternPreprocessArgs
{
    std::string m_library_input_folder;  ///< Input graph library directory.
    std::string m_output_folder;         ///< Output directory for patterns.
    PatternWriterType m_pattern_output_type{PatternWriterType::GRAPHML};  ///< Pattern file format.
    bool m_preprocess_single_graph_from_results{
        false};                       ///< Load single graph index from a results file.
    std::string m_results_file_path;  ///< Path to results file (required when
                                      ///< m_preprocess_single_graph_from_results is true).
    ResultOutputType m_results_file_type{ResultOutputType::JSON};  ///< Format of the results file.
    int64_t m_preprocess_single_graph{-1};  ///< Index of single graph to process; -1 = disabled.
    std::string m_background_graph_path;    ///< Path to the background graph file.
    double m_score_threshold{0.0};          ///< Pattern score cutoff.
    SingleGraphFinderConfig m_config{};     ///< Beam search tuning parameters.
    uint32_t m_preprocess_multigraph{0U};   ///< Number of multigraph patterns to extract; 0 = disabled.
    double m_multigraph_alive_percent{0.5}; ///< Fraction of library graphs a pattern must appear in.
};

/**
 * @brief Arguments consumed by the pattern filter stage.
 */
struct PatternFilterArgs
{
    std::string m_pattern_mapping_cache;                           ///< Path to pattern cache.
    PatternWriterType m_pattern_type{PatternWriterType::GRAPHML};  ///< Pattern cache file format.
    std::string m_background_graph_folder;                         ///< Path to background graph.
    std::string m_output_path;  ///< Output directory for filter results.
    ResultOutputType m_output_type{ResultOutputType::JSON};    ///< Filter results format.
    PriorPolicy m_prior_policy{PriorPolicy::SUBGRAPH_DEGREE};  ///< Vertex ordering heuristic.
    bool m_is_induced{false};  ///< Search for induced subgraph matches.
};

/**
 * @brief All parsed CLI arguments for sgf-pattern-finder.
 */
struct PatternFinderCliArgs
{
    bool m_run_preprocess{false};  ///< Run the pattern preprocessing stage.
    bool m_run_filter{false};      ///< Run the pattern filter stage.
    bool m_is_directed{false};     ///< Treat graphs as directed.
    GraphReaderType m_reader_type{GraphReaderType::GRAPHML};  ///< Graph file format.
    std::string m_log_file_path;                              ///< Optional log file path.
    PatternPreprocessArgs m_preprocess{};                     ///< Preprocessing-stage arguments.
    PatternFilterArgs m_filter{};                             ///< Filter-stage arguments.
};

/**
 * @brief Parses and validates command-line arguments for sgf-pattern-finder.
 *
 * All argument parsing, validation, and help-display logic is encapsulated here.
 * The single public entry point is parse(), which returns std::nullopt when the
 * caller should exit with success (i.e. help was displayed).
 */
class SgfPatternFinderArgumentParser
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
     * @return Validated PatternFinderCliArgs, or std::nullopt if help was displayed.
     * @throws SgfInvalidArgumentException on invalid or missing arguments.
     */
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
    static std::optional<PatternFinderCliArgs> parse(int argc, char* argv[]);

private:
    static constexpr const char* KEY_HELP = "help";
    static constexpr const char* KEY_PREPROCESS = "preprocess";
    static constexpr const char* KEY_FILTER = "filter";
    static constexpr const char* KEY_IS_DIRECTED = "is-directed";
    static constexpr const char* KEY_READER_TYPE = "reader-type";
    static constexpr const char* KEY_LOG_FILE_PATH = "log-file-path";

    static constexpr const char* KEY_LIBRARY_INPUT_FOLDER = "library-input-folder";
    static constexpr const char* KEY_OUTPUT_FOLDER = "output-folder";
    static constexpr const char* KEY_PATTERN_OUTPUT_TYPE = "pattern-output-type";
    static constexpr const char* KEY_PREPROCESS_SINGLE_GRAPH_FROM_RESULTS =
        "preprocess-single-graph-from-results";
    static constexpr const char* KEY_RESULTS_FILE_PATH = "results-file-path";
    static constexpr const char* KEY_RESULTS_FILE_TYPE = "results-file-type";
    static constexpr const char* KEY_PREPROCESS_SINGLE_GRAPH = "preprocess-single-graph";
    static constexpr int64_t SINGLE_GRAPH_DISABLED = -1;  ///< Sentinel: single-graph mode off.
    static constexpr const char* KEY_BACKGROUND_GRAPH_PATH = "background-graph-path";
    static constexpr const char* KEY_SCORE_THRESHOLD = "score-threshold";
    static constexpr const char* KEY_PREPROCESS_MULTIGRAPH = "preprocess-multigraph";
    static constexpr const char* KEY_MULTIGRAPH_ALIVE_PERCENT = "multigraph-alive-percent";
    static constexpr const char* KEY_MAX_ACTIVE_PATTERNS = "max-active-patterns";
    static constexpr const char* KEY_ALPHA_0 = "alpha-0";
    static constexpr const char* KEY_ALPHA_DECAY = "alpha-decay";

    static constexpr const char* KEY_PATTERN_MAPPING_CACHE = "pattern-mapping-cache";
    static constexpr const char* KEY_PATTERN_TYPE = "pattern-type";
    static constexpr const char* KEY_BACKGROUND_GRAPH_FOLDER = "background-graph-folder";
    static constexpr const char* KEY_OUTPUT_PATH = "output-path";
    static constexpr const char* KEY_OUTPUT_TYPE = "output-type";
    static constexpr const char* KEY_PRIOR_POLICY = "prior-policy";
    static constexpr const char* KEY_IS_INDUCED = "is-induced";

    static constexpr const char* READER_GRAPHML = "graphml";
    static constexpr const char* READER_JSON = "json";
    static constexpr const char* READER_VERTEX_EDGE = "vertex-edge";
    static constexpr const char* RESULT_JSON = "json";
    static constexpr const char* RESULT_CSV = "csv";

    static constexpr const char* POLICY_SUBGRAPH_DEGREE_SQUARED = "subgraph-degree-squared";
    static constexpr const char* POLICY_GRAPH_DEGREE_SQUARED = "graph-degree-squared";
    static constexpr const char* POLICY_CONSTANT = "constant";
    static constexpr const char* POLICY_RANDOM = "random";
    static constexpr const char* POLICY_SUBGRAPH_DEGREE = "subgraph-degree";
    static constexpr const char* POLICY_COMBINED = "combined";

    /**
     * @brief Builds the full options descriptor for all CLI flags.
     */
    static boost::program_options::options_description build_options();

    /**
     * @brief Builds the options descriptor for preprocessing-stage flags.
     */
    static boost::program_options::options_description build_preprocess_options();

    /**
     * @brief Builds the options descriptor for filter-stage flags.
     */
    static boost::program_options::options_description build_filter_options();

    /**
     * @brief Builds the options descriptor for SingleGraphFinderConfig tuning flags.
     */
    static boost::program_options::options_description build_config_options();

    /**
     * @brief Parses argc/argv via Boost.PO, wrapping any parse error.
     * @throws SgfInvalidArgumentException on unknown option or bad syntax.
     */
    // NOLINTBEGIN(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
    static boost::program_options::variables_map
    parse_raw(int argc, char* argv[], const boost::program_options::options_description& desc);
    // NOLINTEND(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)

    /**
     * @brief Constructs a PatternFinderCliArgs from the parsed variables map.
     * @throws SgfInvalidArgumentException on invalid or missing arguments.
     */
    static PatternFinderCliArgs
    build_cli_args(const boost::program_options::variables_map& variables_map);

    /**
     * @brief Populates preprocessing-stage args from the parsed map.
     * @throws SgfInvalidArgumentException for missing required flags.
     */
    static PatternPreprocessArgs
    parse_preprocess_args(const boost::program_options::variables_map& variables_map);

    /**
     * @brief Populates filter-stage args from the parsed map.
     * @throws SgfInvalidArgumentException for missing required flags.
     */
    static PatternFilterArgs
    parse_filter_args(const boost::program_options::variables_map& variables_map);

    /**
     * @brief Validates that mode/feature flags and their dependencies are consistent.
     * @throws SgfInvalidArgumentException if validation fails.
     */
    static void validate(const PatternFinderCliArgs& args);

    /**
     * @brief Validates preprocessing-stage argument consistency.
     * @throws SgfInvalidArgumentException if validation fails.
     */
    static void validate_preprocess_args(const PatternPreprocessArgs& args);

    /**
     * @brief Validates filter-stage argument consistency.
     * @throws SgfInvalidArgumentException if validation fails.
     */
    static void validate_filter_args(const PatternFilterArgs& args);

    /**
     * @brief Returns the string value of a required option, throwing if absent.
     * @throws SgfInvalidArgumentException if the option was not provided.
     */
    static std::string
    get_required_string(const boost::program_options::variables_map& variables_map,
                        const std::string& key);

    /**
     * @brief Returns the string value of an optional option, or empty string.
     */
    static std::string
    get_optional_string(const boost::program_options::variables_map& variables_map,
                        const std::string& key);

    /**
     * @brief Parses a uint32_t from a string, throwing on invalid input.
     * @throws SgfInvalidArgumentException for non-numeric or out-of-range values.
     */
    static uint32_t parse_uint32_value(const std::string& val, const char* flag_name);

    /**
     * @brief Parses an int64_t from a string, throwing on invalid input.
     * @throws SgfInvalidArgumentException for non-numeric or out-of-range values.
     */
    static int64_t parse_int64_value(const std::string& val, const char* flag_name);

    /**
     * @brief Parses a double from a string, throwing on invalid input.
     * @throws SgfInvalidArgumentException for non-numeric values.
     */
    static double parse_double_value(const std::string& val, const char* flag_name);

    /**
     * @brief Parses a graph reader type string.
     * @param type_str One of "graphml", "json", "vertex-edge".
     * @throws SgfInvalidArgumentException for unrecognised values.
     */
    static GraphReaderType parse_reader_type(const std::string& type_str);

    /**
     * @brief Parses a pattern writer type string.
     * @param type_str One of "graphml", "json", "vertex-edge".
     * @throws SgfInvalidArgumentException for unrecognised values.
     */
    static PatternWriterType parse_pattern_writer_type(const std::string& type_str);

    /**
     * @brief Parses a result output type string.
     * @param type_str One of "json", "csv".
     * @throws SgfInvalidArgumentException for unrecognised values.
     */
    static ResultOutputType parse_result_type(const std::string& type_str);

    /**
     * @brief Parses a prior policy string.
     * @param policy_str One of "subgraph-degree-squared", "graph-degree-squared",
     *                   "constant", "random", "subgraph-degree", "combined".
     * @throws SgfInvalidArgumentException for unrecognised values.
     */
    static PriorPolicy parse_prior_policy(const std::string& policy_str);
};

}  // namespace sgf
