#include "FileLogger.h"
#include "ILogger.h"
#include "JsonGraphReader.h"
#include "LoggerHandler.h"
#include "MotifPreprocessor.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

using namespace sgf;

namespace
{

/**
 * @brief Converts a 128-bit unsigned integer to its decimal string representation.
 * @param value The value to convert.
 * @return Decimal string, or "0" if value is zero.
 */
std::string uint128_to_string(const __uint128_t value)
{
    if (value == 0U)
    {
        return "0";
    }
    constexpr uint32_t DECIMAL_BASE = 10U;
    std::string digits;
    __uint128_t remaining = value;
    while (remaining > 0U)
    {
        digits += static_cast<char>('0' + static_cast<uint32_t>(remaining % DECIMAL_BASE));
        remaining /= DECIMAL_BASE;
    }
    std::reverse(digits.begin(), digits.end());
    return digits;
}

/**
 * @brief Builds a FileLogger if a log path is given, otherwise returns nullptr.
 * @param argc Argument count from main.
 * @param argv Argument vector from main.
 * @return Shared pointer to FileLogger, or nullptr for no logging.
 */
std::shared_ptr<ILogger> make_logger(const int argc, const char* const argv[])
{
    constexpr int ARGC_WITH_LOG = 3;
    if (argc == ARGC_WITH_LOG)
    {
        return std::make_shared<FileLogger>(std::string{argv[2]});
    }
    return nullptr;
}

/**
 * @brief Emits motif counts and summary statistics as JSON to stdout.
 * @param motifs Map of canonical motif identifier to occurrence count.
 */
void print_json(const std::unordered_map<__uint128_t, uint32_t>& motifs)
{
    uint64_t total_subgraphs = 0U;
    for (const auto& [key, count] : motifs)
    {
        total_subgraphs += static_cast<uint64_t>(count);
    }
    const uint64_t total_motifs = static_cast<uint64_t>(motifs.size());

    std::cout << "{\n  \"motifs\": {";
    bool first = true;
    for (const auto& [key, count] : motifs)
    {
        if (!first)
        {
            std::cout << ',';
        }
        first = false;
        std::cout << "\n    \"" << uint128_to_string(key) << "\": " << count;
    }
    std::cout << "\n  },\n";
    std::cout << "  \"total_subgraphs\": " << total_subgraphs << ",\n";
    std::cout << "  \"total_motifs\": " << total_motifs << "\n}\n";
}

}  // namespace

/**
 * @brief CLI entry point: reads a JSON graph, runs MotifPreprocessor, prints motif counts.
 *
 * Usage: sgf_motif_counter <graph_json_path> [<log_file_path>]
 *
 * Reads @p graph_json_path (nodes/links JSON format) via JsonGraphReader,
 * runs MotifPreprocessor::calculate(), and prints the result as JSON to stdout.
 * If @p log_file_path is supplied, enumeration progress is written there via FileLogger.
 *
 * JSON output format:
 * @code
 * {
 *   "motifs": {"<128bit_key>": count, ...},
 *   "total_subgraphs": N,
 *   "total_motifs": M
 * }
 * @endcode
 *
 * @param argc Argument count (2 or 3).
 * @param argv Argument vector: argv[1] = graph path, argv[2] = log path (optional).
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on usage error.
 */
int main(const int argc, const char* const argv[])
{
    constexpr int MIN_ARGC = 2;
    constexpr int MAX_ARGC = 3;
    if (argc < MIN_ARGC || argc > MAX_ARGC)
    {
        std::cerr << "Usage: sgf_motif_counter <graph_json_path> [<log_file_path>]\n";
        return EXIT_FAILURE;
    }
    const std::string graph_path{argv[1]};
    const std::shared_ptr<ILogger> logger_ptr = make_logger(argc, argv);
    const LoggerHandler logger{std::weak_ptr<ILogger>{logger_ptr}};

    const JsonGraphReader reader;
    const ColoredGraph graph = reader.read(graph_path, false, logger);
    MotifPreprocessor preprocessor{graph, logger};
    const std::unordered_map<__uint128_t, uint32_t> motifs = preprocessor.calculate();
    print_json(motifs);
    return EXIT_SUCCESS;
}
