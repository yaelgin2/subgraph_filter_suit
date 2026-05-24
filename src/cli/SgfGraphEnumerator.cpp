#include "FlowManager.h"
#include "InvalidArgumentException.h"
#include "SgfException.h"

// NOLINTNEXTLINE(misc-include-cleaner)
#include <boost/program_options.hpp>

#include <cstdint>
#include <iostream>
#include <string>

using namespace sgf;

namespace
{

namespace po = boost::program_options;

// ── Option key constants ───────────────────────────────────────────────────

static constexpr const char* KEY_HELP = "help";
static constexpr const char* KEY_PREPROCESS = "preprocess";
static constexpr const char* KEY_FILTER = "filter";
static constexpr const char* KEY_MOTIFS = "motifs";
static constexpr const char* KEY_PATHS = "paths";
static constexpr const char* KEY_IS_DIRECTED = "is-directed";
static constexpr const char* KEY_LIBRARY_DIR = "library-dir";
static constexpr const char* KEY_READER_TYPE = "reader-type";
static constexpr const char* KEY_CACHE_PATH = "cache-path";
static constexpr const char* KEY_CACHE_TYPE = "cache-type";
static constexpr const char* KEY_LOG_FILE_PATH = "log-file-path";
static constexpr const char* KEY_GRAPH_DIR = "graph-dir";
static constexpr const char* KEY_GRAPH_INPUT_TYPE = "graph-input-type";
static constexpr const char* KEY_RESULT_FOLDER = "result-folder";
static constexpr const char* KEY_RESULT_TYPE = "result-type";

// ── Reader/cache/result type string constants ──────────────────────────────

static constexpr const char* READER_GRAPHML = "graphml";
static constexpr const char* READER_JSON = "json";
static constexpr const char* READER_VERTEX_EDGE = "vertex-edge";
static constexpr const char* CACHE_BINARY = "binary";
static constexpr const char* CACHE_CSV = "csv";
static constexpr const char* RESULT_JSON = "json";
static constexpr const char* RESULT_CSV = "csv";

// ── Argument data structures ───────────────────────────────────────────────

/**
 * @brief Arguments consumed by the preprocessing stage.
 */
struct PreprocessArgs
{
    std::string m_library_dir;                               ///< Graph library directory.
    GraphReaderType m_reader_type{GraphReaderType::GRAPHML}; ///< Graph file format.
    std::string m_cache_path;                                ///< Cache output directory.
    CacheManagerType m_cache_type{CacheManagerType::BINARY}; ///< Cache format.
    std::string m_log_file_path;                             ///< Optional log file path.
};

/**
 * @brief Arguments consumed by the filter stage.
 */
struct FilterArgs
{
    std::string m_cache_path;                                    ///< Cache input directory.
    CacheManagerType m_cache_type{CacheManagerType::BINARY};     ///< Cache format.
    std::string m_graph_dir;                                     ///< Query graphs directory.
    GraphReaderType m_graph_input_type{GraphReaderType::GRAPHML};///< Graph file format.
    std::string m_result_folder;                                 ///< Results output directory.
    ResultOutputType m_result_type{ResultOutputType::JSON};      ///< Results format.
    std::string m_log_file_path;                                 ///< Optional log file path.
};

/**
 * @brief All parsed CLI arguments.
 */
struct CliArgs
{
    bool m_run_preprocess{false}; ///< Run the preprocessing stage.
    bool m_run_filter{false};     ///< Run the filter stage.
    bool m_use_motifs{false};     ///< Enable motif-based processing.
    bool m_use_paths{false};      ///< Enable path-based processing.
    bool m_is_directed{false};    ///< Treat graphs as directed.
    PreprocessArgs m_preprocess{};///< Preprocessing-stage arguments.
    FilterArgs m_filter{};        ///< Filter-stage arguments.
};

// ── Options descriptor ─────────────────────────────────────────────────────

/**
 * @brief Builds the full Boost.PO options descriptor for all CLI flags.
 * @return Combined options_description covering all modes and features.
 */
po::options_description build_options()
{
    po::options_description mode_desc("Mode flags (at least one required)");
    mode_desc.add_options()
        ("help,h",       "Print this help message")
        (KEY_PREPROCESS, po::bool_switch(), "Run the enumeration preprocessing stage")
        (KEY_FILTER,     po::bool_switch(), "Run the enumeration filter stage");

    po::options_description feature_desc("Feature flags (at least one required)");
    feature_desc.add_options()
        (KEY_MOTIFS, po::bool_switch(), "Enable motif-based processing")
        (KEY_PATHS,  po::bool_switch(), "Enable path-based processing");

    po::options_description common_desc("Common flags");
    common_desc.add_options()
        (KEY_IS_DIRECTED,   po::bool_switch(),           "Treat graphs as directed")
        (KEY_CACHE_PATH,    po::value<std::string>(),    "Cache directory")
        (KEY_CACHE_TYPE,    po::value<std::string>(),    "Cache format: binary, csv")
        (KEY_LOG_FILE_PATH, po::value<std::string>(),    "(optional) Log file path");

    po::options_description preprocess_desc("Preprocess flags (required with --preprocess)");
    preprocess_desc.add_options()
        (KEY_LIBRARY_DIR, po::value<std::string>(), "Directory containing the graph library")
        (KEY_READER_TYPE, po::value<std::string>(), "Graph file format: graphml, vertex-edge, json");

    po::options_description filter_desc("Filter flags (required with --filter)");
    filter_desc.add_options()
        (KEY_GRAPH_DIR,        po::value<std::string>(), "Directory containing query graphs")
        (KEY_GRAPH_INPUT_TYPE, po::value<std::string>(), "Graph file format: graphml, vertex-edge, json")
        (KEY_RESULT_FOLDER,    po::value<std::string>(), "Directory for filter result output")
        (KEY_RESULT_TYPE,      po::value<std::string>(), "Result format: json, csv");

    po::options_description all_desc("sgf-graph-enumerator options");
    all_desc.add(mode_desc).add(feature_desc).add(common_desc)
            .add(preprocess_desc).add(filter_desc);
    return all_desc;
}

// ── Parsing helpers ────────────────────────────────────────────────────────

/**
 * @brief Parses argc/argv via Boost.PO, wrapping any parse error.
 * @param argc Argument count from main.
 * @param argv Argument values from main.
 * @param desc Options descriptor.
 * @return Populated variables_map.
 * @throws InvalidArgumentException on unknown option or bad syntax.
 */
po::variables_map parse_args(const int argc, char* argv[], const po::options_description& desc)
{
    po::variables_map vm;
    try
    {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    }
    catch (const po::error& ex)
    {
        throw InvalidArgumentException(ex.what());
    }
    return vm;
}

/**
 * @brief Returns the string value of a required option, throwing if absent.
 * @param vm Parsed variables map.
 * @param key Option key name.
 * @return Non-empty value string.
 * @throws InvalidArgumentException if the option was not provided.
 */
std::string get_required_string(const po::variables_map& vm, const std::string& key)
{
    if (vm.count(key) == 0U)
    {
        throw InvalidArgumentException("Required flag '--" + key + "' is missing.");
    }
    return vm.at(key).as<std::string>();
}

/**
 * @brief Returns the string value of an optional option, or empty string.
 * @param vm Parsed variables map.
 * @param key Option key name.
 * @return Value string, or empty if not provided.
 */
std::string get_optional_string(const po::variables_map& vm, const std::string& key)
{
    if (vm.count(key) == 0U)
    {
        return "";
    }
    return vm.at(key).as<std::string>();
}

// ── Type string parsers ────────────────────────────────────────────────────

/**
 * @brief Parses a graph reader type string.
 * @param type_str One of "graphml", "json", "vertex-edge".
 * @return Corresponding GraphReaderType.
 * @throws InvalidArgumentException for unrecognised values.
 */
GraphReaderType parse_reader_type(const std::string& type_str)
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
    throw InvalidArgumentException(
        "Unknown reader type '" + type_str + "'. Valid values: graphml, json, vertex-edge.");
}

/**
 * @brief Parses a cache type string.
 * @param type_str One of "binary", "csv".
 * @return Corresponding CacheManagerType.
 * @throws InvalidArgumentException for unrecognised values.
 */
CacheManagerType parse_cache_type(const std::string& type_str)
{
    if (type_str == CACHE_BINARY)
    {
        return CacheManagerType::BINARY;
    }
    if (type_str == CACHE_CSV)
    {
        return CacheManagerType::CSV;
    }
    throw InvalidArgumentException(
        "Unknown cache type '" + type_str + "'. Valid values: binary, csv.");
}

/**
 * @brief Parses a result output type string.
 * @param type_str One of "json", "csv".
 * @return Corresponding ResultOutputType.
 * @throws InvalidArgumentException for unrecognised values.
 */
ResultOutputType parse_result_type(const std::string& type_str)
{
    if (type_str == RESULT_JSON)
    {
        return ResultOutputType::JSON;
    }
    if (type_str == RESULT_CSV)
    {
        return ResultOutputType::CSV;
    }
    throw InvalidArgumentException(
        "Unknown result type '" + type_str + "'. Valid values: json, csv.");
}

// ── Stage arg parsers ──────────────────────────────────────────────────────

/**
 * @brief Populates preprocessing-stage args from the parsed variables map.
 * @param vm Parsed variables map.
 * @return Populated PreprocessArgs.
 * @throws InvalidArgumentException for missing required flags.
 */
PreprocessArgs parse_preprocess_args(const po::variables_map& vm)
{
    PreprocessArgs result;
    result.m_library_dir = get_required_string(vm, KEY_LIBRARY_DIR);
    result.m_cache_path  = get_required_string(vm, KEY_CACHE_PATH);
    result.m_reader_type = parse_reader_type(get_required_string(vm, KEY_READER_TYPE));
    result.m_cache_type  = parse_cache_type(get_required_string(vm, KEY_CACHE_TYPE));
    result.m_log_file_path = get_optional_string(vm, KEY_LOG_FILE_PATH);
    return result;
}

/**
 * @brief Populates filter-stage args from the parsed variables map.
 * @param vm Parsed variables map.
 * @return Populated FilterArgs.
 * @throws InvalidArgumentException for missing required flags.
 */
FilterArgs parse_filter_args(const po::variables_map& vm)
{
    FilterArgs result;
    result.m_cache_path       = get_required_string(vm, KEY_CACHE_PATH);
    result.m_cache_type       = parse_cache_type(get_required_string(vm, KEY_CACHE_TYPE));
    result.m_graph_dir        = get_required_string(vm, KEY_GRAPH_DIR);
    result.m_graph_input_type = parse_reader_type(get_required_string(vm, KEY_GRAPH_INPUT_TYPE));
    result.m_result_folder    = get_required_string(vm, KEY_RESULT_FOLDER);
    result.m_result_type      = parse_result_type(get_required_string(vm, KEY_RESULT_TYPE));
    result.m_log_file_path    = get_optional_string(vm, KEY_LOG_FILE_PATH);
    return result;
}

/**
 * @brief Parses all CLI arguments into a CliArgs struct.
 * @param vm Parsed variables map.
 * @return Populated CliArgs.
 * @throws InvalidArgumentException for missing required stage-specific flags.
 */
CliArgs parse_cli_args(const po::variables_map& vm)
{
    CliArgs result;
    result.m_run_preprocess = vm.at(KEY_PREPROCESS).as<bool>();
    result.m_run_filter     = vm.at(KEY_FILTER).as<bool>();
    result.m_use_motifs     = vm.at(KEY_MOTIFS).as<bool>();
    result.m_use_paths      = vm.at(KEY_PATHS).as<bool>();
    result.m_is_directed    = vm.at(KEY_IS_DIRECTED).as<bool>();
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

// ── Validation ─────────────────────────────────────────────────────────────

/**
 * @brief Validates that at least one mode and one feature flag are set.
 * @param args Parsed CLI arguments.
 * @throws InvalidArgumentException if validation fails.
 */
void validate_mode_flags(const CliArgs& args)
{
    if (!args.m_run_preprocess && !args.m_run_filter)
    {
        throw InvalidArgumentException(
            "At least one of --preprocess or --filter must be specified.");
    }
    if (!args.m_use_motifs && !args.m_use_paths)
    {
        throw InvalidArgumentException(
            "At least one of --motifs or --paths must be specified.");
    }
}

// ── Stage runners ──────────────────────────────────────────────────────────

/**
 * @brief Invokes the preprocessing stage via FlowManager.
 * @param args Validated CLI arguments.
 */
void run_preprocess(const CliArgs& args)
{
    std::string cache_path = args.m_preprocess.m_cache_path;
    FlowManager::enumerator_preprocess_run(
        args.m_preprocess.m_library_dir,
        args.m_is_directed,
        args.m_preprocess.m_reader_type,
        cache_path,
        args.m_preprocess.m_cache_type,
        args.m_preprocess.m_log_file_path,
        args.m_use_paths,
        args.m_use_motifs);
}

/**
 * @brief Invokes the filter stage via FlowManager.
 * @param args Validated CLI arguments.
 */
void run_filter(const CliArgs& args)
{
    std::string result_folder = args.m_filter.m_result_folder;
    FlowManager::enumerator_filter_run(
        args.m_filter.m_graph_dir,
        args.m_is_directed,
        args.m_filter.m_graph_input_type,
        args.m_filter.m_cache_path,
        args.m_filter.m_cache_type,
        result_folder,
        args.m_filter.m_result_type,
        args.m_filter.m_log_file_path,
        args.m_use_paths,
        args.m_use_motifs);
}

/**
 * @brief Runs the full pipeline; returns a process exit code.
 * @param vm Parsed variables map.
 * @return 0 on success, non-zero on failure.
 */
int run_pipeline(const po::variables_map& vm)
{
    try
    {
        const CliArgs cli_args = parse_cli_args(vm);
        validate_mode_flags(cli_args);
        if (cli_args.m_run_preprocess)
        {
            run_preprocess(cli_args);
        }
        if (cli_args.m_run_filter)
        {
            run_filter(cli_args);
        }
    }
    catch (const SgfException& ex)
    {
        std::cerr << "Error: " << ex.what() << '\n';
        return static_cast<int>(static_cast<int32_t>(ex.return_code()));
    }
    return 0;
}

}  // namespace

// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, char* argv[])
{
    const po::options_description desc = build_options();
    po::variables_map vm;
    try
    {
        vm = parse_args(argc, argv, desc);
    }
    catch (const SgfException& ex)
    {
        std::cerr << "Error: " << ex.what() << '\n';
        return static_cast<int>(static_cast<int32_t>(ex.return_code()));
    }
    const bool show_help = (vm.count(KEY_HELP) != 0U) || (argc == 1);
    if (show_help)
    {
        std::cout << desc << '\n';
        return 0;
    }
    return run_pipeline(vm);
}
