#include "FlowManager.h"

#include "BinaryCacheIOManager.h"
#include "CSVCacheIOManager.h"
#include "CSVFilterIOManager.h"
#include "CSVPatternCacheIOManager.h"
#include "ColoredGraph.h"
#include "EnumerationPreprocessManager.h"
#include "FilteringUtils.h"
#include "GraphmlGraphReader.h"
#include "GraphmlPatternWriter.h"
#include "GroupEnumerationGraphFilter.h"
#include "IColoredGraphReader.h"
#include "IFilterIOManager.h"
#include "IGraphPreprocessor.h"
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
#include "MotifPreprocessor.h"
#include "MultiGraphPatternPreprocessor.h"
#include "PathProcessor.h"
#include "PattermPreprocessManager.h"
#include "SingleGraphPatternPreprocessor.h"
#include "VertexEdgeGraphReader.h"
#include "VertexEdgePatternWriter.h"

#include <ICacheIOManager.h>
#include <array>
#include <cstdint>
#include <ctime>
#include <filesystem>
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
void FlowManager::enumerator_preprocess_run(const std::string& input_path, const bool is_directed,
                                            const GraphReaderType reader_type,
                                            std::string& output_path, CacheManagerType output_type,
                                            const std::string& log_file_path, bool preprocess_paths,
                                            bool preprocess_motifs)
{
    const LoggerBundle log_bundle(log_file_path);
    const LibraryData library =
        load_library(input_path, reader_type, is_directed, log_bundle.handler());
    EnumerationPreprocessManager preprocess_manager(library.m_library, log_bundle.handler());
    const std::shared_ptr<ICacheIOManager> cache_manager =
        make_cache_manager(output_type, output_path, log_bundle.handler());
    const std::string timestamp = generate_timestamp();
    if (preprocess_paths)
    {
        get_graph_enumeration(
            true, std::string(PATH_CACHE_BASE_NAME), cache_manager, preprocess_manager, library,
            [](const ColoredGraph& graph,
               const LoggerHandler& logger) -> std::unique_ptr<IGraphPreprocessor>
            {
                return std::make_unique<PathProcessor>(graph, logger);
            },
            timestamp);
    }
    if (preprocess_motifs)
    {
        get_graph_enumeration(
            true, std::string(MOTIF_CACHE_BASE_NAME), cache_manager, preprocess_manager, library,
            [](const ColoredGraph& graph,
               const LoggerHandler& logger) -> std::unique_ptr<IGraphPreprocessor>
            {
                return std::make_unique<MotifPreprocessor>(graph, logger);
            },
            timestamp);
    }
}

// NOLINTNEXTLINE(readability-function-size)
void FlowManager::enumerate_and_filter(
    const std::string& library_cache_file, const bool load_graph_cache,
    const std::string& graphs_cache_path, const std::string& run_type_file_base_name,
    const PreprocessorFactory& factory, const CacheManagerType cache_reader_type,
    const std::shared_ptr<ICacheIOManager>& graphs_cache_manager, LibraryData& graphs_to_find_in,
    const std::unique_ptr<EnumerationPreprocessManager>& preprocess_manager,
    IFilterIOManager& filter_results_writer, const std::string& timestamp,
    const LoggerHandler& logger)
{
    EnumerationResultVector graph_enumeration;
    if (load_graph_cache)
    {
        std::tie(graphs_to_find_in.m_graph_names, graph_enumeration) =
            load_graph_enumeration(cache_reader_type, graphs_cache_path, logger);
    }
    else
    {
        graph_enumeration = get_graph_enumeration(
            graphs_cache_manager != nullptr, run_type_file_base_name, graphs_cache_manager,
            *preprocess_manager, graphs_to_find_in, factory, timestamp);
    }
    const std::filesystem::path lib_cache(library_cache_file);
    const std::shared_ptr<ICacheIOManager> lib_cache_manager =
        make_cache_manager(cache_reader_type, lib_cache.parent_path().string(), logger);
    run_enumeration_filter_stage(run_type_file_base_name, graph_enumeration, *lib_cache_manager,
                                 lib_cache.stem().string(), filter_results_writer,
                                 graphs_to_find_in, timestamp, logger);
}

// NOLINTNEXTLINE(readability-function-size)
void FlowManager::enumerator_filter_run(
    const std::string& graph_input_path, const bool is_directed, const GraphReaderType reader_type,
    const std::string& motif_cache_file, const std::string& path_cache_file,
    const CacheManagerType cache_reader_type, std::string& output_folder,
    ResultOutputType output_type, const std::string& log_file_path, bool filter_paths,
    bool filter_motifs, const GraphEnumerationCacheConfig& graph_cache_config)
{
    const LoggerBundle log_bundle(log_file_path);
    LibraryData graphs_to_find_in;
    std::unique_ptr<EnumerationPreprocessManager> preprocess_manager = nullptr;
    const bool need_path_compute =
        filter_paths && graph_cache_config.m_graphs_path_cache_path.empty();
    const bool need_motif_compute =
        filter_motifs && graph_cache_config.m_graphs_motif_cache_path.empty();
    if (need_path_compute || need_motif_compute)
    {
        graphs_to_find_in =
            load_library(graph_input_path, reader_type, is_directed, log_bundle.handler());
        preprocess_manager = std::make_unique<EnumerationPreprocessManager>(
            graphs_to_find_in.m_library, log_bundle.handler());
    }
    std::unique_ptr<IFilterIOManager> filter_results_writer =
        make_filter_results_io_manager(output_type, output_folder, log_bundle.handler());
    const std::string timestamp = generate_timestamp();
    std::shared_ptr<ICacheIOManager> graphs_cache_manager = nullptr;
    if (graph_cache_config.m_cache_enumeration)
    {
        graphs_cache_manager = make_cache_manager(
            cache_reader_type, graph_cache_config.m_graph_cache_dir, log_bundle.handler());
    }
    if (filter_paths)
    {
        enumerate_and_filter(
            path_cache_file, !graph_cache_config.m_graphs_path_cache_path.empty(),
            graph_cache_config.m_graphs_path_cache_path, std::string(PATH_CACHE_BASE_NAME),
            [](const ColoredGraph& graph,
               const LoggerHandler& logger) -> std::unique_ptr<IGraphPreprocessor>
            {
                return std::make_unique<PathProcessor>(graph, logger);
            },
            cache_reader_type, graphs_cache_manager, graphs_to_find_in, preprocess_manager,
            *filter_results_writer, timestamp, log_bundle.handler());
    }
    if (filter_motifs)
    {
        enumerate_and_filter(
            motif_cache_file, !graph_cache_config.m_graphs_motif_cache_path.empty(),
            graph_cache_config.m_graphs_motif_cache_path, std::string(MOTIF_CACHE_BASE_NAME),
            [](const ColoredGraph& graph,
               const LoggerHandler& logger) -> std::unique_ptr<IGraphPreprocessor>
            {
                return std::make_unique<MotifPreprocessor>(graph, logger);
            },
            cache_reader_type, graphs_cache_manager, graphs_to_find_in, preprocess_manager,
            *filter_results_writer, timestamp, log_bundle.handler());
    }
}

// NOLINTNEXTLINE(readability-function-size)
void FlowManager::pattern_preprocess_run(
    const std::string& input_path, const bool is_directed, GraphReaderType reader_type,
    std::string& output_path, const PatternWriterType output_type, const std::string& log_file_path,
    const uint32_t preprocess_multigraph, const uint32_t multigraph_alive_percent,
    const bool preprocess_singlegraph_results_file, const std::string& results_file_path,
    const int64_t preprocess_singlegraph, const ResultOutputType results_file_type,
    const std::string& background_graph_path, const double score_threshold,
    const SingleGraphFinderConfig& config)
{
    const LoggerBundle log_bundle(log_file_path);
    const LibraryData library =
        load_library(input_path, reader_type, is_directed, log_bundle.handler());
    const std::shared_ptr<IPatternWriter> pattern_writer = make_pattern_writer(output_type);
    const std::string timestamp = generate_timestamp();
    PatternPreprocessManager preprocess_manager(library.m_library, log_bundle.handler());
    if (preprocess_multigraph > 0U)
    {
        const PatternOutput multigraph_results = preprocess_manager.preprocess(
            [preprocess_multigraph, multigraph_alive_percent,
             is_directed](std::vector<ColoredGraph>& library_ref,
                          LoggerHandler logger) -> std::unique_ptr<IPatternPreprocessor>
            {
                return std::make_unique<MultiGraphPatternPreprocessor>(
                    library_ref, is_directed, preprocess_multigraph, multigraph_alive_percent,
                    std::move(logger));
            });
        const CSVPatternCacheIOManager cache_manager(output_path, log_bundle.handler(),
                                                     pattern_writer);
        cache_manager.write(multigraph_results, timestamp);
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
        const PatternOutput results = preprocess_manager.preprocess(
            [is_directed, &background, &to_process, score_threshold,
             &config](std::vector<ColoredGraph>& library_ref,
                      LoggerHandler logger) -> std::unique_ptr<IPatternPreprocessor>
            {
                return std::make_unique<SingleGraphPatternPreprocessor>(
                    library_ref, is_directed, background, to_process, score_threshold, config,
                    std::move(logger));
            });
        const CSVPatternCacheIOManager cache_manager(output_path, log_bundle.handler(),
                                                     pattern_writer);
        cache_manager.write(results, timestamp);
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
        const PatternOutput results = preprocess_manager.preprocess(
            [is_directed, &background, &to_process, score_threshold,
             &config](std::vector<ColoredGraph>& library_ref,
                      LoggerHandler logger) -> std::unique_ptr<IPatternPreprocessor>
            {
                return std::make_unique<SingleGraphPatternPreprocessor>(
                    library_ref, is_directed, background, to_process, score_threshold, config,
                    std::move(logger));
            });
        const CSVPatternCacheIOManager cache_manager(output_path, log_bundle.handler(),
                                                     pattern_writer);
        cache_manager.write(results, timestamp);
    }
}

void FlowManager::pattern_filter_run()
{
}

void FlowManager::subgraph_isomorphism_run()
{
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
FlowManager::load_graph_enumeration(CacheManagerType manager_type, const std::string& cache_path,
                                    LoggerHandler logger)
{
    const std::filesystem::path motif_file(cache_path);
    const std::shared_ptr<ICacheIOManager> cache_manager =
        make_cache_manager(manager_type, motif_file.parent_path().string(), std::move(logger));

    const std::unordered_map<std::string, EnumerationResult> result =
        cache_manager->read(motif_file.stem().string());
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

EnumerationResultVector
// NOLINTNEXTLINE(readability-function-size)
FlowManager::get_graph_enumeration(const bool write_to_cache, const std::string& cache_base_name,
                                   const std::shared_ptr<ICacheIOManager>& cache_manager,
                                   EnumerationPreprocessManager& preprocess_manager,
                                   const LibraryData& library, const PreprocessorFactory& factory,
                                   const std::string& timestamp)
{
    EnumerationResultVector result = preprocess_manager.preprocess(factory);
    if (write_to_cache && cache_manager)
    {
        cache_manager->write(std::string(cache_base_name) + "_" + timestamp, result,
                             library.m_graph_names);
    }
    return result;
}

// NOLINTNEXTLINE(readability-function-size)
void FlowManager::run_enumeration_filter_stage(
    const std::string& result_file_base_name, const EnumerationResultVector& graphs_enumeration,
    const ICacheIOManager& lib_cache_manager, const std::string& lib_cache_path,
    IFilterIOManager& filter_results_writer, const LibraryData& library,
    const std::string& timestamp, const LoggerHandler& logger)
{
    const std::unordered_map<std::string, EnumerationResult> library_enumeration =
        lib_cache_manager.read(lib_cache_path);
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
    }
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

LibraryData FlowManager::load_library(const std::string& path, const GraphReaderType reader_type,
                                      const bool is_directed, const LoggerHandler& logger)
{
    LibraryData library;
    library.m_graph_names = IOUtils::get_files_in_directory(path);
    library.m_library.reserve(library.m_graph_names.size());
    std::unique_ptr<IColoredGraphReader> reader = make_graph_reader(reader_type);
    for (const std::string& graph_name : library.m_graph_names)
    {
        library.m_library.push_back(reader->read(graph_name, is_directed, logger));
    }
    return library;
}

}  // namespace sgf
