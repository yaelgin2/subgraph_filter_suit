#include "BinaryCacheWriter.h"

#include "Int128.h"
#include "SgfPathDoesntExistException.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <utility>

namespace sgf
{

BinaryCacheWriter::BinaryCacheWriter(std::string folder, std::string base_filename)
    : ICacheWriter(std::move(folder), std::move(base_filename))
{
}

std::string BinaryCacheWriter::get_extension() const
{
    return "bin";
}

void BinaryCacheWriter::write_collection_header(std::ofstream& out, const size_t size,
                                                const uint8_t fix_base, const uint8_t format16,
                                                const uint8_t format32)
{
    if (size <= MSGPACK_FIX_COLLECTION_MAX)
    {
        const char byte = static_cast<char>(fix_base | static_cast<uint8_t>(size));
        out.write(&byte, SINGLE_BYTE);
    }
    else if (size <= MSGPACK_COLLECTION16_MAX)
    {
        const uint16_t len = static_cast<uint16_t>(size);
        const std::array<char, 3> bytes = {static_cast<char>(format16),
                                           static_cast<char>((len >> 8U) & 0xFFU),
                                           static_cast<char>(len & 0xFFU)};
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    else
    {
        const uint32_t len = static_cast<uint32_t>(size);
        const std::array<char, 5> bytes = {static_cast<char>(format32),
                                           static_cast<char>((len >> 24U) & 0xFFU),
                                           static_cast<char>((len >> 16U) & 0xFFU),
                                           static_cast<char>((len >> 8U) & 0xFFU),
                                           static_cast<char>(len & 0xFFU)};
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
}

void BinaryCacheWriter::write_array_header(std::ofstream& out, const size_t size)
{
    write_collection_header(out, size, MSGPACK_FIXARRAY_BASE, MSGPACK_ARRAY16_FORMAT,
                            MSGPACK_ARRAY32_FORMAT);
}

void BinaryCacheWriter::write_map_header(std::ofstream& out, const size_t size)
{
    write_collection_header(out, size, MSGPACK_FIXMAP_BASE, MSGPACK_MAP16_FORMAT,
                            MSGPACK_MAP32_FORMAT);
}

void BinaryCacheWriter::write_uint32_value(std::ofstream& out, const uint32_t value)
{
    const std::array<char, 5> bytes = {static_cast<char>(MSGPACK_UINT32_FORMAT),
                                       static_cast<char>((value >> 24U) & 0xFFU),
                                       static_cast<char>((value >> 16U) & 0xFFU),
                                       static_cast<char>((value >> 8U) & 0xFFU),
                                       static_cast<char>(value & 0xFFU)};
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void BinaryCacheWriter::write_uint64_value(std::ofstream& out, const uint64_t value)
{
    const std::array<char, 9> bytes = {static_cast<char>(MSGPACK_UINT64_FORMAT),
                                       static_cast<char>((value >> 56U) & 0xFFU),
                                       static_cast<char>((value >> 48U) & 0xFFU),
                                       static_cast<char>((value >> 40U) & 0xFFU),
                                       static_cast<char>((value >> 32U) & 0xFFU),
                                       static_cast<char>((value >> 24U) & 0xFFU),
                                       static_cast<char>((value >> 16U) & 0xFFU),
                                       static_cast<char>((value >> 8U) & 0xFFU),
                                       static_cast<char>(value & 0xFFU)};
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void BinaryCacheWriter::write_uint128_key(std::ofstream& out, const UInt128& value)
{
    const char header = static_cast<char>(MSGPACK_FIXARRAY_BASE | MSGPACK_UINT128_ARRAY_SIZE);
    out.write(&header, SINGLE_BYTE);
    write_uint64_value(out, value.m_high);
    write_uint64_value(out, value.m_low);
}

void BinaryCacheWriter::write_graph_result(std::ofstream& out, const EnumerationResult& result)
{
    write_map_header(out, result.size());
    for (const auto& entry : result)
    {
        write_uint128_key(out, entry.first);
        write_uint32_value(out, entry.second);
    }
}

void BinaryCacheWriter::write_to_file(const EnumerationData& data,
                                      const std::string& full_path) const
{
    std::ofstream file(full_path, std::ios::binary);
    if (!file.is_open())
    {
        throw SgfPathDoesntExistException("Cannot open file for writing: '" + full_path + "'");
    }
    write_array_header(file, data.size());
    for (const auto& result : data)
    {
        write_graph_result(file, result);
    }
}

}  // namespace sgf
