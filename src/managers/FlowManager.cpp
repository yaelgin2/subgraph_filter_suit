#include "FlowManager.h"

#include "BinaryCacheIOManager.h"
#include "CSVCacheIOManager.h"
#include "CSVFilterIOManager.h"
#include "CSVPatternCacheIOManager.h"
#include "ColoredGraph.h"
#include "EnumerationPreprocessManager.h"
#include "FilteringUtils.h"
#include "GraphmlColorMapIOManager.h"
#include "GraphmlGraphReader.h"
#include "GraphmlPatternWriter.h"
#include "GroupEnumerationGraphFilter.h"
#include "IColoredGraphReader.h"
#include "IFilterIOManager.h"
#include "IGraphPreprocessor.h"
#include "IOConstants.h"
#include "IOUtils.h"
#include "IPatternCacheIOManager.h"
#include "IPatternPreprocessor.h"
#include "IPatternWriter.h"
#include "InvalidArgumentException.h"
#include "JsonFilterIOManager.h"
#include "JsonGraphReader.h"
#include "JsonPatternWriter.h"
#include "LogLevel.h"
#include "LoggerBundle.h"
#include "LoggerHandler.h"
#include "MatchOutputWriter.h"
#include "MotifDagExpander.h"
#include "MotifPreprocessor.h"
#include "MultiGraphPatternPreprocessor.h"
#include "PathProcessor.h"
#include "PatternGraphFilter.h"
#include "PatternPreprocessManager.h"
#include "PriorPolicy.h"
#include "SingleGraphPatternPreprocessor.h"
#include "SubgraphSearcher.h"
#include "VertexEdgeGraphReader.h"
#include "VertexEdgePatternWriter.h"

#include <ICacheIOManager.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sgf
{

/* ---------- Factory methods ---------- */

std::unique_ptr<IColoredGraphReader> FlowManager::make_graph_reader(const GraphReaderType type)
{
    switch (type)
    {
    case GraphReaderType::GRAPHML:
        return std::make_unique<GraphmlGraphReader>();
    case GraphReaderType::JSON:
        return std::make_unique<JsonGraphReader>();
    case GraphReaderType::VERTEX_EDGE:
        return std::make_unique<VertexEdgeGraphReader>();
    default:
        throw InvalidArgumentException("Unknown GraphReaderType.");
    }
}

std::shared_ptr<IPatternWriter> FlowManager::make_pattern_writer(const PatternWriterType type)
{
    switch (type)
    {
    case PatternWriterType::GRAPHML:
        return std::make_shared<GraphmlPatternWriter>();
    case PatternWriterType::JSON:
        return std::make_shared<JsonPatternWriter>();
    case PatternWriterType::VERTEX_EDGE:
        return std::make_shared<VertexEdgePatternWriter>();
    default:
        throw InvalidArgumentException("Unknown PatternWriterType.");
    }
}

GraphReaderType
FlowManager::pattern_witer_type_to_graph_reader_type(const PatternWriterType pattern_writer_type)
{
    switch (pattern_writer_type)
    {
    case PatternWriterType::GRAPHML:
        return GraphReaderType::GRAPHML;
    case PatternWriterType::JSON:
        return GraphReaderType::JSON;
    case PatternWriterType::VERTEX_EDGE:
        return GraphReaderType::VERTEX_EDGE;
    default:
        throw InvalidArgumentException("Unknown PatternWriterType.");
    }
}

std::shared_ptr<ICacheIOManager> FlowManager::make_cache_manager(const CacheManagerType type,
                                                                 const std::string& folder,
                                                                 LoggerHandler logger)
{
    switch (type)
    {
    case CacheManagerType::BINARY:
        return std::make_shared<BinaryCacheIOManager>(folder, std::move(logger));
    case CacheManagerType::CSV:
        return std::make_shared<CSVCacheIOManager>(folder, std::move(logger));
    default:
        throw InvalidArgumentException("Unknown CacheManagerType.");
    }
}

std::unique_ptr<IFilterIOManager>
FlowManager::make_filter_results_io_manager(const ResultOutputType type, const std::string& folder,
                                            LoggerHandler logger)
{
    switch (type)
    {
    case ResultOutputType::JSON:
        return std::make_unique<JsonFilterIOManager>(folder, std::move(logger));
    case ResultOutputType::CSV:
        return std::make_unique<CSVFilterIOManager>(folder, std::move(logger));
    default:
        throw InvalidArgumentException("Unknown CacheManagerType.");
    }
}

/* ---------- Pipeline stages ---------- */

// NOLINTNEXTLINE(readability-function-size)
std::vector<EnumerationResultVector> FlowManager::enumerator_preprocess_run(
    const std::string& input_path, const bool is_directed, const GraphReaderType reader_type,
    std::string& output_path, CacheManagerType output_type, const std::string& log_file_path,
    bool preprocess_paths, bool preprocess_motifs, const uint32_t thread_number,
    const std::string& graphml_color_map_path, const bool use_gpu)
{
    std::vector<EnumerationResultVector> result;
    const LoggerBundle log_bundle(log_file_path);
    const std::string color_map_save_folder = graphml_color_map_path.empty() ? "" : output_path;
    const LibraryData library =
        load_library(input_path, reader_type, is_directed, log_bundle.handler(),
                     ColorMapConfig{graphml_color_map_path, color_map_save_folder});
    EnumerationPreprocessManager preprocess_manager(library.m_library, log_bundle.handler());
    const std::shared_ptr<ICacheIOManager> cache_manager =
        make_cache_manager(output_type, output_path, log_bundle.handler());
    if (preprocess_paths)
    {
        result.push_back(get_graph_enumeration(
            true, std::string(PATH_CACHE_BASE_NAME), cache_manager, preprocess_manager, library,
            [thread_number](const ColoredGraph& graph,
                            const LoggerHandler& logger) -> std::unique_ptr<IGraphPreprocessor>
            {
                return std::make_unique<PathProcessor>(graph, logger, thread_number);
            },
            log_bundle.handler(), use_gpu));
    }
    if (preprocess_motifs)
    {
        result.push_back(get_graph_enumeration(
            true, std::string(MOTIF_CACHE_BASE_NAME), cache_manager, preprocess_manager, library,
            [thread_number](const ColoredGraph& graph,
                            const LoggerHandler& logger) -> std::unique_ptr<IGraphPreprocessor>
            {
                return std::make_unique<MotifPreprocessor>(graph, logger, thread_number);
            },
            log_bundle.handler(), use_gpu));
    }
    return result;
}

// NOLINTNEXTLINE(readability-function-size)
std::unordered_map<std::string, FilterResult> FlowManager::enumerate_and_filter(
    const std::string& library_cache_dir, const bool load_graph_cache,
    const std::string& graphs_cache_dir, const std::string& run_type_file_base_name,
    const PreprocessorFactory& factory, const CacheManagerType cache_reader_type,
    const std::shared_ptr<ICacheIOManager>& graphs_cache_manager, LibraryData& graphs_to_find_in,
    const std::unique_ptr<EnumerationPreprocessManager>& preprocess_manager,
    IFilterIOManager& filter_results_writer, const std::string& timestamp,
    const LoggerHandler& logger, const bool use_gpu, const EnumerationTransformer& post_process)
{
    EnumerationResultVector graph_enumeration;
    if (load_graph_cache)
    {
        std::tie(graphs_to_find_in.m_graph_names, graph_enumeration) = load_graph_enumeration(
            cache_reader_type, graphs_cache_dir, run_type_file_base_name, logger);
    }
    else
    {
        graph_enumeration = get_graph_enumeration(
            graphs_cache_manager != nullptr, run_type_file_base_name, graphs_cache_manager,
            *preprocess_manager, graphs_to_find_in, factory, logger, use_gpu);
    }
    post_process(graph_enumeration);
    const std::shared_ptr<ICacheIOManager> lib_cache_manager =
        make_cache_manager(cache_reader_type, library_cache_dir, logger);
    return run_enumeration_filter_stage(run_type_file_base_name, graph_enumeration,
                                        *lib_cache_manager, filter_results_writer,
                                        graphs_to_find_in, timestamp, logger);
}

// NOLINTNEXTLINE(readability-function-size)
std::vector<std::unordered_map<std::string, FilterResult>> FlowManager::enumerator_filter_run(
    const std::string& graph_input_path, const bool is_directed, const GraphReaderType reader_type,
    const std::string& motif_cache_dir, const std::string& path_cache_dir,
    const CacheManagerType cache_reader_type, std::string& output_folder,
    ResultOutputType output_type, const std::string& log_file_path, bool filter_paths,
    bool filter_motifs, const GraphEnumerationCacheConfig& graph_cache_config,
    const bool non_induced, const uint32_t thread_number, const std::string& graphml_color_map_path,
    const bool use_gpu)
{
    const LoggerBundle log_bundle(log_file_path);
    const std::string timestamp = generate_timestamp();
    LibraryData graphs_to_find_in;
    std::unique_ptr<EnumerationPreprocessManager> preprocess_manager = nullptr;
    const bool need_path_compute =
        filter_paths && graph_cache_config.m_graphs_path_cache_dir.empty();
    const bool need_motif_compute =
        filter_motifs && graph_cache_config.m_graphs_motif_cache_dir.empty();
    if (need_path_compute || need_motif_compute)
    {
        const std::string color_map_save_folder =
            graphml_color_map_path.empty() ? "" : output_folder;
        graphs_to_find_in =
            load_library(graph_input_path, reader_type, is_directed, log_bundle.handler(),
                         ColorMapConfig{graphml_color_map_path, color_map_save_folder});
        preprocess_manager = std::make_unique<EnumerationPreprocessManager>(
            graphs_to_find_in.m_library, log_bundle.handler());
    }
    std::unique_ptr<IFilterIOManager> filter_results_writer =
        make_filter_results_io_manager(output_type, output_folder, log_bundle.handler());
    std::shared_ptr<ICacheIOManager> graphs_cache_manager = nullptr;
    if (graph_cache_config.m_cache_enumeration)
    {
        graphs_cache_manager = make_cache_manager(
            cache_reader_type, graph_cache_config.m_graph_cache_dir, log_bundle.handler());
    }
    std::vector<std::unordered_map<std::string, FilterResult>> filter_results;
    const EnumerationTransformer no_op = [](EnumerationResultVector&)
    {
    };
    const MotifDagExpander::GraphType dag_type = is_directed
                                                     ? MotifDagExpander::GraphType::DIRECTED
                                                     : MotifDagExpander::GraphType::UNDIRECTED;
    if (filter_paths)
    {

        filter_results.push_back(enumerate_and_filter(
            path_cache_dir, !graph_cache_config.m_graphs_path_cache_dir.empty(),
            graph_cache_config.m_graphs_path_cache_dir, std::string(PATH_CACHE_BASE_NAME),
            [thread_number](const ColoredGraph& graph,
                            const LoggerHandler& logger) -> std::unique_ptr<IGraphPreprocessor>
            {
                return std::make_unique<PathProcessor>(graph, logger, thread_number);
            },
            cache_reader_type, graphs_cache_manager, graphs_to_find_in, preprocess_manager,
            *filter_results_writer, timestamp, log_bundle.handler(), use_gpu, no_op));
    }
    if (filter_motifs)
    {
        const EnumerationTransformer motif_transform =
            non_induced ? EnumerationTransformer(
                              [dag_type](EnumerationResultVector& enumeration)
                              {
                                  const MotifDagExpander expander(dag_type);
                                  for (auto& result : enumeration)
                                  {
                                      result = expander.expand(std::move(result));
                                  }
                              })
                        : no_op;
        filter_results.push_back(enumerate_and_filter(
            motif_cache_dir, !graph_cache_config.m_graphs_motif_cache_dir.empty(),
            graph_cache_config.m_graphs_motif_cache_dir, std::string(MOTIF_CACHE_BASE_NAME),
            [thread_number](const ColoredGraph& graph,
                            const LoggerHandler& logger) -> std::unique_ptr<IGraphPreprocessor>
            {
                return std::make_unique<MotifPreprocessor>(graph, logger, thread_number);
            },
            cache_reader_type, graphs_cache_manager, graphs_to_find_in, preprocess_manager,
            *filter_results_writer, timestamp, log_bundle.handler(), use_gpu, motif_transform));
    }
    return filter_results;
}

// NOLINTNEXTLINE(readability-function-size)
std::vector<PatternPreprocessorResult> FlowManager::pattern_preprocess_run(
    const std::string& input_path, const bool is_directed, GraphReaderType reader_type,
    std::string& output_path, const PatternWriterType output_type, const std::string& log_file_path,
    const bool preprocess_singlegraph_results_file, const std::string& results_file_path,
    const int64_t preprocess_singlegraph, const ResultOutputType results_file_type,
    const std::string& background_graph_path, const double score_threshold,
    const SingleGraphFinderConfig& config, const uint32_t preprocess_multigraph,
    const double multigraph_alive_percent, const uint32_t thread_number,
    const std::string& graphml_color_map_path)
{
    std::vector<PatternPreprocessorResult> result;
    const LoggerBundle log_bundle(log_file_path);
    const std::string timestamp = generate_timestamp();
    const std::string color_map_save_folder = graphml_color_map_path.empty() ? "" : output_path;
    const LibraryData library =
        load_library(input_path, reader_type, is_directed, log_bundle.handler(),
                     ColorMapConfig{graphml_color_map_path, color_map_save_folder});
    const std::shared_ptr<IPatternWriter> pattern_writer = make_pattern_writer(output_type);
    PatternPreprocessManager preprocess_manager(library.m_library, log_bundle.handler());
    if (preprocess_multigraph > 0U)
    {
        const PatternOutput multigraph_results = preprocess_manager.preprocess(
            [preprocess_multigraph, multigraph_alive_percent, is_directed,
             thread_number](std::vector<ColoredGraph>& library_ref,
                            LoggerHandler logger) -> std::unique_ptr<IPatternPreprocessor>
            {
                return std::make_unique<MultiGraphPatternPreprocessor>(
                    library_ref, is_directed, preprocess_multigraph, multigraph_alive_percent,
                    thread_number, std::move(logger));
            });
        result.insert(result.end(), multigraph_results.begin(), multigraph_results.end());
        const CSVPatternCacheIOManager cache_manager(output_path, log_bundle.handler());
        cache_manager.write(multigraph_results, timestamp, pattern_writer, is_directed);
    }
    const bool need_background =
        preprocess_singlegraph != -1 || preprocess_singlegraph_results_file;
    std::optional<ColoredGraph> background_graph_opt;
    if (need_background)
    {
        const std::unique_ptr<IColoredGraphReader> reader = make_graph_reader(reader_type);
        background_graph_opt =
            reader->read(background_graph_path, is_directed, log_bundle.handler());
    }
    if (preprocess_singlegraph != -1 && background_graph_opt.has_value())
    {
        std::vector<bool> to_process(library.m_library.size(), false);
        to_process[static_cast<size_t>(preprocess_singlegraph)] = true;
        const ColoredGraph& background = *background_graph_opt;
        SingleGraphFinderConfig config_with_threads = config;
        config_with_threads.m_thread_number = thread_number;
        const PatternOutput single_graph_results = preprocess_manager.preprocess(
            [is_directed, &background, &to_process, score_threshold,
             &config_with_threads](std::vector<ColoredGraph>& library_ref,
                                   LoggerHandler logger) -> std::unique_ptr<IPatternPreprocessor>
            {
                return std::make_unique<SingleGraphPatternPreprocessor>(
                    library_ref, is_directed, background, to_process, score_threshold,
                    config_with_threads, std::move(logger));
            });
        result.insert(result.end(), single_graph_results.begin(), single_graph_results.end());
        const CSVPatternCacheIOManager cache_manager(output_path, log_bundle.handler());
        cache_manager.write(single_graph_results, timestamp, pattern_writer, is_directed);
    }
    if (preprocess_singlegraph_results_file && background_graph_opt.has_value())
    {
        const std::filesystem::path results_path(results_file_path);
        const std::unique_ptr<IFilterIOManager> result_reader = make_filter_results_io_manager(
            results_file_type, results_path.parent_path().string(), log_bundle.handler());
        const std::unordered_map<std::string, bool> filter_map =
            result_reader->read(results_path.stem().string());
        std::vector<bool> to_process = build_to_process(library.m_graph_names, filter_map);
        const ColoredGraph& background = *background_graph_opt;
        SingleGraphFinderConfig results_config_with_threads = config;
        results_config_with_threads.m_thread_number = thread_number;
        const PatternOutput single_graph_results = preprocess_manager.preprocess(
            [is_directed, &background, &to_process, score_threshold, &results_config_with_threads](
                std::vector<ColoredGraph>& library_ref,
                LoggerHandler logger) -> std::unique_ptr<IPatternPreprocessor>
            {
                return std::make_unique<SingleGraphPatternPreprocessor>(
                    library_ref, is_directed, background, to_process, score_threshold,
                    results_config_with_threads, std::move(logger));
            });
        const CSVPatternCacheIOManager cache_manager(output_path, log_bundle.handler());
        cache_manager.write(single_graph_results, timestamp, pattern_writer, is_directed);
        result.insert(result.end(), single_graph_results.begin(), single_graph_results.end());
    }
    return result;
}

// NOLINTNEXTLINE(readability-function-size)
std::vector<std::unordered_map<std::string, FilterResult>> FlowManager::pattern_filter_run(
    const std::string& pattern_to_filter_cache, const PatternWriterType pattern_type,
    const std::string& background_graph_path, const GraphReaderType reader_type,
    const bool is_directed, std::string& output_path, const ResultOutputType output_type,
    const std::string& log_file_path, const PriorPolicy prior_policy, const bool is_induced)
{
    const LoggerBundle log_bundle(log_file_path);
    const std::filesystem::path cache_path_obj(pattern_to_filter_cache);
    const std::string cache_folder = cache_path_obj.parent_path().string();
    const CSVPatternCacheIOManager cache_manager(cache_folder, log_bundle.handler());
    const PatternMapping pattern_mapping = cache_manager.read(pattern_to_filter_cache);
    const std::unique_ptr<IColoredGraphReader> pattern_reader =
        make_graph_reader(pattern_witer_type_to_graph_reader_type(pattern_type));
    std::vector<ColoredGraphPatternResult> library_cache;
    std::vector<std::string> pattern_filenames;
    library_cache.reserve(pattern_mapping.size());
    pattern_filenames.reserve(pattern_mapping.size());
    for (const auto& [filename, indices] : pattern_mapping)
    {
        const std::string file_path = (std::filesystem::path(cache_folder) / filename).string();
        library_cache.emplace_back(
            pattern_reader->read(file_path, is_directed, log_bundle.handler()), indices);
        pattern_filenames.push_back(filename);
    }
    const PatternGraphFilter pattern_filter(std::move(library_cache), log_bundle.handler());
    const std::unique_ptr<IColoredGraphReader> graph_reader = make_graph_reader(reader_type);
    std::unique_ptr<IFilterIOManager> filter_results_writer =
        make_filter_results_io_manager(output_type, output_path, log_bundle.handler());
    const std::string timestamp = generate_timestamp();
    const std::vector<std::string> bg_files =
        std::filesystem::is_directory(background_graph_path)
            ? IOUtils::get_files_in_directory(background_graph_path)
            : std::vector<std::string>{background_graph_path};
    std::vector<std::unordered_map<std::string, FilterResult>> results;
    for (const std::string& bg_file : bg_files)
    {
        const ColoredGraph background =
            graph_reader->read(bg_file, is_directed, log_bundle.handler());
        const FilterResult filter_result =
            pattern_filter.filter(background, is_induced, prior_policy);
        const std::string background_stem = std::filesystem::path(bg_file).stem().string();
        std::string result_filename = background_stem;
        result_filename += PATTERN_FILTER_RESULT_SUFFIX;
        result_filename += timestamp;
        filter_results_writer->write(result_filename, pattern_filenames, filter_result);
        results.push_back(
            std::unordered_map<std::string, FilterResult>{{background_stem, filter_result}});
    }
    return results;
}

// NOLINTNEXTLINE(readability-function-size)
uint64_t FlowManager::subgraph_isomorphism_run(
    const std::string& subgraph_path, const std::string& background_graph_path,
    GraphReaderType reader_type, bool is_output, std::string& output_path, bool is_directed,
    bool is_induced, PriorPolicy policy, bool stop_on_first_match, const std::string& log_file_path)
{
    const LoggerBundle log_bundle(log_file_path);
    const std::unique_ptr<IColoredGraphReader> reader = make_graph_reader(reader_type);
    const ColoredGraph subgraph = reader->read(subgraph_path, is_directed, log_bundle.handler());
    const ColoredGraph background =
        reader->read(background_graph_path, is_directed, log_bundle.handler());
    std::unique_ptr<MatchOutputWriter> match_writer = nullptr;
    if (is_output)
    {
        match_writer = std::make_unique<MatchOutputWriter>(output_path);
    }
    const SubgraphSearcher seracher(policy, is_directed, is_induced, std::move(match_writer),
                                    log_bundle.handler());
    const uint64_t match_count = seracher.find_all(background, subgraph, stop_on_first_match);
    return match_count;
}

/* ---------- Private helpers ---------- */

std::string FlowManager::generate_timestamp()
{
    const std::time_t now = std::time(nullptr);
    std::tm local_time{};
#ifdef _WIN32
    localtime_s(&local_time, &now);
#else
    localtime_r(&now, &local_time);  // NOLINT(misc-include-cleaner)
#endif
    constexpr size_t timestamp_buffer_size = 20U;
    std::array<char, timestamp_buffer_size> buffer{};
    std::strftime(buffer.data(), buffer.size(), "%Y-%m-%d_%H-%M-%S", &local_time);
    return buffer.data();
}

std::pair<std::vector<std::string>, EnumerationResultVector>
FlowManager::load_graph_enumeration(CacheManagerType manager_type, const std::string& cache_dir,
                                    const std::string& cache_base_name, LoggerHandler logger)
{
    const std::shared_ptr<ICacheIOManager> cache_manager =
        make_cache_manager(manager_type, cache_dir, std::move(logger));

    const std::unordered_map<std::string, EnumerationResult> result =
        cache_manager->read(cache_base_name);
    std::vector<std::string> ordered_graph_names;
    EnumerationResultVector ordered_enumeration;
    ordered_graph_names.reserve(result.size());
    ordered_enumeration.reserve(result.size());
    for (const auto& [graph_name, enumeration] : result)
    {
        ordered_graph_names.push_back(graph_name);
        ordered_enumeration.push_back(enumeration);
    }
    return {ordered_graph_names, ordered_enumeration};
}

std::string FlowManager::build_per_graph_cache_name(const std::string& cache_base_name,
                                                    const std::string& graph_name)
{
    const std::string graph_stem = std::filesystem::path(graph_name).stem().string();
    return cache_base_name + "_" + graph_stem;
}

EnumerationResult
// NOLINTNEXTLINE(readability-function-size)
FlowManager::get_single_graph_enumeration(const bool write_to_cache,
                                          const std::string& cache_base_name,
                                          const std::shared_ptr<ICacheIOManager>& cache_manager,
                                          EnumerationPreprocessManager& preprocess_manager,
                                          const std::string& graph_name, const size_t graph_index,
                                          const PreprocessorFactory& factory,
                                          const LoggerHandler& logger, const bool use_gpu)
{
    const std::string per_graph_cache_name =
        build_per_graph_cache_name(cache_base_name, graph_name);
    if (write_to_cache && cache_manager && cache_manager->exists(per_graph_cache_name))
    {
        logger.log(LogLevel::INFO, "enumeration-found-skipped: graph=" + graph_name);
        return cache_manager->read(per_graph_cache_name).begin()->second;
    }
    const EnumerationResult result =
        preprocess_manager.preprocess_graph(graph_index, factory, use_gpu);
    if (write_to_cache && cache_manager)
    {
        cache_manager->write(per_graph_cache_name, result);
    }
    return result;
}

EnumerationResultVector
// NOLINTNEXTLINE(readability-function-size)
FlowManager::get_graph_enumeration(const bool write_to_cache, const std::string& cache_base_name,
                                   const std::shared_ptr<ICacheIOManager>& cache_manager,
                                   EnumerationPreprocessManager& preprocess_manager,
                                   const LibraryData& library, const PreprocessorFactory& factory,
                                   const LoggerHandler& logger, const bool use_gpu)
{
    EnumerationResultVector result;
    result.reserve(library.m_graph_names.size());
    for (size_t graph_index = 0U; graph_index < library.m_graph_names.size(); ++graph_index)
    {
        result.push_back(get_single_graph_enumeration(
            write_to_cache, cache_base_name, cache_manager, preprocess_manager,
            library.m_graph_names[graph_index], graph_index, factory, logger, use_gpu));
    }
    return result;
}

// NOLINTNEXTLINE(readability-function-size)
std::unordered_map<std::string, FilterResult> FlowManager::run_enumeration_filter_stage(
    const std::string& result_file_base_name, const EnumerationResultVector& graphs_enumeration,
    const ICacheIOManager& lib_cache_manager, IFilterIOManager& filter_results_writer,
    const LibraryData& library, const std::string& timestamp, const LoggerHandler& logger)
{
    std::unordered_map<std::string, FilterResult> filter_results;
    const std::unordered_map<std::string, EnumerationResult> library_enumeration =
        lib_cache_manager.read(result_file_base_name);
    std::vector<std::string> library_graph_names;
    EnumerationResultVector library_enumeration_vector;
    for (const auto& graph_data : library_enumeration)
    {
        library_graph_names.push_back(graph_data.first);
        library_enumeration_vector.push_back(graph_data.second);
    }
    const GroupEnumerationGraphFilter filter(library_enumeration_vector, logger);
    for (uint32_t graph_index = 0U; graph_index < static_cast<uint32_t>(graphs_enumeration.size());
         ++graph_index)
    {
        logger.log(LogLevel::INFO,
                   "Start filtering in backround graph " + std::to_string(graph_index) + "...");
        const FilterResult filtering_result = filter.filter(graphs_enumeration[graph_index]);
        std::string results_filename =
            std::filesystem::path(library.m_graph_names[graph_index]).stem().string();
        results_filename += "_";
        results_filename += result_file_base_name;
        results_filename += "_filtering_result_";
        results_filename += timestamp;
        filter_results_writer.write(results_filename, library_graph_names, filtering_result);
        filter_results[std::filesystem::path(library.m_graph_names[graph_index]).stem().string()] =
            filtering_result;
    }
    return filter_results;
}

std::vector<bool>
FlowManager::build_to_process(const std::vector<std::string>& graph_names,
                              const std::unordered_map<std::string, bool>& filter_map)
{
    std::vector<bool> to_process;
    to_process.reserve(graph_names.size());
    for (const std::string& name : graph_names)
    {
        const std::string stem = std::filesystem::path(name).stem().string();
        const std::unordered_map<std::string, bool>::const_iterator iter = filter_map.find(stem);
        const bool pruned = (iter != filter_map.end()) && iter->second;
        to_process.push_back(!pruned);
    }
    return to_process;
}

void FlowManager::read_graphs(IColoredGraphReader& reader, LibraryData& library,
                              const bool is_directed, const LoggerHandler& logger)
{
    for (uint32_t idx = 0U; idx < static_cast<uint32_t>(library.m_graph_names.size()); ++idx)
    {
        logger.log(LogLevel::INFO,
                   "[load] index=" + std::to_string(idx) + " file=" + library.m_graph_names[idx]);
        library.m_library.push_back(reader.read(library.m_graph_names[idx], is_directed, logger));
    }
}

LibraryData FlowManager::load_library(const std::string& path, const GraphReaderType reader_type,
                                      const bool is_directed, const LoggerHandler& logger,
                                      const ColorMapConfig& color_map_config)
{
    LibraryData library;
    const std::vector<std::string> all_files = IOUtils::get_files_in_directory(path);
    library.m_graph_names = reader_type == GraphReaderType::VERTEX_EDGE
                                ? collect_vertex_edge_base_paths(all_files)
                                : all_files;
    library.m_library.reserve(library.m_graph_names.size());
    if (reader_type != GraphReaderType::GRAPHML)
    {
        const std::unique_ptr<IColoredGraphReader> reader = make_graph_reader(reader_type);
        read_graphs(*reader, library, is_directed, logger);
        return library;
    }
    std::map<std::string, uint32_t> initial_color_map;
    if (!color_map_config.m_color_map_path.empty())
    {
        initial_color_map =
            GraphmlColorMapIOManager("", logger).load(color_map_config.m_color_map_path);
    }
    GraphmlGraphReader graphml_reader(initial_color_map);
    read_graphs(graphml_reader, library, is_directed, logger);
    if (!color_map_config.m_output_folder.empty())
    {
        GraphmlColorMapIOManager(color_map_config.m_output_folder, logger)
            .save("color_map_" + generate_timestamp(), graphml_reader.get_color_map());
    }
    return library;
}

std::vector<std::string>
FlowManager::collect_vertex_edge_base_paths(const std::vector<std::string>& all_files)
{
    std::vector<std::string> base_paths;
    for (const std::string& file : all_files)
    {
        const std::filesystem::path file_path(file);
        if (file_path.extension() == IOConstants::NODE_LABELS_SUFFIX)
        {
            base_paths.push_back((file_path.parent_path() / file_path.stem()).string());
        }
    }
    return base_paths;
}

}  // namespace sgf
