#include "SgfGraphSearcherArgumentParser.h"

#include "FlowManager.h"
#include "PriorPolicy.h"
#include "SgfInvalidArgumentException.h"

// NOLINTNEXTLINE(misc-include-cleaner)
#include <boost/program_options.hpp>
#include <boost/program_options/errors.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/value_semantic.hpp>
#include <boost/program_options/variables_map.hpp>
#include <iostream>
#include <optional>
#include <string>

namespace po = boost::program_options;

namespace sgf
{

// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays)
std::optional<GraphSearcherArgs>
SgfGraphSearcherArgumentParser::parse(const int argc,
                                      char* argv[])  // NOLINT(modernize-avoid-c-arrays)
{
    const po::options_description desc = build_options();
    const po::variables_map variables_map = parse_raw(argc, argv, desc);
    const bool show_help = (variables_map.count(KEY_HELP) != 0U) || (argc == 1);
    if (show_help)
    {
        std::cout << desc << '\n';
        return std::nullopt;
    }
    return build_args(variables_map);
}

po::options_description SgfGraphSearcherArgumentParser::build_options()
{
    po::options_description desc("sgf-graph-searcher options");
    desc.add_options()("help,h", "Print this help message")(
        KEY_SUBGRAPH_PATH, po::value<std::string>(),
        "Path to the subgraph file (required)")(KEY_BACKGROUND_PATH, po::value<std::string>(),
                                                "Path to the background graph file (required)")(
        KEY_READER_TYPE, po::value<std::string>(),
        "Graph file format: graphml, json, vertex-edge (required)")(
        KEY_IS_DIRECTED, po::bool_switch(), "Treat graphs as directed")(
        KEY_IS_INDUCED, po::bool_switch(), "Search for induced subgraph")(
        KEY_PRIOR_POLICY, po::value<std::string>(),
        "Vertex ordering heuristic: subgraph-degree-squared, graph-degree-squared, "
        "constant, random, subgraph-degree, combined (required)")(
        KEY_STOP_ON_FIRST_MATCH, po::bool_switch(), "Stop after first match is found")(
        KEY_OUTPUT_PATH, po::value<std::string>(), "(optional) Output file path")(
        KEY_LOG_FILE_PATH, po::value<std::string>(), "(optional) Log file path");
    return desc;
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
po::variables_map SgfGraphSearcherArgumentParser::parse_raw(const int argc, char* argv[],
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

GraphSearcherArgs SgfGraphSearcherArgumentParser::build_args(const po::variables_map& variables_map)
{
    GraphSearcherArgs result;
    result.m_subgraph_path = get_required_string(variables_map, KEY_SUBGRAPH_PATH);
    result.m_background_path = get_required_string(variables_map, KEY_BACKGROUND_PATH);
    result.m_reader_type = parse_reader_type(get_required_string(variables_map, KEY_READER_TYPE));
    result.m_prior_policy =
        parse_prior_policy(get_required_string(variables_map, KEY_PRIOR_POLICY));
    result.m_is_directed = variables_map.at(KEY_IS_DIRECTED).as<bool>();
    result.m_is_induced = variables_map.at(KEY_IS_INDUCED).as<bool>();
    result.m_stop_on_first_match = variables_map.at(KEY_STOP_ON_FIRST_MATCH).as<bool>();
    result.m_output_path = get_optional_string(variables_map, KEY_OUTPUT_PATH);
    result.m_log_file_path = get_optional_string(variables_map, KEY_LOG_FILE_PATH);
    return result;
}

std::string
SgfGraphSearcherArgumentParser::get_required_string(const po::variables_map& variables_map,
                                                    const std::string& key)
{
    if (variables_map.count(key) == 0U)
    {
        throw SgfInvalidArgumentException("Required flag '--" + key + "' is missing.");
    }
    return variables_map.at(key).as<std::string>();
}

std::string
SgfGraphSearcherArgumentParser::get_optional_string(const po::variables_map& variables_map,
                                                    const std::string& key)
{
    if (variables_map.count(key) == 0U)
    {
        return {};
    }
    return variables_map.at(key).as<std::string>();
}

GraphReaderType SgfGraphSearcherArgumentParser::parse_reader_type(const std::string& type_str)
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

// NOLINTNEXTLINE(readability-function-size)
PriorPolicy SgfGraphSearcherArgumentParser::parse_prior_policy(const std::string& policy_str)
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
