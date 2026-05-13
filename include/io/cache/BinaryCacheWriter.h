#pragma once

#include "ICacheWriter.h"
#include "Int128.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>

namespace sgf
{

/**
 * @class BinaryCacheWriter
 * @brief Writes enumeration frequency data to a MessagePack binary cache file.
 *
 * Serializes the data as a MessagePack array of maps. Each outer element
 * corresponds to one library graph; each map entry encodes one motif's
 * frequency. Keys are UInt128 values serialized as a 2-element fixarray
 * [high:uint64, low:uint64]; values are uint32.
 */
class BinaryCacheWriter : public ICacheWriter
{
public:
    /**
     * @brief Constructs a BinaryCacheWriter.
     *
     * @param folder        Directory where the binary file will be written.
     * @param base_filename File name without extension.
     */
    BinaryCacheWriter(std::string folder, std::string base_filename);

protected:
    /**
     * @brief Writes enumeration data to a MessagePack file at @p full_path.
     *
     * @param data      The enumeration data to serialize.
     * @param full_path Destination file path including the .bin extension.
     * @throws SgfPathDoesntExistException if the file cannot be opened for writing.
     */
    void write_to_file(const EnumerationData& data, const std::string& full_path) const override;

    /**
     * @brief Returns the binary file extension.
     * @return "bin"
     */
    [[nodiscard]] std::string get_extension() const override;

private:
    static constexpr uint8_t MSGPACK_FIXARRAY_BASE      = 0x90U;
    static constexpr uint8_t MSGPACK_FIXMAP_BASE        = 0x80U;
    static constexpr uint8_t MSGPACK_ARRAY16_FORMAT     = 0xDCU;
    static constexpr uint8_t MSGPACK_ARRAY32_FORMAT     = 0xDDU;
    static constexpr uint8_t MSGPACK_MAP16_FORMAT       = 0xDEU;
    static constexpr uint8_t MSGPACK_MAP32_FORMAT       = 0xDFU;
    static constexpr uint8_t MSGPACK_UINT32_FORMAT      = 0xCEU;
    static constexpr uint8_t MSGPACK_UINT64_FORMAT      = 0xCFU;
    static constexpr size_t  MSGPACK_FIX_COLLECTION_MAX = 15U;
    static constexpr size_t  MSGPACK_COLLECTION16_MAX   = 65535U;
    static constexpr uint8_t MSGPACK_UINT128_ARRAY_SIZE = 2U;
    static constexpr std::streamsize SINGLE_BYTE        = 1;

    /**
     * @brief Writes an array or map header, choosing the smallest fitting format.
     *
     * @param out      Output stream.
     * @param size     Number of elements.
     * @param fix_base Base byte for the fix format (OR-ed with size for ≤15 elements).
     * @param format16 Format byte for 16-bit length encoding.
     * @param format32 Format byte for 32-bit length encoding.
     */
    static void write_collection_header(std::ofstream& out, size_t size,
                                        uint8_t fix_base, uint8_t format16, uint8_t format32);

    /**
     * @brief Writes a MessagePack array header for @p size elements.
     * @param out  Output stream.
     * @param size Number of array elements.
     */
    static void write_array_header(std::ofstream& out, size_t size);

    /**
     * @brief Writes a MessagePack map header for @p size key-value pairs.
     * @param out  Output stream.
     * @param size Number of map entries.
     */
    static void write_map_header(std::ofstream& out, size_t size);

    /**
     * @brief Writes a uint32 value in MessagePack uint32 format (0xCE + 4 bytes BE).
     * @param out   Output stream.
     * @param value Value to encode.
     */
    static void write_uint32_value(std::ofstream& out, uint32_t value);

    /**
     * @brief Writes a uint64 value in MessagePack uint64 format (0xCF + 8 bytes BE).
     * @param out   Output stream.
     * @param value Value to encode.
     */
    static void write_uint64_value(std::ofstream& out, uint64_t value);

    /**
     * @brief Writes a UInt128 as a MessagePack 2-element fixarray [high, low].
     * @param out   Output stream.
     * @param value Value to encode.
     */
    static void write_uint128_key(std::ofstream& out, const UInt128& value);

    /**
     * @brief Writes one graph's EnumerationResult as a MessagePack map.
     * @param out    Output stream.
     * @param result The per-graph frequency map to serialize.
     */
    static void write_graph_result(std::ofstream& out, const EnumerationResult& result);
};

}  // namespace sgf
