#include "MatchOutputWriter.h"

#include "SgfPathDoesntExistException.h"

#include <ios>
#include <mutex>
#include <string>

namespace sgf
{

MatchOutputWriter::MatchOutputWriter(const std::string& file_path)
    : m_file(file_path, std::ios::out | std::ios::trunc)
{
    if (!m_file.is_open())
    {
        throw SgfPathDoesntExistException("Cannot open match output file: " + file_path);
    }
}

MatchOutputWriter::~MatchOutputWriter()
{
    m_file.close();
}

void MatchOutputWriter::write_match(const std::string& match)
{
    const std::lock_guard<std::mutex> lock{m_mutex};
    m_file << match << "\n";
}

}  // namespace sgf
