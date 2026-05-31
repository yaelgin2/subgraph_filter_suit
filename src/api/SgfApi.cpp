#include "SgfApi.h"

#include "EnumerationPreprocessManager.h"
#include "FilteringUtils.h"
#include "FlowManager.h"
#include "InvalidArgumentException.h"
#include "IPatternPreprocessor.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace sgf
{

namespace
{

/**
 * @brief Throw InvalidArgumentException if @p value is empty.
 * @param value The string to check.
 * @param field Human-readable field name for the error message.
 * @throws InvalidArgumentException if @p value is empty.
 */
void require_non_empty(const std::string& value, const char* field)
{
    if (value.empty())
    {
        throw InvalidArgumentException(std::string("required field '") + field + "' is empty");
    }
}

/**
 * @brief Return the string inside @p opt, or an empty string when absent.
 * @param opt Optional string.
 * @return Contained value or "".
 */
std::string optional_or_empty(const std::optional<std::string>& opt)
{
    return opt.has_value() ? *opt : std::string{};
}

}  // namespace

std::vector<EnumerationResultVector> enumerate_library(const EnumerateLibraryParams& params)
{
    require_non_empty(params.m_library_path, "m_library_path");
    require_non_empty(params.m_output_path, "m_output_path");
    if (!params.m_preprocess_motifs && !params.m_preprocess_paths)
    {
        throw InvalidArgumentException(
            "at least one of m_preprocess_motifs or m_preprocess_paths must be true");
    }
    std::string output_path = params.m_output_path;
    return FlowManager::enumerator_preprocess_run(
        params.m_library_path, params.m_is_directed, params.m_reader_type, output_path,
        params.m_cache_type, optional_or_empty(params.m_log_file), params.m_preprocess_paths,
        params.m_preprocess_motifs);
}

std::vector<std::unordered_map<std::string, FilterResult>>
filter_with_enumeration(const FilterWithEnumerationParams& params)
{
    require_non_empty(params.m_query_graph_path, "m_query_graph_path");
    require_non_empty(params.m_output_folder, "m_output_folder");
    if (!params.m_filter_motifs && !params.m_filter_paths)
    {
        throw InvalidArgumentException(
            "at least one of m_filter_motifs or m_filter_paths must be true");
    }
    if (params.m_filter_motifs && !params.m_motif_cache_file.has_value())
    {
        throw InvalidArgumentException(
            "m_motif_cache_file is required when m_filter_motifs is true");
    }
    if (params.m_filter_paths && !params.m_path_cache_file.has_value())
    {
        throw InvalidArgumentException("m_path_cache_file is required when m_filter_paths is true");
    }
    std::string output_folder = params.m_output_folder;
    return FlowManager::enumerator_filter_run(
        params.m_query_graph_path, params.m_is_directed, params.m_reader_type,
        optional_or_empty(params.m_motif_cache_file), optional_or_empty(params.m_path_cache_file),
        params.m_cache_type, output_folder, params.m_result_type,
        optional_or_empty(params.m_log_file), params.m_filter_paths, params.m_filter_motifs,
        params.m_cache_config, params.m_non_induced);
}

std::vector<PatternPreprocessorResult> preprocess_patterns(const PreprocessPatternsParams& params)
{
    require_non_empty(params.m_library_path, "m_library_path");
    require_non_empty(params.m_output_path, "m_output_path");
    const bool use_single_index = params.m_single_graph_index.has_value();
    const bool use_results_file = params.m_results_file_path.has_value();
    if (use_single_index && use_results_file)
    {
        throw InvalidArgumentException(
            "m_single_graph_index and m_results_file_path are mutually exclusive");
    }
    const bool single_graph_mode = use_single_index || use_results_file;
    if (single_graph_mode && !params.m_background_graph_path.has_value())
    {
        throw InvalidArgumentException("m_background_graph_path is required in single-graph mode");
    }
    std::string output_path = params.m_output_path;
    const int64_t single_graph_index =
        use_single_index ? *params.m_single_graph_index : static_cast<int64_t>(-1);
    return FlowManager::pattern_preprocess_run(
        params.m_library_path, params.m_is_directed, params.m_reader_type, output_path,
        params.m_pattern_type, optional_or_empty(params.m_log_file), use_results_file,
        optional_or_empty(params.m_results_file_path), single_graph_index,
        params.m_results_file_type, optional_or_empty(params.m_background_graph_path),
        params.m_score_threshold, params.m_finder_config);
}

std::vector<std::unordered_map<std::string, FilterResult>>
filter_with_patterns(const FilterWithPatternsParams& params)
{
    require_non_empty(params.m_pattern_cache_path, "m_pattern_cache_path");
    require_non_empty(params.m_background_graph_folder, "m_background_graph_folder");
    require_non_empty(params.m_output_path, "m_output_path");
    std::string output_path = params.m_output_path;
    return FlowManager::pattern_filter_run(
        params.m_pattern_cache_path, params.m_pattern_type, params.m_background_graph_folder,
        params.m_reader_type, params.m_is_directed, output_path, params.m_result_type,
        optional_or_empty(params.m_log_file), params.m_prior_policy, params.m_is_induced);
}

uint64_t find_subgraph(const FindSubgraphParams& params)
{
    require_non_empty(params.m_subgraph_path, "m_subgraph_path");
    require_non_empty(params.m_background_path, "m_background_path");
    std::string output_path = optional_or_empty(params.m_output_path);
    const bool has_output = params.m_output_path.has_value();
    return FlowManager::subgraph_isomorphism_run(
        params.m_subgraph_path, params.m_background_path, params.m_reader_type, has_output,
        output_path, params.m_is_directed, params.m_is_induced, params.m_prior_policy,
        params.m_stop_on_first_match, optional_or_empty(params.m_log_file));
}

}  // namespace sgf
