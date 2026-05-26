#include "SgfGraphEnumeratorArgumentParser.h"

#include "SgfInvalidArgumentException.h"

// NOLINTNEXTLINE(misc-include-cleaner)
#include <boost/program_options.hpp>
#include <iostream>
#include <optional>
#include <string>

namespace po = boost::program_options;

namespace sgf
{

std::optional<CliArgs> SgfGraphEnumeratorArgumentParser::parse(const int argc, char* argv[])
{
    const po::options_description desc = build_options();
    const po::variables_map vm = parse_raw(argc, argv, desc);
    const bool show_help = (vm.count(KEY_HELP) != 0U) || (argc == 1);
    if (show_help)
    {
        std::cout << desc << '\n';
        return std::nullopt;
    }
    const CliArgs cli_args = build_cli_args(vm);
    validate_mode_flags(cli_args);
    return cli_args;
}

po::options_description SgfGraphEnumeratorArgumentParser::build_options()
{
    po::options_description mode_desc("Mode flags (at least one required)");
    mode_desc.add_options()("help,h", "Print this help message")(
        KEY_PREPROCESS, po::bool_switch(), "Run the enumeration preprocessing stage")(
        KEY_FILTER, po::bool_switch(), "Run the enumeration filter stage");

    po::options_description feature_desc("Feature flags (at least one required)");
    feature_desc.add_options()(KEY_MOTIFS, po::bool_switch(), "Enable motif-based processing")(
        KEY_PATHS, po::bool_switch(), "Enable path-based processing");

    po::options_description common_desc("Common flags");
    common_desc.add_options()(KEY_IS_DIRECTED, po::bool_switch(), "Treat graphs as directed")(
        KEY_CACHE_TYPE, po::value<std::string>(), "Cache format: binary, csv")(
        KEY_LOG_FILE_PATH, po::value<std::string>(), "(optional) Log file path");

    po::options_description all_desc("sgf-graph-enumerator options");
    all_desc.add(mode_desc)
        .add(feature_desc)
        .add(common_desc)
        .add(build_preprocess_options())
        .add(build_filter_options());
    return all_desc;
}

po::options_description SgfGraphEnumeratorArgumentParser::build_preprocess_options()
{
    po::options_description desc("Preprocess flags (required with --preprocess)");
    desc.add_options()(KEY_LIBRARY_DIR, po::value<std::string>(),
                       "Directory containing the graph library")(
        KEY_READER_TYPE, po::value<std::string>(), "Graph file format: graphml, vertex-edge, json")(
        KEY_CACHE_DIR, po::value<std::string>(), "Directory where the cache library is written");
    return desc;
}

po::options_description SgfGraphEnumeratorArgumentParser::build_filter_options()
{
    po::options_description desc("Filter flags (required with --filter)");
    desc.add_options()(KEY_GRAPH_DIR, po::value<std::string>(),
                       "Directory containing query graphs")(
        KEY_GRAPH_INPUT_TYPE, po::value<std::string>(),
        "Graph file format: graphml, vertex-edge, json")(
        KEY_RESULT_FOLDER, po::value<std::string>(), "Directory for filter result output")(
        KEY_RESULT_TYPE, po::value<std::string>(),
        "Result format: json, csv")(KEY_MOTIF_CACHE_FILE, po::value<std::string>(),
                                    "Full path to the motif cache file (required with --motifs)")(
        KEY_PATH_CACHE_FILE, po::value<std::string>(),
        "Full path to the path cache file (required with --paths)")(
        KEY_CACHE_ENUMERATION, po::bool_switch(),
        "Cache the query graph enumeration after computing. "
        "Cannot be combined with --load-motif-graph-cache or --load-path-graph-cache.")(
        KEY_GRAPH_CACHE_DIR, po::value<std::string>(),
        "Directory where the query graph enumeration cache is written "
        "(required with --cache-enumeration).")(
        KEY_LOAD_MOTIF_GRAPH_CACHE, po::value<std::string>(),
        "Full path to an existing motif graph enumeration cache to load instead of computing. "
        "Cannot be combined with --cache-enumeration.")(
        KEY_LOAD_PATH_GRAPH_CACHE, po::value<std::string>(),
        "Full path to an existing path graph enumeration cache to load instead of computing. "
        "Cannot be combined with --cache-enumeration.");
    return desc;
}

po::variables_map SgfGraphEnumeratorArgumentParser::parse_raw(const int argc, char* argv[],
                                                              const po::options_description& desc)
{
    po::variables_map vm;
    try
    {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    }
    catch (const po::error& ex)
    {
        throw SgfInvalidArgumentException(ex.what());
    }
    return vm;
}

CliArgs SgfGraphEnumeratorArgumentParser::build_cli_args(const po::variables_map& vm)
{
    CliArgs result;
    result.m_run_preprocess = vm.at(KEY_PREPROCESS).as<bool>();
    result.m_run_filter = vm.at(KEY_FILTER).as<bool>();
    result.m_use_motifs = vm.at(KEY_MOTIFS).as<bool>();
    result.m_use_paths = vm.at(KEY_PATHS).as<bool>();
    result.m_is_directed = vm.at(KEY_IS_DIRECTED).as<bool>();
    if (result.m_run_preprocess)
    {
        result.m_preprocess = parse_preprocess_args(vm);
    }
    if (result.m_run_filter)
    {
        result.m_filter = parse_filter_args(vm);
    }
    return result;
}

PreprocessArgs SgfGraphEnumeratorArgumentParser::parse_preprocess_args(const po::variables_map& vm)
{
    PreprocessArgs result;
    result.m_library_dir = get_required_string(vm, KEY_LIBRARY_DIR);
    result.m_cache_dir = get_required_string(vm, KEY_CACHE_DIR);
    result.m_reader_type = parse_reader_type(get_required_string(vm, KEY_READER_TYPE));
    result.m_cache_type = parse_cache_type(get_required_string(vm, KEY_CACHE_TYPE));
    result.m_log_file_path = get_optional_string(vm, KEY_LOG_FILE_PATH);
    return result;
}

FilterArgs SgfGraphEnumeratorArgumentParser::parse_filter_args(const po::variables_map& vm)
{
    FilterArgs result;
    result.m_motif_cache_file = get_optional_string(vm, KEY_MOTIF_CACHE_FILE);
    result.m_path_cache_file = get_optional_string(vm, KEY_PATH_CACHE_FILE);
    result.m_cache_type = parse_cache_type(get_required_string(vm, KEY_CACHE_TYPE));
    result.m_graph_dir = get_required_string(vm, KEY_GRAPH_DIR);
    result.m_graph_input_type = parse_reader_type(get_required_string(vm, KEY_GRAPH_INPUT_TYPE));
    result.m_result_folder = get_required_string(vm, KEY_RESULT_FOLDER);
    result.m_result_type = parse_result_type(get_required_string(vm, KEY_RESULT_TYPE));
    result.m_log_file_path = get_optional_string(vm, KEY_LOG_FILE_PATH);
    result.m_cache_graph_enumeration = vm.at(KEY_CACHE_ENUMERATION).as<bool>();
    result.m_graph_cache_dir = get_optional_string(vm, KEY_GRAPH_CACHE_DIR);
    result.m_load_motif_graph_cache_path = get_optional_string(vm, KEY_LOAD_MOTIF_GRAPH_CACHE);
    result.m_load_path_graph_cache_path = get_optional_string(vm, KEY_LOAD_PATH_GRAPH_CACHE);
    return result;
}

void SgfGraphEnumeratorArgumentParser::validate_mode_flags(const CliArgs& args)
{
    validate_mode_selection(args);
    validate_feature_selection(args);
    if (args.m_run_filter)
    {
        validate_filter_args(args.m_filter, args.m_use_motifs, args.m_use_paths);
    }
}

void SgfGraphEnumeratorArgumentParser::validate_mode_selection(const CliArgs& args)
{
    if (args.m_run_preprocess && args.m_run_filter)
    {
        throw SgfInvalidArgumentException("--preprocess and --filter are mutually exclusive.");
    }
    if (!args.m_run_preprocess && !args.m_run_filter)
    {
        throw SgfInvalidArgumentException(
            "At least one of --preprocess or --filter must be specified.");
    }
}

void SgfGraphEnumeratorArgumentParser::validate_feature_selection(const CliArgs& args)
{
    if (!args.m_use_motifs && !args.m_use_paths)
    {
        throw SgfInvalidArgumentException("At least one of --motifs or --paths must be specified.");
    }
}

void SgfGraphEnumeratorArgumentParser::validate_filter_args(const FilterArgs& filter,
                                                            const bool use_motifs,
                                                            const bool use_paths)
{
    require_cache_file(use_motifs, filter.m_motif_cache_file, KEY_MOTIF_CACHE_FILE);
    require_cache_file(use_paths, filter.m_path_cache_file, KEY_PATH_CACHE_FILE);
    validate_cache_enumeration_flags(filter);
}

void SgfGraphEnumeratorArgumentParser::validate_cache_enumeration_flags(const FilterArgs& filter)
{
    if (!filter.m_cache_graph_enumeration)
    {
        return;
    }
    if (filter.m_graph_cache_dir.empty())
    {
        throw SgfInvalidArgumentException(
            "--graph-cache-dir is required when --cache-enumeration is set.");
    }
    if (!filter.m_load_motif_graph_cache_path.empty())
    {
        throw SgfInvalidArgumentException(
            "--cache-enumeration and --load-motif-graph-cache are mutually exclusive.");
    }
    if (!filter.m_load_path_graph_cache_path.empty())
    {
        throw SgfInvalidArgumentException(
            "--cache-enumeration and --load-path-graph-cache are mutually exclusive.");
    }
}

void SgfGraphEnumeratorArgumentParser::require_cache_file(const bool enabled,
                                                          const std::string& path,
                                                          const char* flag_name)
{
    if (enabled && path.empty())
    {
        throw SgfInvalidArgumentException(
            std::string("--") + flag_name +
            " is required when --filter and the feature flag are both set.");
    }
}

std::string SgfGraphEnumeratorArgumentParser::get_required_string(const po::variables_map& vm,
                                                                  const std::string& key)
{
    if (vm.count(key) == 0U)
    {
        throw SgfInvalidArgumentException("Required flag '--" + key + "' is missing.");
    }
    return vm.at(key).as<std::string>();
}

std::string SgfGraphEnumeratorArgumentParser::get_optional_string(const po::variables_map& vm,
                                                                  const std::string& key)
{
    if (vm.count(key) == 0U)
    {
        return {};
    }
    return vm.at(key).as<std::string>();
}

GraphReaderType SgfGraphEnumeratorArgumentParser::parse_reader_type(const std::string& type_str)
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

CacheManagerType SgfGraphEnumeratorArgumentParser::parse_cache_type(const std::string& type_str)
{
    if (type_str == CACHE_BINARY)
    {
        return CacheManagerType::BINARY;
    }
    if (type_str == CACHE_CSV)
    {
        return CacheManagerType::CSV;
    }
    throw SgfInvalidArgumentException("Unknown cache type '" + type_str +
                                      "'. Valid values: binary, csv.");
}

ResultOutputType SgfGraphEnumeratorArgumentParser::parse_result_type(const std::string& type_str)
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

}  // namespace sgf
