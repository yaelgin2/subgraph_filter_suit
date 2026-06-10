#include "FlowManager.h"
#include "SgfException.h"
#include "SgfPatternFinderArgumentParser.h"

#include <cstdint>
#include <iostream>
#include <optional>
#include <string>

namespace sgf
{

/**
 * @brief Invokes the pattern preprocessing stage via FlowManager.
 * @param args Validated CLI arguments.
 */
void run_preprocess(const PatternFinderCliArgs& args)
{
    std::string output_folder = args.m_preprocess.m_output_folder;
    FlowManager::pattern_preprocess_run(
        args.m_preprocess.m_library_input_folder, args.m_is_directed, args.m_reader_type,
        output_folder, args.m_preprocess.m_pattern_output_type, args.m_log_file_path,
        args.m_preprocess.m_preprocess_single_graph_from_results,
        args.m_preprocess.m_results_file_path, args.m_preprocess.m_preprocess_single_graph,
        args.m_preprocess.m_results_file_type, args.m_preprocess.m_background_graph_path,
        args.m_preprocess.m_score_threshold, args.m_preprocess.m_config,
        args.m_preprocess.m_preprocess_multigraph, args.m_preprocess.m_multigraph_alive_percent);
}

/**
 * @brief Invokes the pattern filter stage via FlowManager.
 * @param args Validated CLI arguments.
 */
void run_filter(const PatternFinderCliArgs& args)
{
    std::string output_path = args.m_filter.m_output_path;
    FlowManager::pattern_filter_run(
        args.m_filter.m_pattern_mapping_cache, args.m_filter.m_pattern_type,
        args.m_filter.m_background_graph_folder, args.m_reader_type, args.m_is_directed,
        output_path, args.m_filter.m_output_type, args.m_log_file_path,
        args.m_filter.m_prior_policy, args.m_filter.m_is_induced);
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
        const std::optional<PatternFinderCliArgs> cli_args =
            SgfPatternFinderArgumentParser::parse(argc, argv);
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
