#include "FlowManager.h"
#include "SgfException.h"
#include "SgfGraphSearcherArgumentParser.h"

#include <cstdint>
#include <iostream>
#include <optional>
#include <string>

namespace sgf
{

/**
 * @brief Invokes the subgraph isomorphism stage via FlowManager.
 * @param args Validated CLI arguments.
 * @return Number of matches found.
 */
uint64_t run_searcher(const GraphSearcherArgs& args)
{
    const bool is_output = !args.m_output_path.empty();
    std::string output_path = args.m_output_path;
    return FlowManager::subgraph_isomorphism_run(
        args.m_subgraph_path, args.m_background_path, args.m_reader_type, is_output, output_path,
        args.m_is_directed, args.m_is_induced, args.m_prior_policy, args.m_stop_on_first_match,
        args.m_log_file_path);
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
        const std::optional<GraphSearcherArgs> args =
            SgfGraphSearcherArgumentParser::parse(argc, argv);
        if (!args.has_value())
        {
            return 0;
        }
        const uint64_t match_count = run_searcher(*args);
        std::cout << "Matches found: " << match_count << '\n';
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
