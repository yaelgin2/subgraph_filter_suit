#include "FlowManager.h"

#include "BinaryCacheIOManager.h"
#include "CSVCacheIOManager.h"
#include "GraphmlGraphReader.h"
#include "GraphmlPatternWriter.h"
#include "IOUtils.h"
#include "InvalidArgumentException.h"
#include "JsonGraphReader.h"
#include "JsonPatternWriter.h"
#include "VertexEdgeGraphReader.h"
#include "VertexEdgePatternWriter.h"
#include "FileLogger.h"
#include "LoggerBundle.h"
#include "EnumerationPreprocessManager.h"
#include "PathProcessor.h"
#include "MotifPreprocessor.h"
#include "IFilterOutputManager.h"
#include "JsonFilterOutputManager.h"
#include "GroupEnumerationGraphFilter.h"
#include "CSVFilterOutputManager.h"

#include <array>
#include <ctime>
#include <memory>
#include <string>
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

std::unique_ptr<ICacheIOManager> FlowManager::make_cache_manager(const CacheManagerType type,
                                                                  const std::string& folder)
{
    switch (type)
    {
        case CacheManagerType::BINARY:
            return std::make_unique<BinaryCacheIOManager>(folder);
        case CacheManagerType::CSV:
            return std::make_unique<CSVCacheIOManager>(folder);
        default:
            throw InvalidArgumentException("Unknown CacheManagerType.");
    }
}

std::unique_ptr<IFilterOutputManager> FlowManager::make_filter_results_writer(const ResultOutputType type,
                                                                  const std::string& folder)
{
    switch (type)
    {
        case ResultOutputType::JSON:
            return std::make_unique<JsonFilterOutputManager>(folder);
        case ResultOutputType::CSV:
            return std::make_unique<CSVFilterOutputManager>(folder);
        default:
            throw InvalidArgumentException("Unknown CacheManagerType.");
    }
}

/* ---------- Pipeline stages ---------- */

void FlowManager::enumerator_preprocess_run(const std::string& input_path, const bool is_directed,
    const GraphReaderType reader_type, std::string& output_path,
    CacheManagerType output_type, std::string log_file_path,
    bool preprocess_paths, bool preprocess_motifs)
{
    const LoggerBundle log_bundle(log_file_path);
    LibraryData library = load_library(input_path, reader_type, is_directed, log_bundle.handler());
    EnumerationPreprocessManager preprocess_manager(library.m_library, log_bundle.handler());
    std::unique_ptr<ICacheIOManager> cache_manager = make_cache_manager(output_type, output_path);
    const std::string timestamp = generate_timestamp();
    if (preprocess_paths)
    {
        EnumerationResultVector result = preprocess_manager.preprocess(
            [](const ColoredGraph& graph, LoggerHandler logger) -> std::unique_ptr<IGraphPreprocessor>
            { return std::make_unique<PathProcessor>(graph, logger); });
        cache_manager->write(std::string(PATH_CACHE_BASE_NAME) + "_" + timestamp, result,
                             library.m_graph_names);
    }
    if (preprocess_motifs)
    {
        EnumerationResultVector result = preprocess_manager.preprocess(
            [](const ColoredGraph& graph, LoggerHandler logger) -> std::unique_ptr<IGraphPreprocessor>
            { return std::make_unique<MotifPreprocessor>(graph, logger); });
        cache_manager->write(std::string(MOTIF_CACHE_BASE_NAME) + "_" + timestamp, result,
                             library.m_graph_names);
    }
}

void FlowManager::enumerator_filter_run(const std::string& graph_input_path, const bool is_directed,
    const GraphReaderType reader_type,
    const std::string& cache_path, const CacheManagerType cache_reader_type,
    std::string& output_folder, ResultOutputType output_type, std::string log_file_path,
    bool filter_paths, bool filter_motifs)
{
    const LoggerBundle log_bundle(log_file_path);
    LibraryData graphs_to_find_in = load_library(graph_input_path, reader_type, is_directed, log_bundle.handler());
    std::unique_ptr<ICacheIOManager> cache_manager = make_cache_manager(cache_reader_type, output_folder);
    std::unique_ptr<IFilterOutputManager> filter_results_writer = make_filter_results_writer(output_type, output_folder);
    EnumerationPreprocessManager preprocess_manager(graphs_to_find_in.m_library, log_bundle.handler());
    const std::string timestamp = generate_timestamp();
    if (filter_paths)
    {
        run_enumeration_filter_stage(
            [](const ColoredGraph& graph, LoggerHandler logger) -> std::unique_ptr<IGraphPreprocessor>
            { return std::make_unique<PathProcessor>(graph, logger); },
            preprocess_manager, *cache_manager, cache_path, *filter_results_writer,
            graphs_to_find_in, timestamp);
    }
    if (filter_motifs)
    {
        run_enumeration_filter_stage(
            [](const ColoredGraph& graph, LoggerHandler logger) -> std::unique_ptr<IGraphPreprocessor>
            { return std::make_unique<MotifPreprocessor>(graph, logger); },
            preprocess_manager, *cache_manager, cache_path, *filter_results_writer,
            graphs_to_find_in, timestamp);
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
    const std::tm* local_time = std::localtime(&now);
    constexpr size_t TIMESTAMP_BUFFER_SIZE = 20U;
    std::array<char, TIMESTAMP_BUFFER_SIZE> buffer{};
    std::strftime(buffer.data(), buffer.size(), "%Y-%m-%d_%H-%M-%S", local_time);
    return std::string(buffer.data());
}


void FlowManager::run_enumeration_filter_stage(
    const PreprocessorFactory& factory,
    EnumerationPreprocessManager& preprocess_manager,
    const ICacheIOManager& cache_manager,
    const std::string& cache_path,
    IFilterOutputManager& filter_results_writer,
    const LibraryData& library,
    const std::string& timestamp)
{
    const EnumerationResultVector graphs_enumeration = preprocess_manager.preprocess(factory);
    const std::unordered_map<std::string, EnumerationResult> library_enumeration =
        cache_manager.read(cache_path);
    EnumerationResultVector library_enumeration_vector;
    for(auto & graph_data : library_enumeration)
    {
        library_enumeration_vector.push_back(graph_data.second);
    }
    GroupEnumerationGraphFilter filter(library_enumeration_vector);
    for (uint32_t graph_index = 0U;
         graph_index < static_cast<uint32_t>(graphs_enumeration.size()); ++graph_index)
    {
        const FilterResult filtering_result = filter.filter(graphs_enumeration[graph_index]);
        filter_results_writer.write(
            library.m_graph_names[graph_index] + "_filtering_result_" + timestamp,
            library.m_graph_names,
            filtering_result);
    }
}

LibraryData FlowManager::load_library(const std::string& path, const GraphReaderType reader_type,
    const bool is_directed, LoggerHandler logger)
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
