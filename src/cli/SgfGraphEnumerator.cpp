#include "FlowManager.h"
#include "SgfException.h"
#include "SgfGraphEnumeratorArgumentParser.h"

#include <cstdint>
#include <iostream>
#include <optional>
#include <string>

namespace sgf
{

/**
 * @brief Invokes the preprocessing stage via FlowManager.
 * @param args Validated CLI arguments.
 */
void run_preprocess(const CliArgs& args)
{
    std::string cache_dir = args.m_preprocess.m_cache_dir;
    FlowManager::enumerator_preprocess_run(
        args.m_preprocess.m_library_dir, args.m_is_directed, args.m_preprocess.m_reader_type,
        cache_dir, args.m_preprocess.m_cache_type, args.m_preprocess.m_log_file_path,
        args.m_use_paths, args.m_use_motifs, args.m_thread_number,
        args.m_preprocess.m_graphml_color_map_path);
}

/**
 * @brief Invokes the filter stage via FlowManager.
 * @param args Validated CLI arguments.
 */
void run_filter(const CliArgs& args)
{
    std::string result_folder = args.m_filter.m_result_folder;
    const GraphEnumerationCacheConfig cache_config{
        args.m_filter.m_cache_graph_enumeration, args.m_filter.m_graph_cache_dir,
        args.m_filter.m_load_motif_graph_cache_path, args.m_filter.m_load_path_graph_cache_path};
    FlowManager::enumerator_filter_run(
        args.m_filter.m_graph_dir, args.m_is_directed, args.m_filter.m_graph_input_type,
        args.m_filter.m_motif_cache_file, args.m_filter.m_path_cache_file,
        args.m_filter.m_cache_type, result_folder, args.m_filter.m_result_type,
        args.m_filter.m_log_file_path, args.m_use_paths, args.m_use_motifs, cache_config,
        args.m_non_induced, args.m_thread_number, args.m_filter.m_graphml_color_map_path);
}

/**
 * @brief Runs the full pipeline; returns a process exit code.
 * @param argc Argument count from main.
 * @param argv Argument values from main.
 * @return 0 on success, non-zero on failure.
 */
// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
int run_pipeline(const int argc, char* argv[])
{
    try
    {
        const std::optional<CliArgs> cli_args = SgfGraphEnumeratorArgumentParser::parse(argc, argv);
        if (!cli_args.has_value())
        {
            return 0;
        }
        if (cli_args->m_run_preprocess)
        {
            run_preprocess(*cli_args);
        }
        if (cli_args->m_run_filter)
        {
            run_filter(*cli_args);
        }
    }
    catch (const SgfException& ex)
    {
        std::cerr << "Error: " << ex.what() << '\n';
        return static_cast<int>(static_cast<int32_t>(ex.return_code()));
    }
    return 0;
}

}  // namespace sgf

// NOLINTNEXTLINE(bugprone-exception-escape,cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
int main(const int argc, char* argv[])
{
    return sgf::run_pipeline(argc, argv);
}
