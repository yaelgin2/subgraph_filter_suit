#include "SgfPatternFinderArgumentParser.h"

#include "FlowManager.h"
#include "PriorPolicy.h"
#include "SgfInvalidArgumentException.h"
#include "SingleGraphPatternPreprocessor.h"

// NOLINTNEXTLINE(misc-include-cleaner)
#include <boost/program_options.hpp>
#include <boost/program_options/errors.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/value_semantic.hpp>
#include <boost/program_options/variables_map.hpp>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>

namespace po = boost::program_options;

namespace sgf
{

// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
std::optional<PatternFinderCliArgs> SgfPatternFinderArgumentParser::parse(
    const int argc,
    char* argv
        [])  // NOLINT(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays,-warnings-as-errors)
{
    const po::options_description desc = build_options();
    const po::variables_map variables_map = parse_raw(argc, argv, desc);
    const bool show_help = (variables_map.count(KEY_HELP) != 0U) || (argc == 1);
    if (show_help)
    {
        std::cout << desc << '\n';
        return std::nullopt;
    }
    PatternFinderCliArgs cli_args = build_cli_args(variables_map);
    validate(cli_args);
    return cli_args;
}

po::options_description SgfPatternFinderArgumentParser::build_options()
{
    po::options_description mode_desc("Mode flags (at least one required)");
    mode_desc.add_options()("help,h", "Print this help message")(
        KEY_PREPROCESS, po::bool_switch(), "Run the pattern preprocessing stage")(
        KEY_FILTER, po::bool_switch(), "Run the pattern filter stage");

    po::options_description common_desc("Common flags");
    common_desc.add_options()(KEY_IS_DIRECTED, po::bool_switch(), "Treat graphs as directed")(
        KEY_READER_TYPE, po::value<std::string>(),
        "Graph file format: graphml, json, vertex-edge (required)")(
        KEY_LOG_FILE_PATH, po::value<std::string>(), "(optional) Log file path");

    po::options_description all_desc("sgf-pattern-finder options");
    all_desc.add(mode_desc)
        .add(common_desc)
        .add(build_preprocess_options())
        .add(build_filter_options())
        .add(build_config_options());
    return all_desc;
}

po::options_description SgfPatternFinderArgumentParser::build_preprocess_options()
{
    po::options_description desc("Preprocess flags (required with --preprocess)");
    desc.add_options()(KEY_LIBRARY_INPUT_FOLDER, po::value<std::string>(),
                       "Directory containing the input graph library")(
        KEY_OUTPUT_FOLDER, po::value<std::string>(),
        "Directory where extracted patterns are written")(
        KEY_PATTERN_OUTPUT_TYPE, po::value<std::string>(),
        "Pattern file format: graphml, json, vertex-edge")(
        KEY_PREPROCESS_SINGLE_GRAPH_FROM_RESULTS, po::bool_switch(),
        "Derive single-graph index from a results file instead of --preprocess-single-graph")(
        KEY_RESULTS_FILE_PATH, po::value<std::string>(),
        "Path to results file (required with --preprocess-single-graph-from-results)")(
        KEY_RESULTS_FILE_TYPE, po::value<std::string>()->default_value("json"),
        "Results file format: json, csv (default: json)")(
        KEY_PREPROCESS_SINGLE_GRAPH, po::value<std::string>()->default_value("0"),
        "Index of single graph to process (required when using single-graph mode, default: 0)")(
        KEY_BACKGROUND_GRAPH_PATH, po::value<std::string>()->default_value(""),
        "(optional) Path to background graph file for pattern scoring")(
        KEY_SCORE_THRESHOLD, po::value<std::string>()->default_value("0.0"),
        "Pattern score cutoff; beam search stops below this value (default: 0.0)");
    return desc;
}

po::options_description SgfPatternFinderArgumentParser::build_filter_options()
{
    po::options_description desc("Filter flags (required with --filter)");
    desc.add_options()(KEY_PATTERN_MAPPING_CACHE, po::value<std::string>(),
                       "Path to the pattern cache produced by the preprocessing stage")(
        KEY_PATTERN_TYPE, po::value<std::string>(),
        "Pattern cache file format: graphml, json, vertex-edge")(
        KEY_BACKGROUND_GRAPH_FOLDER, po::value<std::string>(),
        "Path to the background graph used during filtering")(
        KEY_OUTPUT_PATH, po::value<std::string>(), "Directory where filter results are written")(
        KEY_OUTPUT_TYPE, po::value<std::string>(), "Filter results format: json, csv")(
        KEY_PRIOR_POLICY, po::value<std::string>(),
        "Vertex ordering heuristic: subgraph-degree-squared, graph-degree-squared, "
        "constant, random, subgraph-degree, combined")(KEY_IS_INDUCED, po::bool_switch(),
                                                       "Search for induced subgraph matches");
    return desc;
}

po::options_description SgfPatternFinderArgumentParser::build_config_options()
{
    po::options_description desc(
        "SingleGraphFinder config flags (optional, used with --preprocess)");
    desc.add_options()(KEY_MAX_ACTIVE_PATTERNS, po::value<std::string>()->default_value("500"),
                       "Maximum number of simultaneously active beam states (default: 500)")(
        KEY_ALPHA_0, po::value<std::string>()->default_value("1.0"),
        "Initial weight for the outside-neighbour score term (default: 1.0)")(
        KEY_ALPHA_DECAY, po::value<std::string>()->default_value("0.9"),
        "Per-depth multiplicative decay applied to alpha-0 (default: 0.9)");
    return desc;
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
po::variables_map SgfPatternFinderArgumentParser::parse_raw(const int argc, char* argv[],
                                                            const po::options_description& desc)
{
    po::variables_map variables_map;
    try
    {
        po::store(po::parse_command_line(argc, argv, desc), variables_map);
        po::notify(variables_map);
    }
    catch (const po::error& ex)
    {
        throw SgfInvalidArgumentException(ex.what());
    }
    return variables_map;
}

PatternFinderCliArgs
SgfPatternFinderArgumentParser::build_cli_args(const po::variables_map& variables_map)
{
    PatternFinderCliArgs result;
    result.m_run_preprocess = variables_map.at(KEY_PREPROCESS).as<bool>();
    result.m_run_filter = variables_map.at(KEY_FILTER).as<bool>();
    result.m_is_directed = variables_map.at(KEY_IS_DIRECTED).as<bool>();
    result.m_reader_type = parse_reader_type(get_required_string(variables_map, KEY_READER_TYPE));
    result.m_log_file_path = get_optional_string(variables_map, KEY_LOG_FILE_PATH);
    if (result.m_run_preprocess)
    {
        result.m_preprocess = parse_preprocess_args(variables_map);
    }
    if (result.m_run_filter)
    {
        result.m_filter = parse_filter_args(variables_map);
    }
    return result;
}

PatternPreprocessArgs
SgfPatternFinderArgumentParser::parse_preprocess_args(const po::variables_map& variables_map)
{
    PatternPreprocessArgs result;
    result.m_library_input_folder = get_required_string(variables_map, KEY_LIBRARY_INPUT_FOLDER);
    result.m_output_folder = get_required_string(variables_map, KEY_OUTPUT_FOLDER);
    result.m_pattern_output_type =
        parse_pattern_writer_type(get_required_string(variables_map, KEY_PATTERN_OUTPUT_TYPE));
    result.m_preprocess_single_graph_from_results =
        variables_map.at(KEY_PREPROCESS_SINGLE_GRAPH_FROM_RESULTS).as<bool>();
    result.m_results_file_path = get_optional_string(variables_map, KEY_RESULTS_FILE_PATH);
    result.m_results_file_type =
        parse_result_type(variables_map.at(KEY_RESULTS_FILE_TYPE).as<std::string>());
    result.m_preprocess_single_graph =
        parse_int64_value(variables_map.at(KEY_PREPROCESS_SINGLE_GRAPH).as<std::string>(),
                          KEY_PREPROCESS_SINGLE_GRAPH);
    result.m_background_graph_path = variables_map.at(KEY_BACKGROUND_GRAPH_PATH).as<std::string>();
    result.m_score_threshold = parse_double_value(
        variables_map.at(KEY_SCORE_THRESHOLD).as<std::string>(), KEY_SCORE_THRESHOLD);
    result.m_config.m_max_active_patterns = parse_uint32_value(
        variables_map.at(KEY_MAX_ACTIVE_PATTERNS).as<std::string>(), KEY_MAX_ACTIVE_PATTERNS);
    result.m_config.m_alpha_0 =
        parse_double_value(variables_map.at(KEY_ALPHA_0).as<std::string>(), KEY_ALPHA_0);
    result.m_config.m_alpha_decay =
        parse_double_value(variables_map.at(KEY_ALPHA_DECAY).as<std::string>(), KEY_ALPHA_DECAY);
    return result;
}

PatternFilterArgs
SgfPatternFinderArgumentParser::parse_filter_args(const po::variables_map& variables_map)
{
    PatternFilterArgs result;
    result.m_pattern_mapping_cache = get_required_string(variables_map, KEY_PATTERN_MAPPING_CACHE);
    result.m_pattern_type =
        parse_pattern_writer_type(get_required_string(variables_map, KEY_PATTERN_TYPE));
    result.m_background_graph_folder =
        get_required_string(variables_map, KEY_BACKGROUND_GRAPH_FOLDER);
    result.m_output_path = get_required_string(variables_map, KEY_OUTPUT_PATH);
    result.m_output_type = parse_result_type(get_required_string(variables_map, KEY_OUTPUT_TYPE));
    result.m_prior_policy =
        parse_prior_policy(get_required_string(variables_map, KEY_PRIOR_POLICY));
    result.m_is_induced = variables_map.at(KEY_IS_INDUCED).as<bool>();
    return result;
}

void SgfPatternFinderArgumentParser::validate(const PatternFinderCliArgs& args)
{
    if (!args.m_run_preprocess && !args.m_run_filter)
    {
        throw SgfInvalidArgumentException(
            "At least one of --preprocess or --filter must be specified.");
    }
    if (args.m_run_preprocess)
    {
        validate_preprocess_args(args.m_preprocess);
    }
    if (args.m_run_filter)
    {
        validate_filter_args(args.m_filter);
    }
}

void SgfPatternFinderArgumentParser::validate_preprocess_args(const PatternPreprocessArgs& args)
{
    if (args.m_preprocess_single_graph_from_results && args.m_results_file_path.empty())
    {
        throw SgfInvalidArgumentException(
            "--results-file-path is required when --preprocess-single-graph-from-results is set.");
    }
}

void SgfPatternFinderArgumentParser::validate_filter_args(const PatternFilterArgs& /*args*/)
{
}

std::string
SgfPatternFinderArgumentParser::get_required_string(const po::variables_map& variables_map,
                                                    const std::string& key)
{
    if (variables_map.count(key) == 0U)
    {
        throw SgfInvalidArgumentException("Required flag '--" + key + "' is missing.");
    }
    return variables_map.at(key).as<std::string>();
}

std::string
SgfPatternFinderArgumentParser::get_optional_string(const po::variables_map& variables_map,
                                                    const std::string& key)
{
    if (variables_map.count(key) == 0U)
    {
        return {};
    }
    return variables_map.at(key).as<std::string>();
}

uint32_t SgfPatternFinderArgumentParser::parse_uint32_value(const std::string& val,
                                                            const char* flag_name)
{
    try
    {
        const unsigned long result = std::stoul(val);
        if (result > std::numeric_limits<uint32_t>::max())
        {
            throw SgfInvalidArgumentException("Value out of range for --" + std::string(flag_name) +
                                              ": '" + val + "'");
        }
        return static_cast<uint32_t>(result);
    }
    catch (const SgfInvalidArgumentException&)
    {
        throw;
    }
    catch (const std::out_of_range&)
    {
        throw SgfInvalidArgumentException("Value out of range for --" + std::string(flag_name) +
                                          ": '" + val + "'");
    }
    catch (const std::invalid_argument&)
    {
        throw SgfInvalidArgumentException("Invalid integer value for --" + std::string(flag_name) +
                                          ": '" + val + "'");
    }
}

int64_t SgfPatternFinderArgumentParser::parse_int64_value(const std::string& val,
                                                          const char* flag_name)
{
    try
    {
        return std::stoll(val);
    }
    catch (const std::out_of_range&)
    {
        throw SgfInvalidArgumentException("Value out of range for --" + std::string(flag_name) +
                                          ": '" + val + "'");
    }
    catch (const std::invalid_argument&)
    {
        throw SgfInvalidArgumentException("Invalid integer value for --" + std::string(flag_name) +
                                          ": '" + val + "'");
    }
}

double SgfPatternFinderArgumentParser::parse_double_value(const std::string& val,
                                                          const char* flag_name)
{
    try
    {
        return std::stod(val);
    }
    catch (const std::out_of_range&)
    {
        throw SgfInvalidArgumentException("Value out of range for --" + std::string(flag_name) +
                                          ": '" + val + "'");
    }
    catch (const std::invalid_argument&)
    {
        throw SgfInvalidArgumentException("Invalid numeric value for --" + std::string(flag_name) +
                                          ": '" + val + "'");
    }
}

GraphReaderType SgfPatternFinderArgumentParser::parse_reader_type(const std::string& type_str)
{
    if (type_str == READER_GRAPHML)
    {
        return GraphReaderType::GRAPHML;
    }
    if (type_str == READER_JSON)
    {
        return GraphReaderType::JSON;
    }
    if (type_str == READER_VERTEX_EDGE)
    {
        return GraphReaderType::VERTEX_EDGE;
    }
    throw SgfInvalidArgumentException("Unknown reader type '" + type_str +
                                      "'. Valid values: graphml, json, vertex-edge.");
}

PatternWriterType
SgfPatternFinderArgumentParser::parse_pattern_writer_type(const std::string& type_str)
{
    if (type_str == READER_GRAPHML)
    {
        return PatternWriterType::GRAPHML;
    }
    if (type_str == READER_JSON)
    {
        return PatternWriterType::JSON;
    }
    if (type_str == READER_VERTEX_EDGE)
    {
        return PatternWriterType::VERTEX_EDGE;
    }
    throw SgfInvalidArgumentException("Unknown pattern type '" + type_str +
                                      "'. Valid values: graphml, json, vertex-edge.");
}

ResultOutputType SgfPatternFinderArgumentParser::parse_result_type(const std::string& type_str)
{
    if (type_str == RESULT_JSON)
    {
        return ResultOutputType::JSON;
    }
    if (type_str == RESULT_CSV)
    {
        return ResultOutputType::CSV;
    }
    throw SgfInvalidArgumentException("Unknown result type '" + type_str +
                                      "'. Valid values: json, csv.");
}

// NOLINTNEXTLINE(readability-function-size)
PriorPolicy SgfPatternFinderArgumentParser::parse_prior_policy(const std::string& policy_str)
{
    if (policy_str == POLICY_SUBGRAPH_DEGREE_SQUARED)
    {
        return PriorPolicy::SUBGRAPH_DEGREE_SQUARED;
    }
    if (policy_str == POLICY_GRAPH_DEGREE_SQUARED)
    {
        return PriorPolicy::GRAPH_DEGREE_SQUARED;
    }
    if (policy_str == POLICY_CONSTANT)
    {
        return PriorPolicy::CONSTANT;
    }
    if (policy_str == POLICY_RANDOM)
    {
        return PriorPolicy::RANDOM;
    }
    if (policy_str == POLICY_SUBGRAPH_DEGREE)
    {
        return PriorPolicy::SUBGRAPH_DEGREE;
    }
    if (policy_str == POLICY_COMBINED)
    {
        return PriorPolicy::COMBINED;
    }
    throw SgfInvalidArgumentException(
        "Unknown prior policy '" + policy_str +
        "'. Valid values: subgraph-degree-squared, graph-degree-squared, "
        "constant, random, subgraph-degree, combined.");
}

}  // namespace sgf
