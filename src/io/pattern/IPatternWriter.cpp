#include "IPatternWriter.h"

#include "BoostGraph.h"
#include "IOUtils.h"
#include "LogLevel.h"
#include "LoggerHandler.h"

#include <filesystem>
#include <string>

namespace sgf
{

void IPatternWriter::write(const BoostGraph& graph, const std::string& path,
                           const LoggerHandler& logger, const bool is_directed) const
{
    IOUtils::create_directory_if_needed(std::filesystem::path(path).parent_path().string());
    logger.log(LogLevel::INFO, "Writing pattern to '" + path + "'");
    do_write(graph, path, is_directed);
}

}  // namespace sgf
