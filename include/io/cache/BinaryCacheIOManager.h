#pragma once

#include "ICacheIOManager.h"
#include "Int128.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>

namespace sgf
{

/**
 * @class BinaryCacheIOManager
 * @brief Reads and writes enumeration frequency data to/from a MessagePack binary cache file.
 *
 * Serializes the data as a MessagePack array of maps. Each outer element
 * corresponds to one library graph; each map entry encodes one motif's
 * frequency. Keys are UInt128 values serialized as a 2-element fixarray
 * [high:uint64, low:uint64]; values are uint32.
 */
class BinaryCacheIOManager : public ICacheIOManager
{
public:
    /**
     * @brief Constructs a BinaryCacheIOManager.
     *
     * @param folder        Directory where the binary file will be written.
     * @param base_filename File name without extension.
     */
    BinaryCacheIOManager(std::string folder, std::string base_filename);

protected:
    /**
     * @brief Writes enumeration data to a MessagePack file at @p full_path.
     *
     * @param data      The enumeration data to serialize.
     * @param full_path Destination file path including the .bin extension.
     * @throws SgfPathExistsException if the file cannot be opened or written.
     */
    void write_to_file(const EnumerationData& data, const std::string& full_path) const override;

    /**
     * @brief Reads enumeration data from a MessagePack file at @p full_path.
     *
     * @param full_path Source file path including the .bin extension.
     * @return The deserialized enumeration data.
     * @throws SgfPathExistsException if the file cannot be opened for reading.
     * @throws GraphConstructionException if the file contains corrupt or malformed data.
     */
    EnumerationData read_from_file(const std::string& full_path) const override;

    /**
     * @brief Returns the binary file extension.
     * @return "bin"
     */
    [[nodiscard]] std::string get_extension() const override;

private:
    static constexpr uint8_t MSGPACK_FIXARRAY_BASE = 0x90U;
    static constexpr uint8_t MSGPACK_FIXMAP_BASE = 0x80U;
    static constexpr uint8_t MSGPACK_ARRAY16_FORMAT = 0xDCU;
    static constexpr uint8_t MSGPACK_ARRAY32_FORMAT = 0xDDU;
    static constexpr uint8_t MSGPACK_MAP16_FORMAT = 0xDEU;
    static constexpr uint8_t MSGPACK_MAP32_FORMAT = 0xDFU;
    static constexpr uint8_t MSGPACK_UINT32_FORMAT = 0xCEU;
    static constexpr uint8_t MSGPACK_UINT64_FORMAT = 0xCFU;
    static constexpr size_t MSGPACK_FIX_COLLECTION_MAX = 15U;
    static constexpr size_t MSGPACK_COLLECTION16_MAX = 65535U;
    static constexpr uint8_t MSGPACK_UINT128_ARRAY_SIZE = 2U;
    static constexpr uint8_t MSGPACK_FIX_COLLECTION_MASK = 0x0FU;
    static constexpr uint8_t MSGPACK_UINT128_HEADER =
        static_cast<uint8_t>(MSGPACK_FIXARRAY_BASE | MSGPACK_UINT128_ARRAY_SIZE);
    static constexpr std::streamsize SINGLE_BYTE = 1;

    static constexpr uint8_t BYTE_MASK = 0xFFU;
    static constexpr uint32_t SHIFT_8 = 8U;
    static constexpr uint32_t SHIFT_16 = 16U;
    static constexpr uint32_t SHIFT_24 = 24U;
    static constexpr uint32_t SHIFT_32 = 32U;
    static constexpr uint32_t SHIFT_40 = 40U;
    static constexpr uint32_t SHIFT_48 = 48U;
    static constexpr uint32_t SHIFT_56 = 56U;

    static constexpr size_t UINT32_MSGPACK_BYTE_COUNT = 5U;
    static constexpr size_t UINT64_MSGPACK_BYTE_COUNT = 9U;
    static constexpr size_t UINT16_MSGPACK_BYTE_COUNT = 3U;
    static constexpr size_t RAW_UINT32_BYTE_COUNT = 4U;
    static constexpr size_t RAW_UINT16_BYTE_COUNT = 2U;
    static constexpr size_t FORMAT_BYTE_IDX = 0U;

    static constexpr size_t UINT32_BYTE_IDX_1 = 1U;
    static constexpr size_t UINT32_BYTE_IDX_2 = 2U;
    static constexpr size_t UINT32_BYTE_IDX_3 = 3U;
    static constexpr size_t UINT32_BYTE_IDX_4 = 4U;

    static constexpr size_t UINT64_BYTE_IDX_1 = 1U;
    static constexpr size_t UINT64_BYTE_IDX_2 = 2U;
    static constexpr size_t UINT64_BYTE_IDX_3 = 3U;
    static constexpr size_t UINT64_BYTE_IDX_4 = 4U;
    static constexpr size_t UINT64_BYTE_IDX_5 = 5U;
    static constexpr size_t UINT64_BYTE_IDX_6 = 6U;
    static constexpr size_t UINT64_BYTE_IDX_7 = 7U;
    static constexpr size_t UINT64_BYTE_IDX_8 = 8U;

    static constexpr size_t MAX_GRAPH_COUNT = 10'000'000U;

    // ── Write helpers ────────────────────────────────────────────────────────

    /**
     * @brief Writes an array or map header, choosing the smallest fitting format.
     *
     * @param output_stream Output stream.
     * @param size          Number of elements.
     * @param fix_base      Base byte for the fix format (OR-ed with size for ≤15 elements).
     * @param format16      Format byte for 16-bit length encoding.
     * @param format32      Format byte for 32-bit length encoding.
     */
    static void write_collection_header(std::ofstream& output_stream, size_t size, uint8_t fix_base,
                                        uint8_t format16, uint8_t format32);

    /**
     * @brief Writes a MessagePack array header for @p size elements.
     * @param output_stream Output stream.
     * @param size          Number of array elements.
     */
    static void write_array_header(std::ofstream& output_stream, size_t size);

    /**
     * @brief Writes a MessagePack map header for @p size key-value pairs.
     * @param output_stream Output stream.
     * @param size          Number of map entries.
     */
    static void write_map_header(std::ofstream& output_stream, size_t size);

    /**
     * @brief Writes a uint32 value in MessagePack uint32 format (0xCE + 4 bytes BE).
     * @param output_stream Output stream.
     * @param value         Value to encode.
     */
    static void write_uint32_value(std::ofstream& output_stream, uint32_t value);

    /**
     * @brief Writes a uint64 value in MessagePack uint64 format (0xCF + 8 bytes BE).
     * @param output_stream Output stream.
     * @param value         Value to encode.
     */
    static void write_uint64_value(std::ofstream& output_stream, uint64_t value);

    /**
     * @brief Writes a UInt128 as a MessagePack 2-element fixarray [high, low].
     * @param output_stream Output stream.
     * @param value         Value to encode.
     */
    static void write_uint128_key(std::ofstream& output_stream, const UInt128& value);

    /**
     * @brief Writes one graph's EnumerationResult as a MessagePack map.
     * @param output_stream Output stream.
     * @param result        The per-graph frequency map to serialize.
     */
    static void write_graph_result(std::ofstream& output_stream, const EnumerationResult& result);

    // ── Read helpers ─────────────────────────────────────────────────────────

    /**
     * @brief Throws GraphConstructionException if the stream is in a failed state.
     * @param input_stream Stream to check after a read operation.
     * @throws GraphConstructionException on EOF or I/O error.
     */
    static void check_read_stream(const std::ifstream& input_stream);

    /**
     * @brief Reads a single byte from the stream.
     * @param input_stream Input stream.
     * @return The byte read, as uint8_t.
     * @throws GraphConstructionException on EOF or I/O error.
     */
    static uint8_t read_byte(std::ifstream& input_stream);

    /**
     * @brief Reads 4 bytes big-endian and returns the value as a size_t.
     * @param input_stream Input stream.
     * @return Decoded 32-bit big-endian value.
     * @throws GraphConstructionException on EOF or I/O error.
     */
    static size_t read_be_uint32_size(std::ifstream& input_stream);

    /**
     * @brief Reads 2 bytes big-endian and returns the value as a size_t.
     * @param input_stream Input stream.
     * @return Decoded 16-bit big-endian value.
     * @throws GraphConstructionException on EOF or I/O error.
     */
    static size_t read_be_uint16_size(std::ifstream& input_stream);

    /**
     * @brief Reads an array or map header and returns the element count.
     *
     * @param input_stream Input stream.
     * @param format16     Format byte for 16-bit length encoding.
     * @param format32     Format byte for 32-bit length encoding.
     * @return Number of elements in the collection.
     * @throws GraphConstructionException on EOF, I/O error, or corrupt data.
     */
    static size_t read_collection_header(std::ifstream& input_stream, uint8_t format16,
                                         uint8_t format32);

    /**
     * @brief Reads a MessagePack array header and returns the element count.
     * @param input_stream Input stream.
     * @return Number of array elements.
     * @throws GraphConstructionException on EOF, I/O error, or corrupt data.
     */
    static size_t read_array_header(std::ifstream& input_stream);

    /**
     * @brief Reads a MessagePack map header and returns the entry count.
     * @param input_stream Input stream.
     * @return Number of map entries.
     * @throws GraphConstructionException on EOF, I/O error, or corrupt data.
     */
    static size_t read_map_header(std::ifstream& input_stream);

    /**
     * @brief Reads a MessagePack uint32 value (0xCE + 4 bytes BE).
     * @param input_stream Input stream.
     * @return Decoded uint32_t value.
     * @throws GraphConstructionException on EOF, I/O error, or unexpected format byte.
     */
    static uint32_t read_uint32_value(std::ifstream& input_stream);

    /**
     * @brief Reads a MessagePack uint64 value (0xCF + 8 bytes BE).
     * @param input_stream Input stream.
     * @return Decoded uint64_t value.
     * @throws GraphConstructionException on EOF, I/O error, or unexpected format byte.
     */
    static uint64_t read_uint64_value(std::ifstream& input_stream);

    /**
     * @brief Reads a UInt128 from a MessagePack 2-element fixarray [high, low].
     * @param input_stream Input stream.
     * @return Decoded UInt128 value.
     * @throws GraphConstructionException on EOF, I/O error, or unexpected header byte.
     */
    static UInt128 read_uint128_key(std::ifstream& input_stream);

    /**
     * @brief Reads one graph's EnumerationResult from a MessagePack map.
     * @param input_stream Input stream.
     * @return Deserialized per-graph frequency map.
     * @throws GraphConstructionException on EOF, I/O error, or corrupt data.
     */
    static EnumerationResult read_graph_result(std::ifstream& input_stream);

    /**
     * @brief Parses all graphs from an open binary stream.
     * @param input_stream Input stream positioned at the outer array header.
     * @return Deserialized enumeration data.
     * @throws GraphConstructionException on corrupt data or allocation failure.
     */
    static EnumerationData parse_binary(std::ifstream& input_stream);
};

}  // namespace sgf
