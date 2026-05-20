#include "IPatternWriter.h"

#include "IOUtils.h"

#include <filesystem>
#include <string>

namespace sgf
{

void IPatternWriter::write(const BoostGraph& graph, const std::string& path) const
{
    IOUtils::create_directory_if_needed(std::filesystem::path(path).parent_path().string());
    do_write(graph, path);
}

}  // namespace sgf
