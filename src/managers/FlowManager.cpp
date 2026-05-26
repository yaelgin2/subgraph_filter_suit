#include "FlowManager.h"

#include "BinaryCacheIOManager.h"
#include "CSVCacheIOManager.h"
#include "CSVFilterOutputManager.h"
#include "ColoredGraph.h"
#include "EnumerationPreprocessManager.h"
#include "FilteringUtils.h"
#include "GraphmlGraphReader.h"
#include "GraphmlPatternWriter.h"
#include "GroupEnumerationGraphFilter.h"
#include "IColoredGraphReader.h"
#include "IFilterOutputManager.h"
#include "IGraphPreprocessor.h"
#include "IOUtils.h"
#include "IPatternWriter.h"
#include "InvalidArgumentException.h"
#include "JsonFilterOutputManager.h"
#include "JsonGraphReader.h"
#include "JsonPatternWriter.h"
#include "LogLevel.h"
#include "LoggerBundle.h"
#include "LoggerHandler.h"
#include "MotifPreprocessor.h"
#include "PathProcessor.h"
#include "VertexEdgeGraphReader.h"
#include "VertexEdgePatternWriter.h"

#include <ICacheIOManager.h>
#include <array>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <memory>
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

std::unique_ptr<IPatternWriter> FlowManager::make_pattern_writer(const PatternWriterType type)
{
    switch (type)
    {
    case PatternWriterType::GRAPHML:
        return std::make_unique<GraphmlPatternWriter>();
    case PatternWriterType::JSON:
        return std::make_unique<JsonPatternWriter>();
    case PatternWriterType::VERTEX_EDGE:
        return std::make_unique<VertexEdgePatternWriter>();
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

std::unique_ptr<IFilterOutputManager>
FlowManager::make_filter_results_writer(const ResultOutputType type, const std::string& folder,
                                        LoggerHandler logger)
{
    switch (type)
    {
    case ResultOutputType::JSON:
        return std::make_unique<JsonFilterOutputManager>(folder, std::move(logger));
    case ResultOutputType::CSV:
        return std::make_unique<CSVFilterOutputManager>(folder, std::move(logger));
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
    IFilterOutputManager& filter_results_writer, const std::string& timestamp,
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
    std::unique_ptr<IFilterOutputManager> filter_results_writer =
        make_filter_results_writer(output_type, output_folder, log_bundle.handler());
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

void FlowManager::pattern_preprocess_run()
{
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
    IFilterOutputManager& filter_results_writer, const LibraryData& library,
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
