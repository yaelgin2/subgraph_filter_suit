#pragma once

#include "FlowManager.h"
#include "PriorPolicy.h"

// NOLINTNEXTLINE(misc-include-cleaner)
#include <boost/program_options.hpp>
#include <optional>
#include <string>

namespace sgf
{

/**
 * @brief All parsed CLI arguments for sgf-graph-searcher.
 */
struct GraphSearcherArgs
{
    std::string m_subgraph_path;                               ///< Path to subgraph file.
    std::string m_background_path;                             ///< Path to background graph file.
    GraphReaderType m_reader_type{GraphReaderType::GRAPHML};   ///< Graph file format.
    bool m_is_directed{false};                                 ///< Treat graphs as directed.
    bool m_is_induced{false};                                  ///< Search for induced subgraph.
    PriorPolicy m_prior_policy{PriorPolicy::SUBGRAPH_DEGREE};  ///< Vertex ordering heuristic.
    bool m_stop_on_first_match{false};                         ///< Stop after first match found.
    std::string m_output_path;                                 ///< Optional output file path.
    std::string m_log_file_path;                               ///< Optional log file path.
};

/**
 * @brief Parses and validates command-line arguments for sgf-graph-searcher.
 *
 * All argument parsing, validation, and help-display logic is encapsulated here.
 * The single public entry point is parse(), which returns std::nullopt when the
 * caller should exit with success (i.e. help was displayed).
 */
class SgfGraphSearcherArgumentParser
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
     * @return Validated GraphSearcherArgs, or std::nullopt if help was displayed.
     * @throws SgfInvalidArgumentException on invalid or missing arguments.
     */
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
    static std::optional<GraphSearcherArgs> parse(int argc, char* argv[]);

private:
    static constexpr const char* KEY_HELP = "help";
    static constexpr const char* KEY_SUBGRAPH_PATH = "subgraph-path";
    static constexpr const char* KEY_BACKGROUND_PATH = "background-path";
    static constexpr const char* KEY_READER_TYPE = "reader-type";
    static constexpr const char* KEY_IS_DIRECTED = "directed";
    static constexpr const char* KEY_IS_INDUCED = "induced";
    static constexpr const char* KEY_PRIOR_POLICY = "prior-policy";
    static constexpr const char* KEY_STOP_ON_FIRST_MATCH = "stop-on-first-match";
    static constexpr const char* KEY_OUTPUT_PATH = "output-path";
    static constexpr const char* KEY_LOG_FILE_PATH = "log-file-path";

    static constexpr const char* READER_GRAPHML = "graphml";
    static constexpr const char* READER_JSON = "json";
    static constexpr const char* READER_VERTEX_EDGE = "vertex-edge";

    static constexpr const char* POLICY_SUBGRAPH_DEGREE_SQUARED = "subgraph-degree-squared";
    static constexpr const char* POLICY_GRAPH_DEGREE_SQUARED = "graph-degree-squared";
    static constexpr const char* POLICY_CONSTANT = "constant";
    static constexpr const char* POLICY_RANDOM = "random";
    static constexpr const char* POLICY_SUBGRAPH_DEGREE = "subgraph-degree";
    static constexpr const char* POLICY_COMBINED = "combined";

    /**
     * @brief Builds the full options descriptor for all CLI flags.
     * @return Combined options_description.
     */
    static boost::program_options::options_description build_options();

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
     * @brief Constructs a GraphSearcherArgs from the parsed variables map.
     * @param variables_map Parsed variables map.
     * @return Populated GraphSearcherArgs.
     * @throws SgfInvalidArgumentException for missing required flags.
     */
    static GraphSearcherArgs build_args(const boost::program_options::variables_map& variables_map);

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
     * @brief Parses a graph reader type string.
     * @param type_str One of "graphml", "json", "vertex-edge".
     * @return Corresponding GraphReaderType.
     * @throws SgfInvalidArgumentException for unrecognised values.
     */
    static GraphReaderType parse_reader_type(const std::string& type_str);

    /**
     * @brief Parses a prior policy string.
     * @param policy_str One of "subgraph-degree-squared", "graph-degree-squared",
     *                   "constant", "random", "subgraph-degree", "combined".
     * @return Corresponding PriorPolicy.
     * @throws SgfInvalidArgumentException for unrecognised values.
     */
    static PriorPolicy parse_prior_policy(const std::string& policy_str);
};

}  // namespace sgf
