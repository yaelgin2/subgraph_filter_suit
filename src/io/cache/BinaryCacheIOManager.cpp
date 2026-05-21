#include "BinaryCacheIOManager.h"

#include "EnumerationPreprocessManager.h"
#include "GraphConstructionException.h"
#include "ICacheIOManager.h"
#include "IGraphPreprocessor.h"
#include "Int128.h"
#include "SgfPathDoesntExistException.h"

#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <new>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sgf
{

BinaryCacheIOManager::BinaryCacheIOManager(std::string folder)
    : ICacheIOManager(std::move(folder))
{
}

std::string BinaryCacheIOManager::get_extension() const
{
    return "bin";
}

// ── Write helpers ─────────────────────────────────────────────────────────────

void BinaryCacheIOManager::write_collection_header(std::ofstream& output_stream, const size_t size,
                                                   const uint8_t fix_base, const uint8_t format16,
                                                   const uint8_t format32)
{
    if (size <= MSGPACK_FIX_COLLECTION_MAX)
    {
        const char raw_byte = static_cast<char>(
            fix_base | (static_cast<uint8_t>(size) &    // NOLINT(hicpp-signed-bitwise)
                        MSGPACK_FIX_COLLECTION_MASK));  // NOLINT(hicpp-signed-bitwise)
        output_stream.write(&raw_byte, SINGLE_BYTE);
    }
    else if (size <= MSGPACK_COLLECTION16_MAX)
    {
        const uint16_t len = static_cast<uint16_t>(size);
        const std::array<char, UINT16_MSGPACK_BYTE_COUNT> bytes = {
            static_cast<char>(format16),
            static_cast<char>(
                static_cast<uint8_t>((static_cast<uint32_t>(len) >> SHIFT_8) & BYTE_MASK)),
            static_cast<char>(static_cast<uint8_t>(len & BYTE_MASK))};
        output_stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    else
    {
        const uint32_t len = static_cast<uint32_t>(size);
        const std::array<char, UINT32_MSGPACK_BYTE_COUNT> bytes = {
            static_cast<char>(format32),
            static_cast<char>(static_cast<uint8_t>((len >> SHIFT_24) & BYTE_MASK)),
            static_cast<char>(static_cast<uint8_t>((len >> SHIFT_16) & BYTE_MASK)),
            static_cast<char>(static_cast<uint8_t>((len >> SHIFT_8) & BYTE_MASK)),
            static_cast<char>(static_cast<uint8_t>(len & BYTE_MASK))};
        output_stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
}

void BinaryCacheIOManager::write_array_header(std::ofstream& output_stream, const size_t size)
{
    write_collection_header(output_stream, size, MSGPACK_FIXARRAY_BASE, MSGPACK_ARRAY16_FORMAT,
                            MSGPACK_ARRAY32_FORMAT);
}

void BinaryCacheIOManager::write_map_header(std::ofstream& output_stream, const size_t size)
{
    write_collection_header(output_stream, size, MSGPACK_FIXMAP_BASE, MSGPACK_MAP16_FORMAT,
                            MSGPACK_MAP32_FORMAT);
}

void BinaryCacheIOManager::write_uint32_value(std::ofstream& output_stream, const uint32_t value)
{
    const std::array<char, UINT32_MSGPACK_BYTE_COUNT> bytes = {
        static_cast<char>(MSGPACK_UINT32_FORMAT),
        static_cast<char>(static_cast<uint8_t>((value >> SHIFT_24) & BYTE_MASK)),
        static_cast<char>(static_cast<uint8_t>((value >> SHIFT_16) & BYTE_MASK)),
        static_cast<char>(static_cast<uint8_t>((value >> SHIFT_8) & BYTE_MASK)),
        static_cast<char>(static_cast<uint8_t>(value & BYTE_MASK))};
    output_stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void BinaryCacheIOManager::write_uint64_value(std::ofstream& output_stream, const uint64_t value)
{
    const std::array<char, UINT64_MSGPACK_BYTE_COUNT> bytes = {
        static_cast<char>(MSGPACK_UINT64_FORMAT),
        static_cast<char>(static_cast<uint8_t>((value >> SHIFT_56) & BYTE_MASK)),
        static_cast<char>(static_cast<uint8_t>((value >> SHIFT_48) & BYTE_MASK)),
        static_cast<char>(static_cast<uint8_t>((value >> SHIFT_40) & BYTE_MASK)),
        static_cast<char>(static_cast<uint8_t>((value >> SHIFT_32) & BYTE_MASK)),
        static_cast<char>(static_cast<uint8_t>((value >> SHIFT_24) & BYTE_MASK)),
        static_cast<char>(static_cast<uint8_t>((value >> SHIFT_16) & BYTE_MASK)),
        static_cast<char>(static_cast<uint8_t>((value >> SHIFT_8) & BYTE_MASK)),
        static_cast<char>(static_cast<uint8_t>(value & BYTE_MASK))};
    output_stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void BinaryCacheIOManager::write_uint128_key(std::ofstream& output_stream, const UInt128& value)
{
    const char header = static_cast<char>(MSGPACK_UINT128_HEADER);
    output_stream.write(&header, SINGLE_BYTE);
    write_uint64_value(output_stream, value.m_high);
    write_uint64_value(output_stream, value.m_low);
}

void BinaryCacheIOManager::write_graph_result(std::ofstream& output_stream,
                                              const EnumerationResult& result)
{
    write_map_header(output_stream, result.size());
    for (const auto& entry : result)
    {
        write_uint128_key(output_stream, entry.first);
        write_uint32_value(output_stream, entry.second);
    }
}

void BinaryCacheIOManager::write_string(std::ofstream& output_stream, const std::string& value)
{
    const size_t len = value.size();
    if (len <= MSGPACK_FIXSTR_MAX_LEN)
    {
        const char header =
            static_cast<char>(MSGPACK_FIXSTR_BASE | static_cast<uint8_t>(len));
        output_stream.write(&header, SINGLE_BYTE);
    }
    else if (len <= UINT8_MAX)
    {
        const std::array<char, 2> bytes = {static_cast<char>(MSGPACK_STR8_FORMAT),
                                           static_cast<char>(static_cast<uint8_t>(len))};
        output_stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    else if (len <= UINT16_MAX)
    {
        const uint16_t len16 = static_cast<uint16_t>(len);
        const std::array<char, 3> bytes = {
            static_cast<char>(MSGPACK_STR16_FORMAT),
            static_cast<char>(static_cast<uint8_t>(len16 >> SHIFT_8)),
            static_cast<char>(static_cast<uint8_t>(len16 & BYTE_MASK))};
        output_stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    else
    {
        const uint32_t len32 = static_cast<uint32_t>(len);
        const std::array<char, 5> bytes = {
            static_cast<char>(MSGPACK_STR32_FORMAT),
            static_cast<char>(static_cast<uint8_t>((len32 >> SHIFT_24) & BYTE_MASK)),
            static_cast<char>(static_cast<uint8_t>((len32 >> SHIFT_16) & BYTE_MASK)),
            static_cast<char>(static_cast<uint8_t>((len32 >> SHIFT_8) & BYTE_MASK)),
            static_cast<char>(static_cast<uint8_t>(len32 & BYTE_MASK))};
        output_stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    output_stream.write(value.data(), static_cast<std::streamsize>(len));
}

void BinaryCacheIOManager::write_to_file(const EnumerationData& data,
                                         const std::vector<std::string>& graph_names,
                                         const std::string& full_path) const
{
    std::ofstream file(full_path, std::ios::binary);
    if (!file.is_open())
    {
        throw SgfPathDoesntExistException("Cannot open file for writing: '" + full_path + "'");
    }
    write_map_header(file, data.size());
    for (size_t graph_index = 0U; graph_index < data.size(); ++graph_index)
    {
        write_string(file, graph_names[graph_index]);
        write_graph_result(file, data[graph_index]);
    }
    if (file.fail())
    {
        throw SgfPathDoesntExistException("Write error on file: '" + full_path + "'");
    }
}

// ── Read helpers ──────────────────────────────────────────────────────────────

void BinaryCacheIOManager::check_read_stream(const std::ifstream& input_stream)
{
    if (input_stream.fail())
    {
        throw GraphConstructionException(
            "Unexpected end of data or read error in binary cache file");
    }
}

uint8_t BinaryCacheIOManager::read_byte(std::ifstream& input_stream)
{
    char raw_byte{};
    input_stream.read(&raw_byte, SINGLE_BYTE);
    check_read_stream(input_stream);
    return static_cast<uint8_t>(raw_byte);
}

size_t BinaryCacheIOManager::read_be_uint32_size(std::ifstream& input_stream)
{
    std::array<char, RAW_UINT32_BYTE_COUNT> buf{};
    input_stream.read(buf.data(), static_cast<std::streamsize>(buf.size()));
    check_read_stream(input_stream);
    return (static_cast<size_t>(static_cast<uint8_t>(buf[0])) << SHIFT_24) |
           (static_cast<size_t>(static_cast<uint8_t>(buf[1])) << SHIFT_16) |
           (static_cast<size_t>(static_cast<uint8_t>(buf[2])) << SHIFT_8) |
           static_cast<size_t>(static_cast<uint8_t>(buf[3]));
}

size_t BinaryCacheIOManager::read_be_uint16_size(std::ifstream& input_stream)
{
    std::array<char, RAW_UINT16_BYTE_COUNT> buf{};
    input_stream.read(buf.data(), static_cast<std::streamsize>(buf.size()));
    check_read_stream(input_stream);
    return (static_cast<size_t>(static_cast<uint8_t>(buf[0])) << SHIFT_8) |
           static_cast<size_t>(static_cast<uint8_t>(buf[1]));
}

size_t BinaryCacheIOManager::read_collection_header(std::ifstream& input_stream,
                                                    const uint8_t format16, const uint8_t format32)
{
    const uint8_t format_byte = read_byte(input_stream);
    if (format_byte == format32)
    {
        return read_be_uint32_size(input_stream);
    }
    if (format_byte == format16)
    {
        return read_be_uint16_size(input_stream);
    }
    return static_cast<size_t>(format_byte & MSGPACK_FIX_COLLECTION_MASK);
}

size_t BinaryCacheIOManager::read_array_header(std::ifstream& input_stream)
{
    return read_collection_header(input_stream, MSGPACK_ARRAY16_FORMAT, MSGPACK_ARRAY32_FORMAT);
}

size_t BinaryCacheIOManager::read_map_header(std::ifstream& input_stream)
{
    return read_collection_header(input_stream, MSGPACK_MAP16_FORMAT, MSGPACK_MAP32_FORMAT);
}

uint32_t BinaryCacheIOManager::read_uint32_value(std::ifstream& input_stream)
{
    std::array<char, UINT32_MSGPACK_BYTE_COUNT> bytes{};
    input_stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    check_read_stream(input_stream);
    if (static_cast<uint8_t>(bytes[FORMAT_BYTE_IDX]) != MSGPACK_UINT32_FORMAT)
    {
        throw GraphConstructionException("Expected uint32 format byte in binary cache file");
    }
    return (static_cast<uint32_t>(static_cast<uint8_t>(bytes[UINT32_BYTE_IDX_1])) << SHIFT_24) |
           (static_cast<uint32_t>(static_cast<uint8_t>(bytes[UINT32_BYTE_IDX_2])) << SHIFT_16) |
           (static_cast<uint32_t>(static_cast<uint8_t>(bytes[UINT32_BYTE_IDX_3])) << SHIFT_8) |
           static_cast<uint32_t>(static_cast<uint8_t>(bytes[UINT32_BYTE_IDX_4]));
}

uint64_t BinaryCacheIOManager::read_uint64_value(std::ifstream& input_stream)
{
    std::array<char, UINT64_MSGPACK_BYTE_COUNT> bytes{};
    input_stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    check_read_stream(input_stream);
    if (static_cast<uint8_t>(bytes[FORMAT_BYTE_IDX]) != MSGPACK_UINT64_FORMAT)
    {
        throw GraphConstructionException("Expected uint64 format byte in binary cache file");
    }
    return (static_cast<uint64_t>(static_cast<uint8_t>(bytes[UINT64_BYTE_IDX_1])) << SHIFT_56) |
           (static_cast<uint64_t>(static_cast<uint8_t>(bytes[UINT64_BYTE_IDX_2])) << SHIFT_48) |
           (static_cast<uint64_t>(static_cast<uint8_t>(bytes[UINT64_BYTE_IDX_3])) << SHIFT_40) |
           (static_cast<uint64_t>(static_cast<uint8_t>(bytes[UINT64_BYTE_IDX_4])) << SHIFT_32) |
           (static_cast<uint64_t>(static_cast<uint8_t>(bytes[UINT64_BYTE_IDX_5])) << SHIFT_24) |
           (static_cast<uint64_t>(static_cast<uint8_t>(bytes[UINT64_BYTE_IDX_6])) << SHIFT_16) |
           (static_cast<uint64_t>(static_cast<uint8_t>(bytes[UINT64_BYTE_IDX_7])) << SHIFT_8) |
           static_cast<uint64_t>(static_cast<uint8_t>(bytes[UINT64_BYTE_IDX_8]));
}

UInt128 BinaryCacheIOManager::read_uint128_key(std::ifstream& input_stream)
{
    const uint8_t header = read_byte(input_stream);
    if (header != MSGPACK_UINT128_HEADER)
    {
        throw GraphConstructionException("Expected UInt128 fixarray header in binary cache file");
    }
    const uint64_t high = read_uint64_value(input_stream);
    const uint64_t low = read_uint64_value(input_stream);
    return UInt128{high, low};
}

EnumerationResult BinaryCacheIOManager::read_graph_result(std::ifstream& input_stream)
{
    const size_t entry_count = read_map_header(input_stream);
    EnumerationResult result;
    result.reserve(entry_count);
    for (size_t entry_idx = 0; entry_idx < entry_count; ++entry_idx)
    {
        const UInt128 key = read_uint128_key(input_stream);
        const uint32_t value = read_uint32_value(input_stream);
        result.emplace(key, value);
    }
    return result;
}

std::string BinaryCacheIOManager::read_string(std::ifstream& input_stream)
{
    const uint8_t format_byte = read_byte(input_stream);
    size_t len = 0U;
    if ((format_byte & MSGPACK_FIXSTR_PREFIX_MASK) == MSGPACK_FIXSTR_BASE)
    {
        len = static_cast<size_t>(format_byte & MSGPACK_FIXSTR_LEN_MASK);
    }
    else if (format_byte == MSGPACK_STR8_FORMAT)
    {
        len = static_cast<size_t>(read_byte(input_stream));
    }
    else if (format_byte == MSGPACK_STR16_FORMAT)
    {
        len = read_be_uint16_size(input_stream);
    }
    else if (format_byte == MSGPACK_STR32_FORMAT)
    {
        len = read_be_uint32_size(input_stream);
    }
    else
    {
        throw GraphConstructionException("Expected string format byte in binary cache file");
    }
    std::string result(len, '\0');
    input_stream.read(result.data(), static_cast<std::streamsize>(len));
    check_read_stream(input_stream);
    return result;
}

std::unordered_map<std::string, EnumerationResult>
BinaryCacheIOManager::parse_binary(std::ifstream& input_stream)
{
    const size_t graph_count = read_map_header(input_stream);
    if (graph_count > MAX_GRAPH_COUNT)
    {
        throw GraphConstructionException("Graph count exceeds maximum in binary cache file");
    }
    std::unordered_map<std::string, EnumerationResult> data;
    data.reserve(graph_count);
    for (size_t graph_idx = 0; graph_idx < graph_count; ++graph_idx)
    {
        std::string name = read_string(input_stream);
        data.emplace(std::move(name), read_graph_result(input_stream));
    }
    return data;
}

std::unordered_map<std::string, EnumerationResult>
BinaryCacheIOManager::read_from_file(const std::string& full_path) const
{
    std::ifstream file(full_path, std::ios::binary);
    if (!file.is_open())
    {
        throw SgfPathDoesntExistException("Cannot open file for reading: '" + full_path + "'");
    }
    try
    {
        return parse_binary(file);
    }
    catch (const std::bad_alloc&)
    {
        throw GraphConstructionException("Memory allocation failed reading binary cache file: '" +
                                         full_path + "'");
    }
}

}  // namespace sgf
