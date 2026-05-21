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
#include "EnumerationPreprocessManager.h"
#include "PathProcessor.h"
#include "MotifPreprocessor.h"

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

/* ---------- Pipeline stages ---------- */

void FlowManager::enumerator_preprocess_run(const std::string& input_path, const bool is_directed,
    const GraphReaderType reader_type, std::string& output_path,
    CacheManagerType output_type, std::string log_file_path,
    bool preprocess_paths, bool preprocess_motifs)
{
    std::shared_ptr<FileLogger> logger = std::make_shared<FileLogger>(log_file_path);
    LoggerHandler logger_handler(logger);
    LibraryData library = load_library(input_path, reader_type, is_directed, logger_handler);
    EnumerationPreprocessManager preprocess_manager(library.m_library, logger_handler);
    std::unique_ptr<ICacheIOManager> cache_manager = make_cache_manager(output_type, output_path);
    const std::string timestamp = generate_timestamp();
    if (preprocess_paths)
    {
        EnumerationData result = preprocess_manager.preprocess(
            [](const ColoredGraph& graph, LoggerHandler logger) -> std::unique_ptr<IGraphPreprocessor>
            { return std::make_unique<PathProcessor>(graph, logger); });
        cache_manager->write(std::string(PATH_CACHE_BASE_NAME) + "_" + timestamp, result,
                             library.m_graph_names);
    }
    if (preprocess_motifs)
    {
        EnumerationData result = preprocess_manager.preprocess(
            [](const ColoredGraph& graph, LoggerHandler logger) -> std::unique_ptr<IGraphPreprocessor>
            { return std::make_unique<MotifPreprocessor>(graph, logger); });
        cache_manager->write(std::string(MOTIF_CACHE_BASE_NAME) + "_" + timestamp, result,
                             library.m_graph_names);
    }
}

void FlowManager::enumerator_filter_run(const std::string& cache_path, const bool is_directed,
    const CacheManagerType reader_type, std::string& output_file_path,
    bool preprocess_paths, bool preprocess_motifs)
{
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
