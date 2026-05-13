#include "BinaryCacheIOManager.h"

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

BinaryCacheIOManager::BinaryCacheIOManager(std::string folder, std::string base_filename)
    : ICacheIOManagment(std::move(folder), std::move(base_filename))
{
}

std::string BinaryCacheIOManager::get_extension() const
{
    return "bin";
}

// ── Write helpers ─────────────────────────────────────────────────────────────

void BinaryCacheIOManager::write_collection_header(std::ofstream& out, const size_t size,
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

void BinaryCacheIOManager::write_array_header(std::ofstream& out, const size_t size)
{
    write_collection_header(out, size, MSGPACK_FIXARRAY_BASE, MSGPACK_ARRAY16_FORMAT,
                            MSGPACK_ARRAY32_FORMAT);
}

void BinaryCacheIOManager::write_map_header(std::ofstream& out, const size_t size)
{
    write_collection_header(out, size, MSGPACK_FIXMAP_BASE, MSGPACK_MAP16_FORMAT,
                            MSGPACK_MAP32_FORMAT);
}

void BinaryCacheIOManager::write_uint32_value(std::ofstream& out, const uint32_t value)
{
    const std::array<char, 5> bytes = {static_cast<char>(MSGPACK_UINT32_FORMAT),
                                       static_cast<char>((value >> 24U) & 0xFFU),
                                       static_cast<char>((value >> 16U) & 0xFFU),
                                       static_cast<char>((value >> 8U) & 0xFFU),
                                       static_cast<char>(value & 0xFFU)};
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void BinaryCacheIOManager::write_uint64_value(std::ofstream& out, const uint64_t value)
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

void BinaryCacheIOManager::write_uint128_key(std::ofstream& out, const UInt128& value)
{
    const char header = static_cast<char>(MSGPACK_FIXARRAY_BASE | MSGPACK_UINT128_ARRAY_SIZE);
    out.write(&header, SINGLE_BYTE);
    write_uint64_value(out, value.m_high);
    write_uint64_value(out, value.m_low);
}

void BinaryCacheIOManager::write_graph_result(std::ofstream& out, const EnumerationResult& result)
{
    write_map_header(out, result.size());
    for (const auto& entry : result)
    {
        write_uint128_key(out, entry.first);
        write_uint32_value(out, entry.second);
    }
}

void BinaryCacheIOManager::write_to_file(const EnumerationData& data,
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

// ── Read helpers ──────────────────────────────────────────────────────────────

uint8_t BinaryCacheIOManager::read_byte(std::ifstream& in)
{
    char byte{};
    in.read(&byte, SINGLE_BYTE);
    return static_cast<uint8_t>(byte);
}

size_t BinaryCacheIOManager::read_be_uint32_size(std::ifstream& in)
{
    std::array<char, 4> buf{};
    in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
    return (static_cast<size_t>(static_cast<uint8_t>(buf[0])) << 24U)
         | (static_cast<size_t>(static_cast<uint8_t>(buf[1])) << 16U)
         | (static_cast<size_t>(static_cast<uint8_t>(buf[2])) << 8U)
         | static_cast<size_t>(static_cast<uint8_t>(buf[3]));
}

size_t BinaryCacheIOManager::read_be_uint16_size(std::ifstream& in)
{
    std::array<char, 2> buf{};
    in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
    return (static_cast<size_t>(static_cast<uint8_t>(buf[0])) << 8U)
         | static_cast<size_t>(static_cast<uint8_t>(buf[1]));
}

size_t BinaryCacheIOManager::read_collection_header(std::ifstream& in, const uint8_t format16,
                                                     const uint8_t format32)
{
    const uint8_t byte = read_byte(in);
    if (byte == format32)
    {
        return read_be_uint32_size(in);
    }
    if (byte == format16)
    {
        return read_be_uint16_size(in);
    }
    return static_cast<size_t>(byte & MSGPACK_FIX_COLLECTION_MASK);
}

size_t BinaryCacheIOManager::read_array_header(std::ifstream& in)
{
    return read_collection_header(in, MSGPACK_ARRAY16_FORMAT, MSGPACK_ARRAY32_FORMAT);
}

size_t BinaryCacheIOManager::read_map_header(std::ifstream& in)
{
    return read_collection_header(in, MSGPACK_MAP16_FORMAT, MSGPACK_MAP32_FORMAT);
}

uint32_t BinaryCacheIOManager::read_uint32_value(std::ifstream& in)
{
    std::array<char, 5> bytes{};
    in.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return (static_cast<uint32_t>(static_cast<uint8_t>(bytes[1])) << 24U)
         | (static_cast<uint32_t>(static_cast<uint8_t>(bytes[2])) << 16U)
         | (static_cast<uint32_t>(static_cast<uint8_t>(bytes[3])) << 8U)
         | static_cast<uint32_t>(static_cast<uint8_t>(bytes[4]));
}

uint64_t BinaryCacheIOManager::read_uint64_value(std::ifstream& in)
{
    std::array<char, 9> bytes{};
    in.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return (static_cast<uint64_t>(static_cast<uint8_t>(bytes[1])) << 56U)
         | (static_cast<uint64_t>(static_cast<uint8_t>(bytes[2])) << 48U)
         | (static_cast<uint64_t>(static_cast<uint8_t>(bytes[3])) << 40U)
         | (static_cast<uint64_t>(static_cast<uint8_t>(bytes[4])) << 32U)
         | (static_cast<uint64_t>(static_cast<uint8_t>(bytes[5])) << 24U)
         | (static_cast<uint64_t>(static_cast<uint8_t>(bytes[6])) << 16U)
         | (static_cast<uint64_t>(static_cast<uint8_t>(bytes[7])) << 8U)
         | static_cast<uint64_t>(static_cast<uint8_t>(bytes[8]));
}

UInt128 BinaryCacheIOManager::read_uint128_key(std::ifstream& in)
{
    read_byte(in);
    const uint64_t high = read_uint64_value(in);
    const uint64_t low = read_uint64_value(in);
    return UInt128{high, low};
}

EnumerationResult BinaryCacheIOManager::read_graph_result(std::ifstream& in)
{
    const size_t entry_count = read_map_header(in);
    EnumerationResult result;
    result.reserve(entry_count);
    for (size_t entry_idx = 0; entry_idx < entry_count; ++entry_idx)
    {
        const UInt128 key = read_uint128_key(in);
        const uint32_t value = read_uint32_value(in);
        result.emplace(key, value);
    }
    return result;
}

EnumerationData BinaryCacheIOManager::read_from_file(const std::string& full_path) const
{
    std::ifstream file(full_path, std::ios::binary);
    if (!file.is_open())
    {
        throw SgfPathDoesntExistException("Cannot open file for reading: '" + full_path + "'");
    }
    const size_t graph_count = read_array_header(file);
    EnumerationData data;
    data.reserve(graph_count);
    for (size_t graph_idx = 0; graph_idx < graph_count; ++graph_idx)
    {
        data.push_back(read_graph_result(file));
    }
    return data;
}

}  // namespace sgf
